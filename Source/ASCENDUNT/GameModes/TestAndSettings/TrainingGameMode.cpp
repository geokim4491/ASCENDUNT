#include "TrainingGameMode.h"

#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/HeroSelection/AscHeroSelectionComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"

ATrainingGameMode::ATrainingGameMode()
{
    PlayerControllerClass = AAscenduntPlayerController::StaticClass();
    PlayerStateClass = AAscenduntPlayerState::StaticClass();

    // Training does not spawn a default pawn until the player confirms a hero.
    DefaultPawnClass = nullptr;
    bStartPlayersAsSpectators = true;
    bDelayedStart = false;
    bUseSeamlessTravel = false;

    HeroSelectionComponent = CreateDefaultSubobject<UAscHeroSelectionComponent>(TEXT("HeroSelectionComponent"));

    // Training range should not block duplicate heroes/roles.
    HeroSelectionRules.bAllowDuplicateHeroes = true;
    HeroSelectionRules.bAllowDuplicateHeroRoles = true;
    HeroSelectionRules.bCheckDuplicateRulesWithinTeamOnly = true;
}

void ATrainingGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(NewPlayer);
    AAscenduntPlayerState* AscPS = AscPC ? AscPC->GetPlayerState<AAscenduntPlayerState>() : nullptr;

    if (AscPS)
    {
        AscPS->SetTeamId(TrainingTeamId);
        if (AscPS->GetDisplayName().IsEmpty())
        {
            AscPS->SetDisplayName(NewPlayer ? NewPlayer->GetName() : TEXT("TrainingPlayer"));
        }
    }

    if (bOpenHeroSelectionOnPostLogin && AscPC)
    {
        AscPC->Client_OpenHeroSelection(EAscHeroSelectionContext::InGameRespawnRoom);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[TrainingGM] PostLogin PC=%s PS=%s OpenHeroSelection=%d"),
        *GetNameSafe(AscPC),
        *GetNameSafe(AscPS),
        bOpenHeroSelectionOnPostLogin ? 1 : 0);
}

UClass* ATrainingGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    const AAscenduntPlayerState* AscPS = InController ? InController->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    const UHeroPawnData* PawnData = AscPS ? AscPS->GetPawnData() : nullptr;

    if (PawnData && PawnData->PawnClass)
    {
        return PawnData->PawnClass;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[TrainingGM] GetDefaultPawnClass fallback. Controller=%s PawnData=%s"),
        *GetNameSafe(InController),
        *GetNameSafe(PawnData));

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

bool ATrainingGameMode::HandleHeroConfirmRequest(
    AAscenduntPlayerController* PlayerController,
    int32 HeroId,
    EAscHeroSelectionContext Context)
{
    // Training range reuses the respawn-room selection UI/context regardless of
    // where the request came from. It is intentionally always available here.
    return ApplyTrainingHeroConfirm(PlayerController, HeroId);
}

bool ATrainingGameMode::ApplyTrainingHeroConfirm(AAscenduntPlayerController* PlayerController, int32 HeroId)
{
    if (!PlayerController || !HeroSelectionComponent)
    {
        if (PlayerController)
        {
            PlayerController->Client_NotifyHeroConfirmResult(false, EAscHeroSelectionRejectReason::InvalidPlayer);
        }
        return false;
    }

    EAscHeroSelectionRejectReason RejectReason = EAscHeroSelectionRejectReason::None;
    const bool bConfirmed = HeroSelectionComponent->ConfirmHero(
        PlayerController,
        HeroId,
        EAscHeroSelectionContext::InGameRespawnRoom,
        HeroSelectionRules,
        RejectReason);

    if (!bConfirmed)
    {
        PlayerController->Client_NotifyHeroConfirmResult(false, RejectReason);
        UE_LOG(LogTemp, Warning,
            TEXT("[TrainingGM] Hero confirm rejected. PC=%s HeroId=%d Reason=%d"),
            *GetNameSafe(PlayerController),
            HeroId,
            static_cast<int32>(RejectReason));
        return false;
    }

    const bool bSpawned = SpawnOrReplaceHeroPawn(PlayerController);
    PlayerController->Client_NotifyHeroConfirmResult(bSpawned, bSpawned ? EAscHeroSelectionRejectReason::None : EAscHeroSelectionRejectReason::InvalidHero);

    UE_LOG(LogTemp, Warning,
        TEXT("[TrainingGM] Hero confirm accepted. PC=%s HeroId=%d Spawned=%d"),
        *GetNameSafe(PlayerController),
        HeroId,
        bSpawned ? 1 : 0);

    return bSpawned;
}

bool ATrainingGameMode::SpawnOrReplaceHeroPawn(AAscenduntPlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return false;
    }

    AAscenduntPlayerState* AscPS = PlayerController->GetPlayerState<AAscenduntPlayerState>();
    const UHeroPawnData* PawnData = AscPS ? AscPS->GetPawnData() : nullptr;
    if (!AscPS || !PawnData || !PawnData->PawnClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[TrainingGM] Cannot spawn hero pawn. PC=%s PawnData=%s PawnClass=%s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PawnData),
            PawnData ? *GetNameSafe(PawnData->PawnClass) : TEXT("None"));
        return false;
    }

    APawn* OldPawn = PlayerController->GetPawn();
    if (OldPawn && bDestroyPreviousPawnOnHeroChange)
    {
        if (AHeroCharacter* OldHero = Cast<AHeroCharacter>(OldPawn))
        {
            // TrainingGameMode also delegates all Pawn-scoped teardown to InitComp.
            OldHero->DeinitializeForPawnReplacement();
        }

        PlayerController->UnPossess();
        OldPawn->Destroy();
    }

    RestartPlayer(PlayerController);

    APawn* NewPawn = PlayerController->GetPawn();
    const bool bSpawned = IsValid(NewPawn) && NewPawn->IsA(PawnData->PawnClass);
    if (!bSpawned)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[TrainingGM] Spawned Pawn does not match PawnData. PC=%s PawnData=%s PawnClass=%s NewPawn=%s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PawnData),
            *GetNameSafe(PawnData->PawnClass),
            *GetNameSafe(NewPawn));
        return false;
    }

    // HeroInitComponent owns ASC ActorInfo, PawnData AbilitySet application and
    // GameplayReady progression. TrainingGameMode only manages Pawn replacement.
    UE_LOG(LogTemp, Warning,
        TEXT("[TrainingGM] SpawnOrReplaceHeroPawn PC=%s PawnData=%s PawnClass=%s NewPawn=%s Result=1"),
        *GetNameSafe(PlayerController),
        *GetNameSafe(PawnData),
        *GetNameSafe(PawnData->PawnClass),
        *GetNameSafe(NewPawn));

    return true;
}
