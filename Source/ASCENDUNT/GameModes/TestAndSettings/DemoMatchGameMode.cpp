#include "DemoMatchGameMode.h"

#include "ASCENDUNT/GameModes/PVP/MatchGameState.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Kismet/GameplayStatics.h"

ADemoMatchGameMode::ADemoMatchGameMode()
{
    PlayerControllerClass = AAscenduntPlayerController::StaticClass();
    PlayerStateClass = AAscenduntPlayerState::StaticClass();
    GameStateClass = AMatchGameState::StaticClass();

    // Pawn class is selected from PlayerState->PawnData in GetDefaultPawnClassForController.
    DefaultPawnClass = nullptr;

    // Direct-connect demo flow must not depend on lobby seamless travel.
    bUseSeamlessTravel = false;
    bDelayedStart = false;

    // Prevent the engine from spawning a default pawn before we assign PawnData.
    // PostLogin assigns PawnData first, then calls RestartPlayer manually.
    bStartPlayersAsSpectators = true;
}

void ADemoMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    PendingMatchSettings.SelectedMapPath = MapName;
    PendingMatchSettings.SelectedMapDisplayName = MapName;

    const FString EncodedMatchName = UGameplayStatics::ParseOption(Options, TEXT("AscMatchName"));
    const FString GameModeString = UGameplayStatics::ParseOption(Options, TEXT("AscMatchMode"));
    const FString MaxPlayersString = UGameplayStatics::ParseOption(Options, TEXT("AscMaxPlayers"));
    const FString ScoreLimitString = UGameplayStatics::ParseOption(Options, TEXT("AscScoreLimit"));
    PendingMatchSettings.MatchName = EncodedMatchName.IsEmpty()
        ? TEXT("Demo")
        : FGenericPlatformHttp::UrlDecode(EncodedMatchName);

    PendingMatchSettings.Rules.Mode = GameModeString.IsEmpty()
        ? EAscMatchMode::TDM2v2
        : static_cast<EAscMatchMode>(FCString::Atoi(*GameModeString));

    PendingMatchSettings.Rules.MaxPlayers = MaxPlayersString.IsEmpty()
        ? FMath::Max(1, DefaultMaxPlayers)
        : FMath::Max(1, FCString::Atoi(*MaxPlayersString));

    PendingMatchSettings.Rules.RequiredPlayers = PendingMatchSettings.Rules.MaxPlayers;
    PendingMatchSettings.Rules.TeamSize = FMath::Max(1, PendingMatchSettings.Rules.MaxPlayers / 2);

    PendingMatchSettings.Rules.ScoreLimit = ScoreLimitString.IsEmpty()
        ? 30
        : FMath::Max(1, FCString::Atoi(*ScoreLimitString));

    UE_LOG(LogTemp, Warning,
        TEXT("[DemoMatchGM] InitGame Map=%s Room=%s Mode=%d MaxPlayers=%d ScoreLimit=%d"),
        *MapName,
        *PendingMatchSettings.MatchName,
        static_cast<int32>(PendingMatchSettings.Rules.Mode),
        PendingMatchSettings.Rules.MaxPlayers,
        PendingMatchSettings.Rules.ScoreLimit);
}

void ADemoMatchGameMode::InitGameState()
{
    Super::InitGameState();

    if (AMatchGameState* MatchGS = GetGameState<AMatchGameState>())
    {
        MatchGS->SetMatchSettings(PendingMatchSettings);
        MatchGS->SetTeamScore(1, 0);
        MatchGS->SetTeamScore(2, 0);

        FAscMatchClockState InitialClock;
        InitialClock.RemainingTimeSeconds = PendingMatchSettings.Rules.TimeLimitSeconds;
        MatchGS->SetClockState(InitialClock);
    }
}

void ADemoMatchGameMode::PreLogin(
    const FString& Options,
    const FString& Address,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    // Important: this class intentionally does NOT derive from AMatchGameMode,
    // because AMatchGameMode::PreLogin rejects direct joins with ROOM_NOT_JOINABLE.
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[DemoMatchGM] PreLogin rejected by Super: %s"), *ErrorMessage);
        return;
    }

    const int32 MaxPlayers = GetResolvedMaxPlayers();
    const int32 CurrentPlayers = GameState ? GameState->PlayerArray.Num() : GetNumPlayers();

    if (CurrentPlayers >= MaxPlayers)
    {
        ErrorMessage = TEXT("DEMO_ROOM_FULL");
        UE_LOG(LogTemp, Warning,
            TEXT("[DemoMatchGM] PreLogin rejected: DEMO_ROOM_FULL Current=%d Max=%d Address=%s"),
            CurrentPlayers,
            MaxPlayers,
            *Address);
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[DemoMatchGM] PreLogin accepted Address=%s Options=%s"),
        *Address,
        *Options);
}

