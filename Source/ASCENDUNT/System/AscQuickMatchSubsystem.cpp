#include "AscQuickMatchSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "TimerManager.h"

namespace AscQuickMatchJson
{
    static FString GetStringField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const FString& DefaultValue = FString())
    {
        if (!JsonObject.IsValid())
        {
            return DefaultValue;
        }

        FString Value;
        return JsonObject->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
    }

    static int32 GetIntField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, int32 DefaultValue = 0)
    {
        if (!JsonObject.IsValid())
        {
            return DefaultValue;
        }

        double Number = 0.0;
        return JsonObject->TryGetNumberField(FieldName, Number) ? FMath::TruncToInt(Number) : DefaultValue;
    }

    static TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
    {
        if (!JsonObject.IsValid())
        {
            return nullptr;
        }

        const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
        return JsonObject->TryGetObjectField(FieldName, ObjectPtr) ? *ObjectPtr : nullptr;
    }
}

void UAscQuickMatchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadConfig();
    GetOrCreateLocalPlayerId();
}

void UAscQuickMatchSubsystem::Deinitialize()
{
    StopTicketPolling();
    Super::Deinitialize();
}

void UAscQuickMatchSubsystem::JoinQueue(EAscMatchMode Mode, const FString& DisplayName)
{
    // 이미 join 요청 중인지
    if (bJoinRequestInFlight)
    {
        return;
    }

    // 이미 Queued/Launching 상태면 다시 참가 X
    if (QueueState.IsWaiting())
    {
        SetError(TEXT("Already waiting in quick-match queue."), false);
        return;
    }

    const FString PlayerId = GetOrCreateLocalPlayerId();
    LocalDisplayName = DisplayName.IsEmpty() ? PlayerId : DisplayName;

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("player_id"), PlayerId);
    Body->SetStringField(TEXT("display_name"), LocalDisplayName);
    Body->SetStringField(TEXT("mode_key"), MatchModeToApiKey(Mode));
    Body->SetStringField(TEXT("build_version"), BuildVersion);

    // 위에서 만든 JSON body로 POST /queue/join 요청을 만듦
    FAscHttpRequestPtr Request;
    if (!CreateJsonRequest(TEXT("POST"), BuildOrchestratorEndpointUrl(TEXT("/queue/join")), Body, Request))
    {
        SetError(TEXT("Failed to create join queue request."), true);
        return;
    }

    // QueueState Join으로 만들고 Broadcast(UI에 알림)
    QueueState = FAscClientQueueState();
    QueueState.Status = EAscQueueTicketStatus::Queued;
    QueueState.Mode = Mode;
    QueueState.PlayerId = PlayerId;
    QueueState.DisplayName = LocalDisplayName;
    QueueState.ErrorMessage.Reset();
    BroadcastQueueState();

    bJoinRequestInFlight = true;

    // 응답이 오면 HandleJoinQueueResponse 실행됨.
    Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::HandleJoinQueueResponse);
    Request->ProcessRequest();
}

void UAscQuickMatchSubsystem::CancelQueue()
{
    if (QueueState.TicketId.IsEmpty())
    {
        ClearQueueState();
        return;
    }

    if (bCancelRequestInFlight)
    {
        return;
    }

    StopTicketPolling();

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("player_id"), QueueState.PlayerId);
    Body->SetStringField(TEXT("ticket_id"), QueueState.TicketId);

    FAscHttpRequestPtr Request;
    const FString Url = BuildOrchestratorEndpointUrl(FString::Printf(TEXT("/tickets/%s/cancel"), *QueueState.TicketId));
    if (!CreateJsonRequest(TEXT("POST"), Url, Body, Request))
    {
        SetError(TEXT("Failed to create cancel queue request."), false);
        QueueState.Status = EAscQueueTicketStatus::Cancelled;
        BroadcastQueueState();
        return;
    }

    bCancelRequestInFlight = true;
    Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::HandleCancelQueueResponse);
    Request->ProcessRequest();
}

