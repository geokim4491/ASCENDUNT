// Fill out your copyright notice in the Description page of Project Settings.

#include "AscenduntPlayerState.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySet.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntCharacterStatSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntUltimateSet.h"
#include "ASCENDUNT/AbilitySystem/Components/AscCombatContributionComponent.h"
#include "AscenduntPlayerController.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

AAscenduntPlayerState::AAscenduntPlayerState(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UAscenduntAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // These AttributeSets are shared by every hero and must survive Pawn replacement.
    // Keeping their lifetime equal to the PlayerState ASC avoids SpawnedAttributes /
    // ActiveGameplayEffects replication-order crashes during death and hero replacement.
    HealthSet = ObjectInitializer.CreateDefaultSubobject<UAscenduntHealthSet>(this, TEXT("HealthSet"));
    CharacterStatSet = ObjectInitializer.CreateDefaultSubobject<UAscenduntCharacterStatSet>(this, TEXT("CharacterStatSet"));
    WeaponSet = ObjectInitializer.CreateDefaultSubobject<UAscenduntWeaponSet>(this, TEXT("WeaponSet"));
    UltimateSet = ObjectInitializer.CreateDefaultSubobject<UAscenduntUltimateSet>(this, TEXT("UltimateSet"));
    CombatContributionComponent = ObjectInitializer.CreateDefaultSubobject<UAscCombatContributionComponent>(this, TEXT("CombatContributionComponent"));

    SetNetUpdateFrequency(100.0f);
}

void AAscenduntPlayerState::SetGenericTeamId(const FGenericTeamId& NewID)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    const uint8 Old = TeamId;
    const uint8 NewTeamId = NewID.GetId();
    if (Old == NewTeamId)
    {
        return;
    }

    TeamId = NewTeamId;
    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, TeamId, this);

    OnTeamIdChanged.Broadcast(Old, TeamId);
    OnPlayerStateChanged.Broadcast();

    ForceNetUpdate();
}

void AAscenduntPlayerState::SetDisplayName(const FString& InName)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    const FString ResolvedName = InName.IsEmpty() ? GetPlayerName() : InName;
    if (DisplayName == ResolvedName)
    {
        return;
    }

    DisplayName = ResolvedName;
    SetPlayerName(ResolvedName);

    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, DisplayName, this);
    OnPlayerStateChanged.Broadcast();

    ForceNetUpdate();
}

void AAscenduntPlayerState::SetMatchPlayerId(const FString& InPlayerId)
{
    if (GetLocalRole() != ROLE_Authority || MatchPlayerId == InPlayerId)
    {
        return;
    }

    MatchPlayerId = InPlayerId;
    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, MatchPlayerId, this);
    OnPlayerStateChanged.Broadcast();
    ForceNetUpdate();
}

void AAscenduntPlayerState::SetMatchTicketId(const FString& InTicketId)
{
    if (GetLocalRole() != ROLE_Authority || MatchTicketId == InTicketId)
    {
        return;
    }

    MatchTicketId = InTicketId;
    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, MatchTicketId, this);
    OnPlayerStateChanged.Broadcast();
    ForceNetUpdate();
}

void AAscenduntPlayerState::SetSelectedHeroId(int32 InHeroId)
{
    if (GetLocalRole() != ROLE_Authority || SelectedHeroId == InHeroId)
    {
        return;
    }

    SelectedHeroId = InHeroId;
    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, SelectedHeroId, this);
    OnPlayerStateChanged.Broadcast();
    ForceNetUpdate();
}

void AAscenduntPlayerState::SetConfirmedHero(int32 InHeroId, EAscHeroRole InHeroRole)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    bool bChanged = false;

    if (SelectedHeroId != InHeroId)
    {
        SelectedHeroId = InHeroId;
        MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, SelectedHeroId, this);
        bChanged = true;
    }

    if (ConfirmedHeroId != InHeroId)
    {
        ConfirmedHeroId = InHeroId;
        MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, ConfirmedHeroId, this);
        bChanged = true;
    }

    if (ConfirmedHeroRole != InHeroRole)
    {
        ConfirmedHeroRole = InHeroRole;
        MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, ConfirmedHeroRole, this);
        bChanged = true;
    }

    if (!bHeroConfirmed)
    {
        bHeroConfirmed = true;
        MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, bHeroConfirmed, this);
        bChanged = true;
    }

    if (bChanged)
    {
        OnPlayerStateChanged.Broadcast();
        ForceNetUpdate();
    }
}


