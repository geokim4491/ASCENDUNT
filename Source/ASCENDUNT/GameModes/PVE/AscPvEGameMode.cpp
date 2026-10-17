#include "AscPvEGameMode.h"

#include "ASCENDUNT/GameModes/PVE/AscPvEGameState.h"
#include "ASCENDUNT/Mission/AscMissionDirector.h"
#include "ASCENDUNT/HeroSelection/AscHeroSelectionComponent.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAscPvEGameMode::AAscPvEGameMode()
{
    HeroSelectionComponent = CreateDefaultSubobject<UAscHeroSelectionComponent>(TEXT("HeroSelectionComponent"));

    GameStateClass = AAscPvEGameState::StaticClass();
    PlayerControllerClass = AAscenduntPlayerController::StaticClass();
    PlayerStateClass = AAscenduntPlayerState::StaticClass();

    // Players stay without a combat Pawn until every squad member confirms a hero.
    DefaultPawnClass = nullptr;

    // HeroSelection waits with a normal PlayerController and no Pawn.
    // Spectator state is not used here; keeping players as spectators can interfere
    // with the later standard RestartPlayer() possession path.
    bStartPlayersAsSpectators = false;
    bDelayedStart = true;
}

void AAscPvEGameMode::BeginPlay()
{
    Super::BeginPlay();

    BindMissionDirector();
}

void AAscPvEGameMode::InitGame(
    const FString& MapName,
    const FString& Options,
    FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    UE_LOG(LogTemp, Log,
        TEXT("[PvEGM] InitGame Map=%s Options=%s RequiredPlayers=%d MaxPlayers=%d"),
        *MapName,
        *Options,
        RequiredPlayers,
        MaxPlayers);
}

void AAscPvEGameMode::InitGameState()
{
    Super::InitGameState();

    if (AAscPvEGameState* PvEGameState = GetPvEGameState())
    {
        PvEGameState->SetGamePhase(EAscPvEGamePhase::HeroSelection);
    }
}

void AAscPvEGameMode::PreLogin(
    const FString& Options,
    const FString& Address,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    const int32 ConnectedPlayerCount = GameState ? GameState->PlayerArray.Num() : GetNumPlayers();
    if (ConnectedPlayerCount >= MaxPlayers)
    {
        ErrorMessage = TEXT("PvE server is full.");
    }

    UE_LOG(LogTemp, Log,
        TEXT("[PvEGM] PreLogin Address=%s Accepted=%d Players=%d/%d"),
        *Address,
        ErrorMessage.IsEmpty() ? 1 : 0,
        ConnectedPlayerCount,
        MaxPlayers);
}

APlayerController* AAscPvEGameMode::Login(
    UPlayer* NewPlayer,
    ENetRole InRemoteRole,
    const FString& Portal,
    const FString& Options,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    APlayerController* NewPlayerController = Super::Login(
        NewPlayer,
        InRemoteRole,
        Portal,
        Options,
        UniqueId,
        ErrorMessage);

    UE_LOG(LogTemp, Log,
        TEXT("[PvEGM] Login PC=%s Error=%s"),
        *GetNameSafe(NewPlayerController),
        *ErrorMessage);

    return NewPlayerController;
}

void AAscPvEGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(NewPlayer);
    AAscenduntPlayerState* AscPS = AscPC ? AscPC->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    if (!AscPC || !AscPS)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PvEGM] PostLogin failed to resolve ASC player types. PC=%s PS=%s"),
            *GetNameSafe(NewPlayer),
            *GetNameSafe(NewPlayer ? NewPlayer->GetPlayerState<APlayerState>() : nullptr));
        return;
    }

    AscPS->SetTeamId(PvETeamId);

    const int32 PlayerNumber = GameState
        ? GameState->PlayerArray.Num()
        : 1;

    AscPS->SetPlayerName(
        FString::Printf(TEXT("PLAYER %d"), PlayerNumber)
    );

    // PostLogin is the first safe server point to call an owning-client RPC.
    // The reusable selection UI sends MatchInitialSelect back through the
    // IAscHeroSelectionAuthorityInterface implemented by this GameMode.
    if (const AAscPvEGameState* PvEGameState = GetPvEGameState();
        PvEGameState && PvEGameState->GetGamePhase() == EAscPvEGamePhase::HeroSelection)
    {
        AscPC->Client_OpenHeroSelection(EAscHeroSelectionContext::MatchInitialSelect);
    }

    const int32 ConnectedPlayerCount = GameState ? GameState->PlayerArray.Num() : GetNumPlayers();
    UE_LOG(LogTemp, Log,
        TEXT("[PvEGM] PostLogin PC=%s Team=%d Players=%d/%d -> HeroSelection UI"),
        *GetNameSafe(AscPC),
        AscPS->GetTeamId(),
        ConnectedPlayerCount,
        RequiredPlayers);
}