void UAscQuickMatchSubsystem::PollTicketStatus()
{
    if (QueueState.TicketId.IsEmpty() || bPollRequestInFlight)
    {
        return;
    }

    FAscHttpRequestPtr Request;
    const FString Url = BuildOrchestratorEndpointUrl(FString::Printf(TEXT("/tickets/%s"), *QueueState.TicketId));
    
    // Get /tickets/{ticket_id} 요청을 보냄.
    if (!CreateJsonRequest(TEXT("GET"), Url, nullptr, Request))
    {
        SetError(TEXT("Failed to create ticket polling request."), false);
        return;
    }

    bPollRequestInFlight = true;

    // HandlePollTicketResponse에서 StartTicketPolling을 다시 호출하거나 Travel하거나 Stop함.
    Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::HandlePollTicketResponse);
    Request->ProcessRequest();
}

bool UAscQuickMatchSubsystem::TravelToMatchServer()
{
    if (!QueueState.CanTravelToMatch())
    {
        SetError(TEXT("Match server is not assigned yet."), false);
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
    if (!PC)
    {
        SetError(TEXT("Cannot travel because local PlayerController is missing."), false);
        return false;
    }

    // TicketId, PlayerId, DisplayName 을 URL option 으로 같이 넘기는 이유는
    // MatchGameMode::PreLogin / InitNewPlayer / PostLogin 에서 플레이어를 식별하기 위함
    const FString TravelUrl = BuildMatchServerTravelUrl();
    if (TravelUrl.IsEmpty())
    {
        SetError(TEXT("Cannot travel because match server address is empty."), false);
        return false;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[AscQuickMatch] ClientTravel URL=%s MatchId=%s ConnectAddress=%s Ip=%s Port=%d TicketId=%s PlayerId=%s DisplayName=%s"),
        *TravelUrl,
        *QueueState.Connection.MatchId,
        *QueueState.Connection.MatchServerTravelAddress,
        *QueueState.Connection.Ip,
        QueueState.Connection.Port,
        *QueueState.TicketId,
        *QueueState.PlayerId,
        *QueueState.DisplayName);

    StopTicketPolling();
    PC->ClientTravel(TravelUrl, TRAVEL_Absolute);
    return true;
}

void UAscQuickMatchSubsystem::ClearQueueState()
{
    StopTicketPolling();

    const FString PreservePlayerId = QueueState.PlayerId.IsEmpty() ? LocalPlayerId : QueueState.PlayerId;
    const FString PreserveDisplayName = QueueState.DisplayName.IsEmpty() ? LocalDisplayName : QueueState.DisplayName;

    QueueState = FAscClientQueueState();
    QueueState.PlayerId = PreservePlayerId;
    QueueState.DisplayName = PreserveDisplayName;

    BroadcastQueueState();
}

void UAscQuickMatchSubsystem::SetOrchestratorApiUrl(const FString& InBaseUrl)
{
    OrchestratorApiUrl = InBaseUrl;
    OrchestratorApiUrl.TrimStartAndEndInline();
    OrchestratorApiUrl.RemoveFromEnd(TEXT("/"));
}

void UAscQuickMatchSubsystem::SetLocalPlayerIdentity(const FString& InPlayerId, const FString& InDisplayName)
{
    if (!InPlayerId.IsEmpty())
    {
        LocalPlayerId = InPlayerId;
    }

    if (!InDisplayName.IsEmpty())
    {
        LocalDisplayName = InDisplayName;
    }

    QueueState.PlayerId = LocalPlayerId;
    QueueState.DisplayName = LocalDisplayName;
    BroadcastQueueState();
}

void UAscQuickMatchSubsystem::StartTicketPolling()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(PollTimerHandle);
    World->GetTimerManager().SetTimer(
        PollTimerHandle,
        this,
        &ThisClass::PollTicketStatus,
        FMath::Max(0.1f, PollIntervalSeconds),
        true);
}

void UAscQuickMatchSubsystem::StopTicketPolling()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PollTimerHandle);
    }

    bPollRequestInFlight = false;
}

void UAscQuickMatchSubsystem::BroadcastQueueState()
{
    OnQueueStateChanged.Broadcast(QueueState);
}

void UAscQuickMatchSubsystem::SetError(const FString& ErrorMessage, bool bMarkFailed)
{
    QueueState.ErrorMessage = ErrorMessage;

    if (bMarkFailed)
    {
        QueueState.Status = EAscQueueTicketStatus::Failed;
        StopTicketPolling();
    }

    UE_LOG(LogTemp, Warning, TEXT("[AscQuickMatch] %s"), *ErrorMessage);
    OnQueueError.Broadcast(ErrorMessage);
    BroadcastQueueState();
}