void AAscenduntPlayerState::SetRespawnState(EAscPlayerRespawnState NewState, float InRespawnEndServerTime)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    const float ResolvedEndTime = NewState == EAscPlayerRespawnState::DeadWaitingRespawn
        ? FMath::Max(0.0f, InRespawnEndServerTime)
        : 0.0f;

    bool bChanged = false;
    if (RespawnState != NewState)
    {
        RespawnState = NewState;
        MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, RespawnState, this);
        bChanged = true;
    }

    if (!FMath::IsNearlyEqual(RespawnEndServerTime, ResolvedEndTime))
    {
        RespawnEndServerTime = ResolvedEndTime;
        MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, RespawnEndServerTime, this);
        bChanged = true;
    }

    if (bChanged)
    {
        OnPlayerStateChanged.Broadcast();
        ForceNetUpdate();
    }
}

AAscenduntPlayerController* AAscenduntPlayerState::GetAscenduntPlayerController() const
{
    return Cast<AAscenduntPlayerController>(GetOwner());
}

UAbilitySystemComponent* AAscenduntPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AAscenduntPlayerState::InitializeCombatForCurrentPawnData()
{
    if (GetLocalRole() != ROLE_Authority || !AbilitySystemComponent || !PawnData || !CombatContributionComponent)
    {
        return;
    }

    CombatContributionComponent->InitializeForPawnData(PawnData, AbilitySystemComponent);
}

void AAscenduntPlayerState::SetPawnData(const UHeroPawnData* InPawnData)
{
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        return; // 에디터 프리뷰/에셋 열기 같은 상황 방지
    }

    check(InPawnData);

    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    if (PawnData == InPawnData)
    {
        return;
    }

    // PawnData selection and AbilitySet application intentionally have different lifetimes.
    // The selected PawnData is needed before spawning the new Pawn, while its AbilitySets
    // must be granted only after that Pawn becomes the ASC AvatarActor.
    PawnData = InPawnData;

    // AppliedPawnData represents what is still installed on the persistent PlayerState ASC.
    // During death selection it intentionally remains the previous hero until respawn decides
    // whether the hero state should be preserved or replaced.
    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, PawnData, this);

    OnPawnDataAvailable.Broadcast();
    ForceNetUpdate();
}

void AAscenduntPlayerState::RemovePawnDataAbilitySets()
{
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    if (!AbilitySystemComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("RemovePawnDataAbilitySets failed: ASC is null on player state [%s]."), *GetNameSafe(this));
        return;
    }

    // Handles remember the grants from the previously applied PawnData even if PawnData
    // has already been changed to the newly selected hero.
    PawnDataAbilitySetHandles.TakeFromAbilitySystem(AbilitySystemComponent);

    if (HealthSet)
    {
        HealthSet->SetSourcePawnData(nullptr);
    }


    AppliedPawnData = nullptr;
    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, AppliedPawnData, this);

    ForceNetUpdate();
}