AActor* AAscPvEGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    TArray<APlayerStart*> AvailableStarts;
    TArray<APlayerStart*> AllStarts;

    if (GetWorld())
    {
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            APlayerStart* PlayerStart = *It;
            if (!IsValid(PlayerStart))
            {
                continue;
            }

            AllStarts.Add(PlayerStart);
            if (IsPlayerStartAvailable(PlayerStart, Player))
            {
                AvailableStarts.Add(PlayerStart);
            }
        }
    }

    if (AvailableStarts.Num() > 0)
    {
        APlayerStart* SelectedStart = AvailableStarts[FMath::RandRange(0, AvailableStarts.Num() - 1)];
        UE_LOG(LogTemp, Verbose,
            TEXT("[PvEGM] ChoosePlayerStart available. PC=%s Start=%s"),
            *GetNameSafe(Player),
            *GetNameSafe(SelectedStart));
        return SelectedStart;
    }

    // Preserve the old fallback behavior if the level does not provide enough separated starts.
    // The spawn validation below will still reject an actual failed spawn instead of advancing phase.
    if (AllStarts.Num() > 0)
    {
        APlayerStart* SelectedStart = AllStarts[FMath::RandRange(0, AllStarts.Num() - 1)];
        UE_LOG(LogTemp, Warning,
            TEXT("[PvEGM] ChoosePlayerStart using occupied fallback. PC=%s Start=%s"),
            *GetNameSafe(Player),
            *GetNameSafe(SelectedStart));
        return SelectedStart;
    }

    return Super::ChoosePlayerStart_Implementation(Player);
}

bool AAscPvEGameMode::IsPlayerStartAvailable(const AActor* PlayerStart, const AController* Player) const
{
    if (!PlayerStart || PlayerStartOccupancyRadius <= 0.0f || !GetWorld())
    {
        return true;
    }

    const FVector StartLocation = PlayerStart->GetActorLocation();
    const float OccupancyRadiusSq = FMath::Square(PlayerStartOccupancyRadius);

    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        const APawn* ExistingPawn = *It;
        if (!IsValid(ExistingPawn) || ExistingPawn->GetController() == Player)
        {
            continue;
        }

        if (FVector::DistSquared(ExistingPawn->GetActorLocation(), StartLocation) <= OccupancyRadiusSq)
        {
            return false;
        }
    }

    return true;
}

UClass* AAscPvEGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    const AAscenduntPlayerState* AscPS = InController
        ? InController->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;
    const UHeroPawnData* PawnData = AscPS ? AscPS->GetPawnData() : nullptr;

    if (PawnData && PawnData->PawnClass)
    {
        return PawnData->PawnClass;
    }

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

bool AAscPvEGameMode::HandleHeroConfirmRequest(
    AAscenduntPlayerController* PlayerController,
    int32 HeroId,
    EAscHeroSelectionContext Context)
{
    AAscPvEGameState* PvEGameState = GetPvEGameState();
    if (!PlayerController || !PvEGameState ||
        PvEGameState->GetGamePhase() != EAscPvEGamePhase::HeroSelection ||
        Context != EAscHeroSelectionContext::MatchInitialSelect)
    {
        if (PlayerController)
        {
            PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::InvalidPhase);
        }
        return false;
    }

    if (!HeroSelectionComponent)
    {
        PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::InvalidHero);
        return false;
    }

    // A valid selectable entry must also be spawnable. The shared component
    // intentionally allows roster entries without PawnData, so PvE validates it here.
    const UHeroPawnData* PawnData = HeroSelectionComponent->ResolvePawnDataForHero(HeroId);
    if (!PawnData || !PawnData->PawnClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PvEGM] Hero confirm rejected: missing PawnData/PawnClass. PC=%s HeroId=%d"),
            *GetNameSafe(PlayerController),
            HeroId);
        PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::InvalidHero);
        return false;
    }

    EAscHeroSelectionRejectReason RejectReason = EAscHeroSelectionRejectReason::None;
    if (!HeroSelectionComponent->ConfirmHero(
        PlayerController,
        HeroId,
        Context,
        HeroSelectionRules,
        RejectReason))
    {
        PlayerController->Client_NotifyHeroConfirmResult(false, RejectReason);
        return false;
    }

    PlayerController->Client_NotifyHeroConfirmResult(true, EAscHeroSelectionRejectReason::None);

    int32 ConfirmedPlayerCount = 0;
    if (GameState)
    {
        for (APlayerState* PlayerState : GameState->PlayerArray)
        {
            const AAscenduntPlayerState* AscPS = Cast<AAscenduntPlayerState>(PlayerState);
            if (AscPS && AscPS->HasConfirmedHero())
            {
                ++ConfirmedPlayerCount;
            }
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[PvEGM] Hero confirmed. PC=%s HeroId=%d Confirmed=%d/%d"),
        *GetNameSafe(PlayerController),
        HeroId,
        ConfirmedPlayerCount,
        RequiredPlayers);

    TryStartPlaying();
    return true;
}

