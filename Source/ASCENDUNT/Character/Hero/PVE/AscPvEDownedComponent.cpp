#include "AscPvEDownedComponent.h"
#include "AscPvEDownedConfig.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroCameraComponent.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Camera/AscenduntCameraComponent.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameMode.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/AbilitySystem/Components/AscCombatContributionComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UAscPvEDownedComponent::UAscPvEDownedComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

bool UAscPvEDownedComponent::EnterDowned()
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Hero->HasAuthority() || bIsDowned)
    {
        return false;
    }

    if (!Config)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PvEDowned] Shared DownedConfig is missing on %s. Expected AHeroCharacter to inject DA_PvEDownedConfig."),
            *GetNameSafe(Hero));
        return false;
    }

    // If this hero was reviving somebody else, becoming Downed immediately cancels that revive.
    if (AHeroCharacter* ExistingTarget = ReviveTarget.Get())
    {
        if (UAscPvEDownedComponent* TargetDowned = ExistingTarget->GetPvEDownedComponent())
        {
            TargetDowned->CancelRevive(Hero);
        }
        SetReviveTarget(nullptr);
    }

    ClearReviveGraceInvincibility();
    SetActiveReviver(nullptr);

    bIsDowned = true;

    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    const float ServerNow = GameState
        ? GameState->GetServerWorldTimeSeconds()
        : (World ? World->GetTimeSeconds() : 0.0f);

    BleedoutEndServerTime = ServerNow + Config->BleedoutDuration;
    ApplyDownedStateLocally(true);
    OnDownedStateChanged.Broadcast(true);

    if (AAscenduntPlayerState* PlayerState = Hero->GetPlayerState<AAscenduntPlayerState>())
    {
        if (UAscCombatContributionComponent* Contribution = PlayerState->GetCombatContributionComponent())
        {
            Contribution->RecordDowned();
        }
    }

    if (UWorld* MutableWorld = GetWorld())
    {
        MutableWorld->GetTimerManager().SetTimer(
            BleedoutTimerHandle,
            this,
            &ThisClass::HandleBleedoutExpired,
            Config->BleedoutDuration,
            false);
    }

    Hero->ForceNetUpdate();

    if (AAscPvEGameMode* PvEGameMode = World ? World->GetAuthGameMode<AAscPvEGameMode>() : nullptr)
    {
        PvEGameMode->NotifyHeroLifeStateChanged(Hero);
    }

    UE_LOG(LogTemp, Log,
        TEXT("[PvEDowned] EnterDowned Hero=%s Duration=%.2f EndServerTime=%.2f"),
        *GetNameSafe(Hero),
        Config->BleedoutDuration,
        BleedoutEndServerTime);

    return true;
}

float UAscPvEDownedComponent::GetBleedoutRemainingTime() const
{
    if (!bIsDowned || BleedoutEndServerTime <= 0.0f)
    {
        return 0.0f;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.0f;
    }

    const AGameStateBase* GameState = World->GetGameState();
    const float ServerNow = GameState
        ? GameState->GetServerWorldTimeSeconds()
        : World->GetTimeSeconds();

    return FMath::Max(0.0f, BleedoutEndServerTime - ServerNow);
}

bool UAscPvEDownedComponent::CanBeRevivedBy(const AHeroCharacter* Interactor) const
{
    const AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Interactor || Hero == Interactor || !bIsDowned)
    {
        return false;
    }

    // Revive must COMPLETE before bleedout reaches zero.
    if (GetBleedoutRemainingTime() <= 0.0f)
    {
        return false;
    }

    if (Hero->GetGenericTeamId().GetId() != Interactor->GetGenericTeamId().GetId())
    {
        return false;
    }

    const UAscPvEDownedComponent* InteractorDowned = Interactor->GetPvEDownedComponent();
    if (InteractorDowned && InteractorDowned->IsDowned())
    {
        return false;
    }

    const UAscenduntHealthComponent* InteractorHealth = Interactor->GetAscenduntHealthComponent();
    if (!InteractorHealth || InteractorHealth->GetDeathState() != EAscenduntDeathState::NotDead)
    {
        return false;
    }

    // Exactly one reviver per target. The current reviver may revalidate its own Hold.
    if (IsValid(ActiveReviver) && ActiveReviver != Interactor)
    {
        return false;
    }

    return true;
}