FString UAscQuickMatchSubsystem::BuildOrchestratorEndpointUrl(const FString& RelativePath) const
{
    FString BaseUrl = OrchestratorApiUrl;
    BaseUrl.TrimStartAndEndInline();
    BaseUrl.RemoveFromEnd(TEXT("/"));

    if (RelativePath.StartsWith(TEXT("/")))
    {
        return BaseUrl + RelativePath;
    }

    return BaseUrl + TEXT("/") + RelativePath;
}

FString UAscQuickMatchSubsystem::BuildMatchServerTravelUrl() const
{
    FString Address = QueueState.Connection.MatchServerTravelAddress;
    if (Address.IsEmpty() && !QueueState.Connection.Ip.IsEmpty() && QueueState.Connection.Port > 0)
    {
        Address = FString::Printf(TEXT("%s:%d"), *QueueState.Connection.Ip, QueueState.Connection.Port);
    }

    if (Address.IsEmpty())
    {
        return FString();
    }

    const FString EncodedTicketId = FGenericPlatformHttp::UrlEncode(QueueState.TicketId);
    const FString EncodedPlayerId = FGenericPlatformHttp::UrlEncode(QueueState.PlayerId);
    const FString EncodedDisplayName = FGenericPlatformHttp::UrlEncode(QueueState.DisplayName);

    return FString::Printf(
        TEXT("%s?TicketId=%s?PlayerId=%s?DisplayName=%s"),
        *Address,
        *EncodedTicketId,
        *EncodedPlayerId,
        *EncodedDisplayName);
}

FString UAscQuickMatchSubsystem::GetOrCreateLocalPlayerId()
{
    if (LocalPlayerId.IsEmpty())
    {
        LocalPlayerId = FString::Printf(TEXT("player_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }

    return LocalPlayerId;
}

FString UAscQuickMatchSubsystem::MatchModeToApiKey(EAscMatchMode Mode)
{
    switch (Mode)
    {
    case EAscMatchMode::Duel1v1:
        return TEXT("DUEL_1V1");
    case EAscMatchMode::TDM2v2:
        return TEXT("TDM_2V2");
    case EAscMatchMode::Main3v3:
        return TEXT("MAIN_3V3");
    case EAscMatchMode::Practice:
        return TEXT("PRACTICE");
    case EAscMatchMode::FreeForAll:
        return TEXT("FFA");
    default:
        return TEXT("MAIN_3V3");
    }
}

EAscQueueTicketStatus UAscQuickMatchSubsystem::QueueStatusFromString(const FString& StatusString)
{
    const FString Normalized = StatusString.ToLower();

    if (Normalized == TEXT("queued") || Normalized == TEXT("waiting"))
    {
        return EAscQueueTicketStatus::Queued;
    }
    if (Normalized == TEXT("launching") || Normalized == TEXT("starting"))
    {
        return EAscQueueTicketStatus::Launching;
    }
    if (Normalized == TEXT("ready"))
    {
        return EAscQueueTicketStatus::Ready;
    }
    if (Normalized == TEXT("cancelled") || Normalized == TEXT("canceled"))
    {
        return EAscQueueTicketStatus::Cancelled;
    }
    if (Normalized == TEXT("expired"))
    {
        return EAscQueueTicketStatus::Expired;
    }
    if (Normalized == TEXT("failed") || Normalized == TEXT("error"))
    {
        return EAscQueueTicketStatus::Failed;
    }

    return EAscQueueTicketStatus::None;
}

bool UAscQuickMatchSubsystem::CreateJsonRequest(const FString& Verb, const FString& Url, const TSharedPtr<FJsonObject>& JsonObject, FAscHttpRequestPtr& OutRequest) const
{
    if (Url.IsEmpty())
    {
        return false;
    }

    OutRequest = FHttpModule::Get().CreateRequest();
    OutRequest->SetURL(Url);
    OutRequest->SetVerb(Verb);
    OutRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));

    if (JsonObject.IsValid())
    {
        FString Body;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
        FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

        OutRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        OutRequest->SetContentAsString(Body);
    }

    return true;
}

bool UAscQuickMatchSubsystem::ParseJsonObject(FAscHttpResponsePtr Response, TSharedPtr<FJsonObject>& OutJsonObject, FString& OutError) const
{
    if (!Response.IsValid())
    {
        OutError = TEXT("No HTTP response.");
        return false;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString Content = Response->GetContentAsString();

    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        OutError = FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *Content);
        return false;
    }

    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, OutJsonObject) || !OutJsonObject.IsValid())
    {
        OutError = FString::Printf(TEXT("Invalid JSON response: %s"), *Content);
        return false;
    }

    return true;
}