void AAscPvEGameMode::SetGamePhase(EAscPvEGamePhase NewPhase)
{
    if (AAscPvEGameState* PvEGameState = GetPvEGameState())
    {
        PvEGameState->SetGamePhase(NewPhase);
    }
}

AAscPvEGameState* AAscPvEGameMode::GetPvEGameState() const
{
    return GetGameState<AAscPvEGameState>();
}

bool AAscPvEGameMode::AreAllRequiredPlayersConfirmed() const
{
    if (!GameState || GameState->PlayerArray.Num() < RequiredPlayers)
    {
        return false;
    }

    int32 ValidPlayerCount = 0;
    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        const AAscenduntPlayerState* AscPS = Cast<AAscenduntPlayerState>(PlayerState);
        if (!AscPS)
        {
            continue;
        }

        ++ValidPlayerCount;

        const UHeroPawnData* PawnData = AscPS->GetPawnData();
        if (!AscPS->HasConfirmedHero() || !PawnData || !PawnData->PawnClass)
        {
            return false;
        }
    }

    return ValidPlayerCount >= RequiredPlayers;
}

bool AAscPvEGameMode::SpawnConfirmedHero(AAscenduntPlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return false;
    }

    AAscenduntPlayerState* AscPS = PlayerController->GetPlayerState<AAscenduntPlayerState>();
    const UHeroPawnData* PawnData = AscPS ? AscPS->GetPawnData() : nullptr;
    if (!AscPS || !AscPS->HasConfirmedHero() || !PawnData || !PawnData->PawnClass)
    {
        return false;
    }

    // Match the stable PvP pawn lifecycle: resolve a start first, explicitly enter
    // Playing controller state, then spawn/possess at that exact start.
    AActor* StartSpot = ChoosePlayerStart(PlayerController);
    if (!StartSpot)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PvEGM] Failed to spawn confirmed hero: no PlayerStart. PC=%s PawnClass=%s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PawnData->PawnClass));
        return false;
    }

    PlayerController->ChangeState(NAME_Playing);
    RestartPlayerAtPlayerStart(PlayerController, StartSpot);

    APawn* SpawnedPawn = PlayerController->GetPawn();
    const bool bSpawnedCorrectHero =
        IsValid(SpawnedPawn) &&
        SpawnedPawn->IsA(PawnData->PawnClass) &&
        SpawnedPawn->GetController() == PlayerController;
    if (!bSpawnedCorrectHero)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PvEGM] Failed to spawn confirmed hero. PC=%s PawnData=%s PawnClass=%s Start=%s Spawned=%s Controller=%s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PawnData),
            *GetNameSafe(PawnData->PawnClass),
            *GetNameSafe(StartSpot),
            *GetNameSafe(SpawnedPawn),
            *GetNameSafe(SpawnedPawn ? SpawnedPawn->GetController() : nullptr));
        return false;
    }

    return true;
}

bool AAscPvEGameMode::SpawnAllConfirmedHeroes()
{
    if (!GetWorld())
    {
        return false;
    }

    bool bAllSpawned = true;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get());
        if (!AscPC)
        {
            continue;
        }

        if (!SpawnConfirmedHero(AscPC))
        {
            bAllSpawned = false;
        }
    }

    return bAllSpawned;
}

void AAscPvEGameMode::NotifyHeroLifeStateChanged(AHeroCharacter* Hero)
{
    if (!Hero || !Hero->HasAuthority())
    {
        return;
    }

    EvaluateSquadWipe();
}

