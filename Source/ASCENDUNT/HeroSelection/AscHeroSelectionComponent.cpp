#include "AscHeroSelectionComponent.h"

#include "AscHeroRosterData.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

UAscHeroSelectionComponent::UAscHeroSelectionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UAscHeroSelectionComponent::CanConfirmHero(
    AAscenduntPlayerController* PlayerController,
    int32 HeroId,
    const FAscHeroSelectionRules& Rules,
    EAscHeroSelectionRejectReason& OutRejectReason) const
{
    OutRejectReason = EAscHeroSelectionRejectReason::None;

    const AAscenduntPlayerState* AscPS = PlayerController ? PlayerController->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    if (!PlayerController || !AscPS)
    {
        OutRejectReason = EAscHeroSelectionRejectReason::InvalidPlayer;
        return false;
    }

    if (!IsHeroIdValid(HeroId))
    {
        OutRejectReason = EAscHeroSelectionRejectReason::InvalidHero;
        return false;
    }

    if (WouldViolateDuplicateHeroRule(AscPS, HeroId, Rules))
    {
        OutRejectReason = EAscHeroSelectionRejectReason::DuplicateHeroBlocked;
        return false;
    }

    const EAscHeroRole HeroRole = ResolveHeroRole(HeroId);
    if (WouldViolateDuplicateRoleRule(AscPS, HeroRole, Rules))
    {
        OutRejectReason = EAscHeroSelectionRejectReason::DuplicateRoleBlocked;
        return false;
    }

    return true;
}

bool UAscHeroSelectionComponent::ConfirmHero(
    AAscenduntPlayerController* PlayerController,
    int32 HeroId,
    EAscHeroSelectionContext Context,
    const FAscHeroSelectionRules& Rules,
    EAscHeroSelectionRejectReason& OutRejectReason)
{
    if (!CanConfirmHero(PlayerController, HeroId, Rules, OutRejectReason))
    {
        return false;
    }

    AAscenduntPlayerState* AscPS = PlayerController ? PlayerController->GetPlayerState<AAscenduntPlayerState>() : nullptr;
    if (!AscPS)
    {
        OutRejectReason = EAscHeroSelectionRejectReason::InvalidPlayer;
        return false;
    }

    const EAscHeroRole HeroRole = ResolveHeroRole(HeroId);
    const UHeroPawnData* PawnData = ResolvePawnDataForHero(HeroId);

    AscPS->SetConfirmedHero(HeroId, HeroRole);

    // Both initial selection and respawn-room selection should update the
    // authoritative PawnData. Whether the current pawn is immediately replaced
    // is still a GameMode policy and will be wired in the next step.
    if (PawnData)
    {
        AscPS->SetPawnData(PawnData);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[HeroSelectionComponent] Confirmed Player=%s HeroId=%d Role=%d Context=%d PawnData=%s"),
        *GetNameSafe(AscPS),
        HeroId,
        static_cast<int32>(HeroRole),
        static_cast<int32>(Context),
        *GetNameSafe(PawnData));

    return true;
}

const FAscHeroDefinition* UAscHeroSelectionComponent::FindHeroDefinition(int32 HeroId) const
{
    return HeroRosterData ? HeroRosterData->FindHeroDefinition(HeroId) : nullptr;
}

bool UAscHeroSelectionComponent::IsHeroIdValid(int32 HeroId) const
{
    if (HeroId < 0 || !HeroRosterData || !HeroRosterData->HasHeroDefinitions())
    {
        return false;
    }

    return FindHeroDefinition(HeroId) != nullptr;
}

EAscHeroRole UAscHeroSelectionComponent::ResolveHeroRole(int32 HeroId) const
{
    if (const FAscHeroDefinition* Definition = FindHeroDefinition(HeroId))
    {
        return Definition->HeroRole;
    }

    return EAscHeroRole::MainDPS;
}

const UHeroPawnData* UAscHeroSelectionComponent::ResolvePawnDataForHero(int32 HeroId) const
{
    if (const FAscHeroDefinition* Definition = FindHeroDefinition(HeroId))
    {
        return Definition->PawnData;
    }

    return nullptr;
}

bool UAscHeroSelectionComponent::WouldViolateDuplicateHeroRule(
    const AAscenduntPlayerState* RequestingPlayerState,
    int32 HeroId,
    const FAscHeroSelectionRules& Rules) const
{
    if (Rules.bAllowDuplicateHeroes || !GetWorld() || !RequestingPlayerState)
    {
        return false;
    }

    const AGameStateBase* GameState = GetWorld()->GetGameState();
    if (!GameState)
    {
        return false;
    }

    for (APlayerState* PS : GameState->PlayerArray)
    {
        const AAscenduntPlayerState* OtherPS = Cast<AAscenduntPlayerState>(PS);
        if (!ShouldCompareForDuplicateRules(RequestingPlayerState, OtherPS, Rules))
        {
            continue;
        }

        if (OtherPS->GetConfirmedHeroId() == HeroId)
        {
            return true;
        }
    }

    return false;
}

bool UAscHeroSelectionComponent::WouldViolateDuplicateRoleRule(
    const AAscenduntPlayerState* RequestingPlayerState,
    EAscHeroRole HeroRole,
    const FAscHeroSelectionRules& Rules) const
{
    if (Rules.bAllowDuplicateHeroRoles || !GetWorld() || !RequestingPlayerState)
    {
        return false;
    }

    const AGameStateBase* GameState = GetWorld()->GetGameState();
    if (!GameState)
    {
        return false;
    }

    for (APlayerState* PS : GameState->PlayerArray)
    {
        const AAscenduntPlayerState* OtherPS = Cast<AAscenduntPlayerState>(PS);
        if (!ShouldCompareForDuplicateRules(RequestingPlayerState, OtherPS, Rules))
        {
            continue;
        }

        if (OtherPS->GetConfirmedHeroRole() == HeroRole)
        {
            return true;
        }
    }

    return false;
}

bool UAscHeroSelectionComponent::ShouldCompareForDuplicateRules(
    const AAscenduntPlayerState* RequestingPlayerState,
    const AAscenduntPlayerState* OtherPlayerState,
    const FAscHeroSelectionRules& Rules) const
{
    if (!RequestingPlayerState || !OtherPlayerState || RequestingPlayerState == OtherPlayerState || !OtherPlayerState->HasConfirmedHero())
    {
        return false;
    }

    if (Rules.bCheckDuplicateRulesWithinTeamOnly)
    {
        const uint8 RequestingTeamId = RequestingPlayerState->GetTeamId();
        const uint8 OtherTeamId = OtherPlayerState->GetTeamId();
        if (RequestingTeamId != 0 && OtherTeamId != 0 && RequestingTeamId != OtherTeamId)
        {
            return false;
        }
    }

    return true;
}