bool AAscenduntPlayerState::ApplyPawnDataAbilitySets()
{
    if (GetLocalRole() != ROLE_Authority)
    {
        return false;
    }

    if (!AbilitySystemComponent || !PawnData)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ApplyPawnDataAbilitySets failed: PS=%s ASC=%s PawnData=%s"),
            *GetNameSafe(this),
            *GetNameSafe(AbilitySystemComponent),
            *GetNameSafe(PawnData));
        return false;
    }

    if (AppliedPawnData == PawnData)
    {
        return true;
    }

    if (AppliedPawnData || !PawnDataAbilitySetHandles.IsEmpty())
    {
        UE_LOG(LogTemp, Error,
            TEXT("ApplyPawnDataAbilitySets failed: previous PawnData grants are still active. PS=%s Applied=%s Requested=%s"),
            *GetNameSafe(this),
            *GetNameSafe(AppliedPawnData),
            *GetNameSafe(PawnData));
        return false;
    }

    AActor* AvatarActor = AbilitySystemComponent->GetAvatarActor();
    if (!IsValid(AvatarActor) || !PawnData->PawnClass || !AvatarActor->IsA(PawnData->PawnClass))
    {
        UE_LOG(LogTemp, Error,
            TEXT("ApplyPawnDataAbilitySets failed: ASC AvatarActor does not match PawnData. PS=%s PawnData=%s PawnClass=%s Avatar=%s"),
            *GetNameSafe(this),
            *GetNameSafe(PawnData),
            PawnData->PawnClass ? *GetNameSafe(PawnData->PawnClass) : TEXT("None"),
            *GetNameSafe(AvatarActor));
        return false;
    }

    for (const UAscenduntAbilitySet* AbilitySet : PawnData->AbilitySets)
    {
        if (AbilitySet)
        {
            AbilitySet->GiveToAbilitySystem(
                AbilitySystemComponent,
                &PawnDataAbilitySetHandles,
                this,
                EAscAttributeSetGrantPolicy::Skip);
        }
    }

    const bool bCommonSetsReady =
        HealthSet && CharacterStatSet && WeaponSet && UltimateSet &&
        AbilitySystemComponent->GetSet<UAscenduntHealthSet>() == HealthSet &&
        AbilitySystemComponent->GetSet<UAscenduntCharacterStatSet>() == CharacterStatSet &&
        AbilitySystemComponent->GetSet<UAscenduntWeaponSet>() == WeaponSet &&
        AbilitySystemComponent->GetSet<UAscenduntUltimateSet>() == UltimateSet;

    if (!bCommonSetsReady)
    {
        UE_LOG(LogTemp, Error,
            TEXT("ApplyPawnDataAbilitySets failed: persistent common AttributeSets are not registered. PS=%s Health=%s Stat=%s Weapon=%s Ultimate=%s"),
            *GetNameSafe(this),
            *GetNameSafe(HealthSet),
            *GetNameSafe(CharacterStatSet),
            *GetNameSafe(WeaponSet),
            *GetNameSafe(UltimateSet));
        PawnDataAbilitySetHandles.TakeFromAbilitySystem(AbilitySystemComponent);
        return false;
    }

    HealthSet->SetSourcePawnData(PawnData);

    AppliedPawnData = PawnData;
    MARK_PROPERTY_DIRTY_FROM_NAME(AAscenduntPlayerState, AppliedPawnData, this);

    // Server does not broadcast. Remote clients are notified by OnRep_AppliedPawnData().
    ForceNetUpdate();
    return true;
}

void AAscenduntPlayerState::PreInitializeComponents()
{
    Super::PreInitializeComponents();
}

void AAscenduntPlayerState::PostInitializeComponents()
{
    Super::PostInitializeComponents();
}

void AAscenduntPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PawnData, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, AppliedPawnData, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, TeamId, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DisplayName, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MatchPlayerId, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MatchTicketId, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SelectedHeroId, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ConfirmedHeroId, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ConfirmedHeroRole, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bHeroConfirmed, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, RespawnState, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, RespawnEndServerTime, SharedParams);
}

void AAscenduntPlayerState::OnRep_PawnData()
{
    OnPawnDataAvailable.Broadcast();
}

void AAscenduntPlayerState::OnRep_AppliedPawnData()
{


    OnPawnDataApplied.Broadcast();
}

void AAscenduntPlayerState::OnRep_TeamId(uint8 OldTeamId)
{
    OnTeamIdChanged.Broadcast(OldTeamId, TeamId);
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_DisplayName()
{
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_MatchPlayerId()
{
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_MatchTicketId()
{
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_SelectedHeroId()
{
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_ConfirmedHeroId()
{
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_ConfirmedHeroRole()
{
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_HeroConfirmed()
{
    OnPlayerStateChanged.Broadcast();
}



void AAscenduntPlayerState::OnRep_RespawnState()
{
    OnPlayerStateChanged.Broadcast();
}

void AAscenduntPlayerState::OnRep_RespawnEndServerTime()
{
    OnPlayerStateChanged.Broadcast();
}
