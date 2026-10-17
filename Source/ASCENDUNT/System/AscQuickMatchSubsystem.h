#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "AscQuickMatchSubsystem.generated.h"

class IHttpRequest;
class IHttpResponse;

typedef TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> FAscHttpRequestPtr;
typedef TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> FAscHttpResponsePtr;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAscOnQueueStateChanged, const FAscClientQueueState&, QueueState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAscOnQueueError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAscOnMatchServerAssigned, const FAscMatchServerConnectionInfo&, ConnectionInfo);

/**
 * Client-side bridge to the Python quick-match orchestrator.
 *
 * Responsibilities:
 * - Join / cancel the FIFO quick-match queue.
 * - Poll ticket state and expose queue count such as 4/6 to UI.
 * - Travel the local client to the assigned dedicated server when ready.
 *
 * This subsystem intentionally survives map travel, so the same queue ticket can
 * continue while the player is in MainMenu or a future Training Range map.
 * 
 * 주요 함수는 JoinQueue, CancelQueue, PollTicketStatus, TraveltoMatchServer
 * 나머지 함수는 HTTP 요청, JSON 파싱, URL 생성, 상태 Broadcast를 위한 헬퍼
 */
UCLASS(Config = Game)
class ASCENDUNT_API UAscQuickMatchSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /* 빠른 시작 참가 요청 */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Quick Match")
    void JoinQueue(EAscMatchMode Mode, const FString& DisplayName);

    /* 대기 취소 */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Quick Match")
    void CancelQueue();

    /** 4/6, 5/6, ready같은 상태 조회 */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Quick Match")
    void PollTicketStatus();

    /** Directly travels to the assigned match server. Called automatically by default when ticket becomes ready. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Quick Match")
    bool TravelToMatchServer();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Quick Match")
    void ClearQueueState();

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Quick Match")
    void SetOrchestratorApiUrl(const FString& InBaseUrl);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Quick Match")
    void SetLocalPlayerIdentity(const FString& InPlayerId, const FString& InDisplayName);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Quick Match")
    const FAscClientQueueState& GetQueueState() const { return QueueState; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Quick Match")
    bool IsInQueue() const { return QueueState.IsWaiting(); }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Quick Match")
    bool CanTravelToAssignedMatch() const { return QueueState.CanTravelToMatch(); }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Quick Match")
    FString GetLocalPlayerId() const { return LocalPlayerId; }

public:
    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Quick Match")
    FAscOnQueueStateChanged OnQueueStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Quick Match")
    FAscOnQueueError OnQueueError;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Quick Match")
    FAscOnMatchServerAssigned OnMatchServerAssigned;

protected:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Quick Match")
    FString OrchestratorApiUrl = TEXT("http://ASCENDUNT-SERVER.iptime.org:8000");

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Quick Match")
    FString BuildVersion = TEXT("0.1.0");

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Quick Match", meta = (ClampMin = "0.1"))
    float PollIntervalSeconds = 1.0f;

    /** If true, ticket ready immediately triggers ClientTravel. If false, UI can call TravelToMatchServer manually. */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Quick Match")
    bool bAutoTravelWhenReady = true;

private:
    UPROPERTY(Transient)
    FAscClientQueueState QueueState;

    UPROPERTY(Transient)
    FString LocalPlayerId;

    UPROPERTY(Transient)
    FString LocalDisplayName;

    FTimerHandle PollTimerHandle;
    /*이미 Join 요청 중인가?*/
    bool bJoinRequestInFlight = false;
    bool bCancelRequestInFlight = false;
    /*이미 Poll 요청 중인가?*/
    bool bPollRequestInFlight = false;

private:
    void StartTicketPolling();
    void StopTicketPolling();

    void BroadcastQueueState();
    void SetError(const FString& ErrorMessage, bool bMarkFailed);

    FString BuildOrchestratorEndpointUrl(const FString& RelativePath) const;
    FString BuildMatchServerTravelUrl() const;
    FString GetOrCreateLocalPlayerId();

    static FString MatchModeToApiKey(EAscMatchMode Mode);
    static EAscQueueTicketStatus QueueStatusFromString(const FString& StatusString);

    bool CreateJsonRequest(const FString& Verb, const FString& Url, const TSharedPtr<class FJsonObject>& JsonObject, FAscHttpRequestPtr& OutRequest) const;
    bool ParseJsonObject(FAscHttpResponsePtr Response, TSharedPtr<class FJsonObject>& OutJsonObject, FString& OutError) const;
    void ApplyQueueStateFromJson(const TSharedPtr<class FJsonObject>& JsonObject);

    void HandleJoinQueueResponse(FAscHttpRequestPtr Request, FAscHttpResponsePtr Response, bool bWasSuccessful);
    void HandleCancelQueueResponse(FAscHttpRequestPtr Request, FAscHttpResponsePtr Response, bool bWasSuccessful);
    void HandlePollTicketResponse(FAscHttpRequestPtr Request, FAscHttpResponsePtr Response, bool bWasSuccessful);
};