void AAscPvEGameMode::HandleHeroFullDeath(AHeroCharacter* DeadHero)
{
    if (!DeadHero || !DeadHero->HasAuthority())
    {
        return;
    }

    AAscenduntPlayerController* PlayerController = Cast<AAscenduntPlayerController>(DeadHero->GetController());
    AAscenduntPlayerState* AscPS = PlayerController
        ? PlayerController->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;
    if (!PlayerController || !AscPS)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PvEGM] FullDeath missing controller/playerstate. Hero=%s PC=%s PS=%s"),
            *GetNameSafe(DeadHero),
            *GetNameSafe(PlayerController),
            *GetNameSafe(AscPS));
        return;
    }

    if (UAscenduntAbilitySystemComponent* ASC = AscPS->GetAscenduntAbilitySystemComponent())
    {
        ASC->CancelAllAbilities();
        ASC->ClearAbilityInput();
    }

    // Explicitly tear down the dead Pawn before UnPossess. UnPossessed() calls the same
    // idempotent path, so no grants/input/camera bindings can leak into spectator state.
    DeadHero->DeinitializeForPawnReplacement(/*bPreserveHeroState*/ false);
    AscPS->SetRespawnState(EAscPlayerRespawnState::DeadSpectating);

    PlayerController->UnPossess();
    PlayerController->ChangeState(NAME_Spectating);
    PlayerController->Client_EnterDeathSpectating(DeadHero, /*RespawnEndServerTime*/ 0.0f);

    UE_LOG(LogTemp, Warning,
        TEXT("[PvEGM] FullDeath -> Spectating. PC=%s Hero=%s"),
        *GetNameSafe(PlayerController),
        *GetNameSafe(DeadHero));

    EvaluateSquadWipe();
}

int32 AAscPvEGameMode::CountAliveHeroes() const
{
    if (!GameState)
    {
        return 0;
    }

    int32 AliveCount = 0;
    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        const AAscenduntPlayerState* AscPS = Cast<AAscenduntPlayerState>(PlayerState);
        const AHeroCharacter* Hero = AscPS ? Cast<AHeroCharacter>(AscPS->GetPawn()) : nullptr;
        if (!AscPS || AscPS->GetRespawnState() != EAscPlayerRespawnState::Alive || !Hero)
        {
            continue;
        }

        const UAscenduntHealthComponent* HealthComp = Hero->GetAscenduntHealthComponent();
        const UAscPvEDownedComponent* DownedComp = Hero->GetPvEDownedComponent();

        const bool bFullyAlive =
            HealthComp && HealthComp->GetDeathState() == EAscenduntDeathState::NotDead &&
            (!DownedComp || !DownedComp->IsDowned());
        if (bFullyAlive)
        {
            ++AliveCount;
        }
    }

    return AliveCount;
}

void AAscPvEGameMode::EvaluateSquadWipe()
{
    AAscPvEGameState* PvEGameState = GetPvEGameState();
    if (!PvEGameState || PvEGameState->GetGamePhase() != EAscPvEGamePhase::Playing)
    {
        return;
    }

    const int32 AliveCount = CountAliveHeroes();
    if (AliveCount > 0)
    {
        return;
    }

    // Keep MissionDirector as the mission-result source whenever one exists so the
    // existing replicated Mission Fail presentation fires on every client.
    if (AAscMissionDirector* MissionDirector = BoundMissionDirector.Get())
    {
        MissionDirector->FailMission();
    }
    else
    {
        FinishMission(/*bMissionSuccess*/ false);
    }

    UE_LOG(LogTemp, Error,
        TEXT("[PvEGM] Squad wipe detected. AliveCount=0 -> MissionFailed"));
}

void AAscPvEGameMode::TryStartPlaying()
{
    AAscPvEGameState* PvEGameState = GetPvEGameState();
    if (!PvEGameState || PvEGameState->GetGamePhase() != EAscPvEGamePhase::HeroSelection)
    {
        return;
    }

    if (!AreAllRequiredPlayersConfirmed())
    {
        return;
    }

    if (!SpawnAllConfirmedHeroes())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PvEGM] All players confirmed, but one or more hero pawns failed to spawn. Phase remains HeroSelection."));
        return;
    }

    SetGamePhase(EAscPvEGamePhase::Playing);

    // Close selection only after the entire squad has confirmed and every hero
    // has been spawned/possessed successfully. No individual confirm closes UI.
    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
            {
                AscPC->Client_CloseHeroSelection();
            }
        }
    }

    if (AAscMissionDirector* MissionDirector = BoundMissionDirector.Get())
    {
        MissionDirector->StartMission();
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[PvEGM] All %d players confirmed, spawned and possessed. Phase -> Playing"),
        RequiredPlayers);
}