FAscInteractionOption UAscPvEDownedComponent::GetReviveInteractionOption() const
{
    FAscInteractionOption Option;
    Option.PromptText = NSLOCTEXT("AscenduntPvE", "ReviveInteractionPrompt", "부활");
    Option.InputMode = EAscInteractionInputMode::Hold;
    Option.HoldDuration = Config ? Config->ReviveHoldDuration : 0.0f;
    Option.MaxDistance = Config ? Config->ReviveMaxDistance : 0.0f;
    Option.bLockMovement = true;
    Option.Priority = 100.0f;
    return Option;
}

bool UAscPvEDownedComponent::BeginRevive(AHeroCharacter* Interactor)
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Hero->HasAuthority() || !CanBeRevivedBy(Interactor))
    {
        return false;
    }

    SetActiveReviver(Interactor);

    UE_LOG(LogTemp, Log,
        TEXT("[PvEDowned] BeginRevive Target=%s Reviver=%s"),
        *GetNameSafe(Hero),
        *GetNameSafe(Interactor));

    return true;
}

void UAscPvEDownedComponent::CancelRevive(AHeroCharacter* Interactor)
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Hero->HasAuthority() || ActiveReviver != Interactor)
    {
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[PvEDowned] CancelRevive Target=%s Reviver=%s"),
        *GetNameSafe(Hero),
        *GetNameSafe(Interactor));

    SetActiveReviver(nullptr);
}

bool UAscPvEDownedComponent::CompleteRevive(AHeroCharacter* Interactor)
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Hero->HasAuthority() || ActiveReviver != Interactor || !CanBeRevivedBy(Interactor))
    {
        return false;
    }

    UAscenduntHealthComponent* HealthComp = Hero->GetAscenduntHealthComponent();
    if (!HealthComp || HealthComp->GetDeathState() != EAscenduntDeathState::DeathStarted)
    {
        SetActiveReviver(nullptr);
        return false;
    }

    // Keep DeathStarted/Status.PvE.Downed active while the revive heal executes so
    // HealthSet accepts only the Effect.Revive exception and resets its out-of-health guard.
    if (!ApplyReviveHealing(Interactor))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PvEDowned] CompleteRevive failed to apply revive healing. Target=%s"),
            *GetNameSafe(Hero));
        SetActiveReviver(nullptr);
        return false;
    }

    if (HealthComp->GetHealth() <= KINDA_SMALL_NUMBER || !HealthComp->ReviveFromDeathStarted())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[PvEDowned] Revive heal did not restore a valid living health state. Target=%s Health=%.2f"),
            *GetNameSafe(Hero),
            HealthComp->GetHealth());
        SetActiveReviver(nullptr);
        return false;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BleedoutTimerHandle);
    }

    // Set bIsDowned first so ActiveReviver cleanup cannot momentarily restore crawl movement.
    bIsDowned = false;
    BleedoutEndServerTime = 0.0f;
    SetActiveReviver(nullptr);
    ApplyDownedStateLocally(false);
    OnDownedStateChanged.Broadcast(false);
    ApplyReviveGraceInvincibility();

    if (AAscenduntPlayerState* ReviverPS = Interactor->GetPlayerState<AAscenduntPlayerState>())
    {
        if (UAscCombatContributionComponent* Contribution = ReviverPS->GetCombatContributionComponent())
        {
            Contribution->RecordRevive();
        }
    }

    Hero->ForceNetUpdate();

    if (UWorld* World = GetWorld())
    {
        if (AAscPvEGameMode* PvEGameMode = World->GetAuthGameMode<AAscPvEGameMode>())
        {
            PvEGameMode->NotifyHeroLifeStateChanged(Hero);
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[PvEDowned] ReviveComplete Target=%s Reviver=%s Grace=%.2f"),
        *GetNameSafe(Hero),
        *GetNameSafe(Interactor),
        Config ? Config->ReviveGraceDuration : 0.0f);

    return true;
}