void UAscQuickMatchSubsystem::ApplyQueueStateFromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
    if (!JsonObject.IsValid())
    {
        return;
    }

    const TSharedPtr<FJsonObject> QueueObject = AscQuickMatchJson::GetObjectField(JsonObject, TEXT("queue"));
    const TSharedPtr<FJsonObject> ConnectionObject = AscQuickMatchJson::GetObjectField(JsonObject, TEXT("connection"));

    const FString TicketId = AscQuickMatchJson::GetStringField(JsonObject, TEXT("ticket_id"), QueueState.TicketId);
    const FString StatusString = AscQuickMatchJson::GetStringField(JsonObject, TEXT("status"));
    const FString MatchId = AscQuickMatchJson::GetStringField(JsonObject, TEXT("match_id"), QueueState.Connection.MatchId);

    if (!TicketId.IsEmpty())
    {
        QueueState.TicketId = TicketId;
    }

    const EAscQueueTicketStatus ParsedStatus = QueueStatusFromString(StatusString);
    if (ParsedStatus != EAscQueueTicketStatus::None)
    {
        QueueState.Status = ParsedStatus;
    }

    QueueState.CurrentPlayers = AscQuickMatchJson::GetIntField(JsonObject, TEXT("current_players"), QueueState.CurrentPlayers);
    QueueState.RequiredPlayers = AscQuickMatchJson::GetIntField(JsonObject, TEXT("required_players"), QueueState.RequiredPlayers);
    QueueState.QueuePosition = AscQuickMatchJson::GetIntField(JsonObject, TEXT("queue_position"), QueueState.QueuePosition);
    QueueState.ErrorMessage = AscQuickMatchJson::GetStringField(JsonObject, TEXT("error_message"), QueueState.ErrorMessage);

    if (QueueObject.IsValid())
    {
        QueueState.CurrentPlayers = AscQuickMatchJson::GetIntField(QueueObject, TEXT("current_players"), QueueState.CurrentPlayers);
        QueueState.RequiredPlayers = AscQuickMatchJson::GetIntField(QueueObject, TEXT("required_players"), QueueState.RequiredPlayers);
        QueueState.QueuePosition = AscQuickMatchJson::GetIntField(QueueObject, TEXT("queue_position"), QueueState.QueuePosition);
    }

    QueueState.Connection.MatchId = MatchId;
    QueueState.Connection.MatchServerTravelAddress = AscQuickMatchJson::GetStringField(JsonObject, TEXT("connect_address"), QueueState.Connection.MatchServerTravelAddress);
    QueueState.Connection.Ip = AscQuickMatchJson::GetStringField(JsonObject, TEXT("ip"), QueueState.Connection.Ip);
    QueueState.Connection.Port = AscQuickMatchJson::GetIntField(JsonObject, TEXT("port"), QueueState.Connection.Port);
    QueueState.Connection.SelectedMapPath = AscQuickMatchJson::GetStringField(JsonObject, TEXT("selected_map_path"), QueueState.Connection.SelectedMapPath);
    QueueState.Connection.SelectedMapDisplayName = AscQuickMatchJson::GetStringField(JsonObject, TEXT("selected_map_display_name"), QueueState.Connection.SelectedMapDisplayName);

    if (ConnectionObject.IsValid())
    {
        QueueState.Connection.MatchId = AscQuickMatchJson::GetStringField(ConnectionObject, TEXT("match_id"), QueueState.Connection.MatchId);
        QueueState.Connection.MatchServerTravelAddress = AscQuickMatchJson::GetStringField(ConnectionObject, TEXT("connect_address"), QueueState.Connection.MatchServerTravelAddress);
        QueueState.Connection.Ip = AscQuickMatchJson::GetStringField(ConnectionObject, TEXT("ip"), QueueState.Connection.Ip);
        QueueState.Connection.Port = AscQuickMatchJson::GetIntField(ConnectionObject, TEXT("port"), QueueState.Connection.Port);
        QueueState.Connection.SelectedMapPath = AscQuickMatchJson::GetStringField(ConnectionObject, TEXT("selected_map_path"), QueueState.Connection.SelectedMapPath);
        QueueState.Connection.SelectedMapDisplayName = AscQuickMatchJson::GetStringField(ConnectionObject, TEXT("selected_map_display_name"), QueueState.Connection.SelectedMapDisplayName);
    }

    if (QueueState.Connection.MatchServerTravelAddress.IsEmpty() && !QueueState.Connection.Ip.IsEmpty() && QueueState.Connection.Port > 0)
    {
        QueueState.Connection.MatchServerTravelAddress = FString::Printf(TEXT("%s:%d"), *QueueState.Connection.Ip, QueueState.Connection.Port);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[AscQuickMatch] TicketState Status=%d TicketId=%s MatchId=%s ConnectAddress=%s Ip=%s Port=%d Current=%d Required=%d Error=%s"),
        static_cast<int32>(QueueState.Status),
        *QueueState.TicketId,
        *QueueState.Connection.MatchId,
        *QueueState.Connection.MatchServerTravelAddress,
        *QueueState.Connection.Ip,
        QueueState.Connection.Port,
        QueueState.CurrentPlayers,
        QueueState.RequiredPlayers,
        *QueueState.ErrorMessage);
}