void AAscPvEGameMode::BindMissionDirector()
{
    if (!GetWorld())
    {
        return;
    }

    AAscMissionDirector* FirstMissionDirector = nullptr;
    int32 MissionDirectorCount = 0;

    for (TActorIterator<AAscMissionDirector> It(GetWorld()); It; ++It)
    {
        AAscMissionDirector* MissionDirector = *It;
        if (!IsValid(MissionDirector))
        {
            continue;
        }

        ++MissionDirectorCount;
        if (!FirstMissionDirector)
        {
            FirstMissionDirector = MissionDirector;
        }
    }

    if (!FirstMissionDirector)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PvEGM] No MissionDirector found. PvE can enter Playing, but mission completion must be driven manually."));
        return;
    }

    BoundMissionDirector = FirstMissionDirector;

    FirstMissionDirector->OnMissionCompleted.RemoveDynamic(this, &ThisClass::HandleMissionCompleted);
    FirstMissionDirector->OnMissionFailed.RemoveDynamic(this, &ThisClass::HandleMissionFailed);
    FirstMissionDirector->OnMissionCompleted.AddDynamic(this, &ThisClass::HandleMissionCompleted);
    FirstMissionDirector->OnMissionFailed.AddDynamic(this, &ThisClass::HandleMissionFailed);

    if (MissionDirectorCount > 1)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PvEGM] Found %d MissionDirectors. '%s' is bound as the PvE mission owner."),
            MissionDirectorCount,
            *GetNameSafe(FirstMissionDirector));
    }
}

void AAscPvEGameMode::HandleMissionCompleted(AAscMissionDirector* MissionDirector)
{
    if (MissionDirector != BoundMissionDirector.Get())
    {
        return;
    }

    FinishMission(/*bMissionSuccess*/ true);
}

void AAscPvEGameMode::HandleMissionFailed(AAscMissionDirector* MissionDirector)
{
    if (MissionDirector != BoundMissionDirector.Get())
    {
        return;
    }

    FinishMission(/*bMissionSuccess*/ false);
}

void AAscPvEGameMode::FinishMission(bool bMissionSuccess)
{
    AAscPvEGameState* PvEGameState = GetPvEGameState();
    if (!PvEGameState || bMissionEndStarted)
    {
        return;
    }

    const EAscPvEGamePhase CurrentPhase = PvEGameState->GetGamePhase();
    if (CurrentPhase != EAscPvEGamePhase::Playing)
    {
        return;
    }

    bMissionEndStarted = true;
    bPendingMissionSuccess = bMissionSuccess;

    SetGamePhase(
        bMissionSuccess
            ? EAscPvEGamePhase::MissionClear
            : EAscPvEGamePhase::MissionFailed);

    // Freeze local gameplay immediately while MissionObjectiveWidget presents the
    // already-existing Success/Fail title. Keep time dilation at 1 for PvE for now.
    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
            {
                AscPC->Client_BeginPostMatch(/*TimeDilationScale*/ 1.0f);
            }
        }

        const float HoldDuration = FMath::Max(0.0f, MissionResultHoldDuration);
        if (HoldDuration <= KINDA_SMALL_NUMBER)
        {
            BeginMissionExitTransition();
        }
        else
        {
            GetWorld()->GetTimerManager().SetTimer(
                MissionExitTimerHandle,
                this,
                &ThisClass::BeginMissionExitTransition,
                HoldDuration,
                false);
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[PvEGM] Mission finished. Result=%s Hold=%.2fs ExitTransition=%.2fs"),
        bMissionSuccess ? TEXT("Success") : TEXT("Failed"),
        MissionResultHoldDuration,
        MissionExitTransitionDuration);
}

void AAscPvEGameMode::BeginMissionExitTransition()
{
    if (!GetWorld() || !bMissionEndStarted)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(It->Get()))
        {
            AscPC->Client_BeginPvEMissionExitTransition(
                bPendingMissionSuccess,
                FMath::Max(0.0f, MissionExitTransitionDuration));
        }
    }
}