bool UAscPvEDownedComponent::ApplyReviveHealing(AHeroCharacter* Interactor)
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Hero->HasAuthority() || !Interactor || !Config || !Config->ReviveHealEffectClass)
    {
        if (!Config || !Config->ReviveHealEffectClass)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[PvEDowned] ReviveHealEffectClass is not configured in shared DA_PvEDownedConfig. Target=%s"),
                *GetNameSafe(Hero));
        }
        return false;
    }

    UAscenduntHealthComponent* HealthComp = Hero->GetAscenduntHealthComponent();
    UAscenduntAbilitySystemComponent* SourceASC = Interactor->GetAscenduntAbilitySystemComponent();
    UAscenduntAbilitySystemComponent* TargetASC = Hero->GetAscenduntAbilitySystemComponent();
    if (!HealthComp || !SourceASC || !TargetASC)
    {
        return false;
    }

    FAscenduntHealthSnapshot Snapshot;
    if (!HealthComp->GetHealthSnapshot(Snapshot))
    {
        return false;
    }

    // OverHealth is intentionally excluded because normal healing never fills it.
    const float HealableMax = Snapshot.MaxBasicHealth + Snapshot.MaxArmor + Snapshot.MaxThorn;
    const float ReviveAmount = FMath::Max(0.0f, HealableMax * Config->ReviveHealthFraction);
    if (ReviveAmount <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddInstigator(Interactor, Interactor);

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
        Config->ReviveHealEffectClass,
        1.0f,
        Context);

    if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
    {
        return false;
    }

    SpecHandle.Data->SetSetByCallerMagnitude(
        AscenduntGameplayTags::Data_HealOverride,
        ReviveAmount);

    // Do not rely on an editor-side asset tag. This runtime Spec is explicitly a revive.
    SpecHandle.Data->AddDynamicAssetTag(AscenduntGameplayTags::Effect_Revive);

    SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
    return true;
}

void UAscPvEDownedComponent::OnRep_IsDowned()
{
    ApplyDownedStateLocally(bIsDowned);
    ApplyBeingRevivedStateLocally();
    OnDownedStateChanged.Broadcast(bIsDowned);
}

void UAscPvEDownedComponent::OnRep_ActiveReviver()
{
    ApplyBeingRevivedStateLocally();
    OnRevivePresentationChanged.Broadcast();
}

void UAscPvEDownedComponent::OnRep_ReviveTarget()
{
    OnRevivePresentationChanged.Broadcast();
}

void UAscPvEDownedComponent::SetActiveReviver(AHeroCharacter* NewReviver)
{
    if (ActiveReviver == NewReviver)
    {
        return;
    }

    AHeroCharacter* TargetHero = Cast<AHeroCharacter>(GetOwner());
    AHeroCharacter* PreviousReviver = ActiveReviver.Get();

    ActiveReviver = NewReviver;

    // Mirror the relation on the reviver so UI/animation/gameplay consumers can read one replicated state.
    if (PreviousReviver && PreviousReviver != NewReviver)
    {
        if (UAscPvEDownedComponent* PreviousDowned = PreviousReviver->GetPvEDownedComponent())
        {
            if (PreviousDowned->ReviveTarget.Get() == TargetHero)
            {
                PreviousDowned->SetReviveTarget(nullptr);
            }
        }
    }

    if (NewReviver && TargetHero)
    {
        if (UAscPvEDownedComponent* NewReviverDowned = NewReviver->GetPvEDownedComponent())
        {
            NewReviverDowned->SetReviveTarget(TargetHero);
        }
    }

    ApplyBeingRevivedStateLocally();
    OnRevivePresentationChanged.Broadcast();

    if (AActor* Owner = GetOwner())
    {
        Owner->ForceNetUpdate();
    }
}

void UAscPvEDownedComponent::SetReviveTarget(AHeroCharacter* NewTarget)
{
    if (ReviveTarget == NewTarget)
    {
        return;
    }

    ReviveTarget = NewTarget;
    OnRevivePresentationChanged.Broadcast();

    if (AActor* Owner = GetOwner())
    {
        Owner->ForceNetUpdate();
    }
}

void UAscPvEDownedComponent::ApplyBeingRevivedStateLocally()
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero)
    {
        return;
    }

    if (!bIsDowned)
    {
        Hero->SetDownedMovementOverride(false, 0.0f);
        return;
    }

    const bool bBeingRevived = IsValid(ActiveReviver);
    const float CrawlSpeed = Config ? Config->DownedCrawlSpeed : 0.0f;
    Hero->SetDownedMovementOverride(true, bBeingRevived ? 0.0f : CrawlSpeed);

    if (bBeingRevived)
    {
        if (UCharacterMovementComponent* Movement = Hero->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }
}