void UAscQuickMatchSubsystem::HandleJoinQueueResponse(FAscHttpRequestPtr Request, FAscHttpResponsePtr Response, bool bWasSuccessful)
{
    bJoinRequestInFlight = false;

    if (!bWasSuccessful)
    {
        SetError(TEXT("Join queue request failed."), true);
        return;
    }

    // 응답을 JSON으로 파싱
    TSharedPtr<FJsonObject> JsonObject;
    FString Error;
    if (!ParseJsonObject(Response, JsonObject, Error))
    {
        SetError(Error, true);
        return;
    }

    ApplyQueueStateFromJson(JsonObject);
    BroadcastQueueState();

    if (QueueState.Status == EAscQueueTicketStatus::Ready)
    {
        OnMatchServerAssigned.Broadcast(QueueState.Connection);
        if (bAutoTravelWhenReady)
        {
            TravelToMatchServer();
        }
        return;
    }

    // 여기서부터 Polling 시작
    StartTicketPolling();
}

void UAscQuickMatchSubsystem::HandleCancelQueueResponse(FAscHttpRequestPtr Request, FAscHttpResponsePtr Response, bool bWasSuccessful)
{
    bCancelRequestInFlight = false;

    TSharedPtr<FJsonObject> JsonObject;
    FString Error;
    if (bWasSuccessful && ParseJsonObject(Response, JsonObject, Error))
    {
        ApplyQueueStateFromJson(JsonObject);
    }

    QueueState.Status = EAscQueueTicketStatus::Cancelled;
    BroadcastQueueState();
}

void UAscQuickMatchSubsystem::HandlePollTicketResponse(FAscHttpRequestPtr Request, FAscHttpResponsePtr Response, bool bWasSuccessful)
{
    bPollRequestInFlight = false;

    if (!bWasSuccessful)
    {
        SetError(TEXT("Ticket polling request failed."), false);
        return;
    }

    // 응답을 JSON으로 파싱
    TSharedPtr<FJsonObject> JsonObject;
    FString Error;
    if (!ParseJsonObject(Response, JsonObject, Error))
    {
        SetError(Error, false);
        return;
    }

    ApplyQueueStateFromJson(JsonObject);
    BroadcastQueueState();

    switch (QueueState.Status)
    {
    case EAscQueueTicketStatus::Queued:
    case EAscQueueTicketStatus::Launching:
        StartTicketPolling();
        break;

    case EAscQueueTicketStatus::Ready:
        StopTicketPolling();
        OnMatchServerAssigned.Broadcast(QueueState.Connection);
        if (bAutoTravelWhenReady)
        {
            TravelToMatchServer();
        }
        break;

    case EAscQueueTicketStatus::Cancelled:
    case EAscQueueTicketStatus::Failed:
    case EAscQueueTicketStatus::Expired:
        StopTicketPolling();
        break;

    default:
        break;
    }
}