FString ADemoMatchGameMode::InitNewPlayer(
    APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId,
    const FString& Options,
    const FString& Portal)
{
    const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

    if (NewPlayerController)
    {
        PendingPlayerOptions.Add(NewPlayerController, Options);
        UE_LOG(LogTemp, Warning,
            TEXT("[DemoMatchGM] InitNewPlayer PC=%s Options=%s"),
            *GetNameSafe(NewPlayerController),
            *Options);
    }

    return Error;
}

void ADemoMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    FString Options;
    if (const FString* FoundOptions = PendingPlayerOptions.Find(NewPlayer))
    {
        Options = *FoundOptions;
    }

    AssignDemoPawnData(NewPlayer, Options);

    // Spawn after PawnData is selected so GetDefaultPawnClassForController can return the hero pawn.
    // HeroInitComponent owns ASC ActorInfo, PawnData AbilitySet application and GameplayReady.
    RestartPlayer(NewPlayer);
}

void ADemoMatchGameMode::Logout(AController* Exiting)
{
    if (APlayerController* PC = Cast<APlayerController>(Exiting))
    {
        PendingPlayerOptions.Remove(PC);
    }

    Super::Logout(Exiting);
}

UClass* ADemoMatchGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (const AAscenduntPlayerState* AscPS = InController ? InController->GetPlayerState<AAscenduntPlayerState>() : nullptr)
    {
        if (const UHeroPawnData* PawnData = AscPS->GetPawnData())
        {
            if (PawnData->PawnClass)
            {
                return PawnData->PawnClass;
            }
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[DemoMatchGM] GetDefaultPawnClass fallback. Controller=%s"),
        *GetNameSafe(InController));

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

const UHeroPawnData* ADemoMatchGameMode::ResolvePawnDataFromOptions(const FString& Options)
{
    const FString HeroString = UGameplayStatics::ParseOption(Options, TEXT("Hero"));

    if (HeroString.Equals(TEXT("Astell"), ESearchCase::IgnoreCase))
    {
        return AstellPawnData;
    }

    if (HeroString.Equals(TEXT("Hakuko"), ESearchCase::IgnoreCase))
    {
        return HakukoPawnData;
    }

    // No ?Hero option: auto-assign by join order.
    const UHeroPawnData* Result = (NextAutoAssignIndex == 0) ? AstellPawnData.Get() : HakukoPawnData.Get();
    ++NextAutoAssignIndex;
    return Result;
}

void ADemoMatchGameMode::AssignDemoPawnData(APlayerController* PC, const FString& Options)
{
    AAscenduntPlayerState* AscPS = PC ? PC->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    if (!AscPS)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[DemoMatchGM] AssignDemoPawnData failed: invalid PlayerState. PC=%s"),
            *GetNameSafe(PC));
        return;
    }

    if (AscPS->GetPawnData())
    {
        return;
    }

    const FString HeroString = UGameplayStatics::ParseOption(Options, TEXT("Hero"));
    const UHeroPawnData* SelectedPawnData = ResolvePawnDataFromOptions(Options);

    if (!SelectedPawnData)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[DemoMatchGM] PawnData is null. Hero=%s AstellPawnData=%s HakukoPawnData=%s"),
            *HeroString,
            *GetNameSafe(AstellPawnData),
            *GetNameSafe(HakukoPawnData));
        return;
    }

    AscPS->SetPawnData(SelectedPawnData);

    if (SelectedPawnData == AstellPawnData)
    {
        AscPS->SetTeamId(AstellTeamId);
        AscPS->SetDisplayName(TEXT("Astell"));
    }
    else if (SelectedPawnData == HakukoPawnData)
    {
        AscPS->SetTeamId(HakukoTeamId);
        AscPS->SetDisplayName(TEXT("Hakuko"));
    }
    else
    {
        AscPS->SetTeamId(1);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[DemoMatchGM] Assigned Player=%s HeroOption=%s PawnData=%s Team=%d"),
        *GetNameSafe(PC),
        HeroString.IsEmpty() ? TEXT("<Auto>") : *HeroString,
        *GetNameSafe(SelectedPawnData),
        AscPS->GetTeamId());
}

int32 ADemoMatchGameMode::GetResolvedMaxPlayers() const
{
    if (PendingMatchSettings.Rules.MaxPlayers > 0)
    {
        return PendingMatchSettings.Rules.MaxPlayers;
    }

    return FMath::Max(1, DefaultMaxPlayers);
}