void UAscPvEDownedComponent::ApplyDownedStateLocally(bool bEnable)
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero)
    {
        return;
    }

    Hero->SetDownedMovementOverride(
        bEnable,
        (bEnable && IsValid(ActiveReviver)) ? 0.0f : (Config ? Config->DownedCrawlSpeed : 0.0f));

    // Camera/post-process are intentionally owner-local. A dead spectator viewing a
    // Downed ally should see the normal scene through the spectator camera, not that ally's grayscale.
    if (Hero->IsLocallyControlled())
    {
        if (UHeroCameraComponent* HeroCamera = Hero->GetHeroCameraComponent())
        {
            HeroCamera->SetDownedCameraModeActive(bEnable);
        }

        if (UAscenduntCameraComponent* Camera = UAscenduntCameraComponent::FindCameraComponent(Hero))
        {
            Camera->SetDownedGrayscaleEnabled(bEnable);
        }
    }

    UAscenduntAbilitySystemComponent* ASC = Hero->GetAscenduntAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    ASC->SetLooseGameplayTagCount(
        AscenduntGameplayTags::Status_PvE_Downed,
        bEnable ? 1 : 0);

    if (bEnable)
    {
        // Stop fire/reload/skill abilities that were already running when health reached zero.
        // Movement/look are native input and remain available for crawling.
        if (Hero->HasAuthority() || Hero->IsLocallyControlled())
        {
            ASC->CancelAllAbilities();
            ASC->ClearAbilityInput();
        }

        if (!bAbilityInputBlockApplied)
        {
            ASC->AddLooseGameplayTag(TAG_Gameplay_AbilityInputBlocked);
            bAbilityInputBlockApplied = true;
        }

        // Authoritative damage execution already respects Status.Invincible.
        if (!bInvincibleTagApplied)
        {
            ASC->AddLooseGameplayTag(AscenduntGameplayTags::Status_Invincible);
            bInvincibleTagApplied = true;
        }
    }
    else
    {
        if (bAbilityInputBlockApplied)
        {
            ASC->RemoveLooseGameplayTag(TAG_Gameplay_AbilityInputBlocked);
            bAbilityInputBlockApplied = false;
        }

        if (bInvincibleTagApplied)
        {
            ASC->RemoveLooseGameplayTag(AscenduntGameplayTags::Status_Invincible);
            bInvincibleTagApplied = false;
        }
    }
}

void UAscPvEDownedComponent::ApplyReviveGraceInvincibility()
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    const float GraceDuration = Config ? Config->ReviveGraceDuration : 0.0f;
    if (!Hero || !Hero->HasAuthority() || GraceDuration <= 0.0f)
    {
        return;
    }

    UAscenduntAbilitySystemComponent* ASC = Hero->GetAscenduntAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    ClearReviveGraceInvincibility();

    ASC->AddLooseGameplayTag(AscenduntGameplayTags::Status_Invincible);
    bReviveGraceInvincibleApplied = true;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ReviveGraceTimerHandle,
            this,
            &ThisClass::HandleReviveGraceExpired,
            GraceDuration,
            false);
    }
}

void UAscPvEDownedComponent::HandleReviveGraceExpired()
{
    ClearReviveGraceInvincibility();
}

void UAscPvEDownedComponent::ClearReviveGraceInvincibility()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReviveGraceTimerHandle);
    }

    if (!bReviveGraceInvincibleApplied)
    {
        return;
    }

    if (AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner()))
    {
        if (UAscenduntAbilitySystemComponent* ASC = Hero->GetAscenduntAbilitySystemComponent())
        {
            ASC->RemoveLooseGameplayTag(AscenduntGameplayTags::Status_Invincible);
        }
    }

    bReviveGraceInvincibleApplied = false;
}

void UAscPvEDownedComponent::HandleBleedoutExpired()
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Hero->HasAuthority() || !bIsDowned)
    {
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[PvEDowned] Bleedout expired. Hero=%s -> FullDeath"),
        *GetNameSafe(Hero));

    EnterFullDeath();
}

void UAscPvEDownedComponent::EnterFullDeath()
{
    AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
    if (!Hero || !Hero->HasAuthority() || !bIsDowned)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BleedoutTimerHandle);
    }

    bIsDowned = false;
    BleedoutEndServerTime = 0.0f;
    SetActiveReviver(nullptr);
    ApplyDownedStateLocally(false);
    OnDownedStateChanged.Broadcast(false);
    Hero->ForceNetUpdate();

    Hero->EnterDeathRagdoll();

    if (UAscenduntHealthComponent* HealthComp = Hero->GetAscenduntHealthComponent())
    {
        HealthComp->FinishDeath();
    }
}

void UAscPvEDownedComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BleedoutTimerHandle);
        World->GetTimerManager().ClearTimer(ReviveGraceTimerHandle);
    }

    SetActiveReviver(nullptr);
    SetReviveTarget(nullptr);
    ClearReviveGraceInvincibility();
    ApplyDownedStateLocally(false);
    Super::EndPlay(EndPlayReason);
}

void UAscPvEDownedComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UAscPvEDownedComponent, bIsDowned);
    DOREPLIFETIME(UAscPvEDownedComponent, BleedoutEndServerTime);
    DOREPLIFETIME(UAscPvEDownedComponent, ActiveReviver);
    DOREPLIFETIME(UAscPvEDownedComponent, ReviveTarget);
}
