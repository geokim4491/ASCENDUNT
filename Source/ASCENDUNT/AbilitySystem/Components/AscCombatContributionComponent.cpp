#include "AscCombatContributionComponent.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntUltimateSet.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

#include "Net/UnrealNetwork.h"

UAscCombatContributionComponent::UAscCombatContributionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAscCombatContributionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAscCombatContributionComponent, TotalDamageDealt);
	DOREPLIFETIME(UAscCombatContributionComponent, TotalHealingDone);
	DOREPLIFETIME(UAscCombatContributionComponent, TotalHealingReceived);
	DOREPLIFETIME(UAscCombatContributionComponent, TotalKills);
	DOREPLIFETIME(UAscCombatContributionComponent, DownedCount);
	DOREPLIFETIME(UAscCombatContributionComponent, ReviveCount);
	DOREPLIFETIME(UAscCombatContributionComponent, WeaponShotsFired);
	DOREPLIFETIME(UAscCombatContributionComponent, WeaponHits);
	DOREPLIFETIME(UAscCombatContributionComponent, CriticalWeaponHits);
}

void UAscCombatContributionComponent::InitializeForPawnData(const UHeroPawnData* PawnData, UAscenduntAbilitySystemComponent* ASC)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !PawnData || !ASC)
	{
		return;
	}

	const bool bHeroChanged = ConfiguredPawnData != PawnData;
	ConfiguredPawnData = PawnData;
	CachedASC = ASC;

	DamageChargePerPoint = FMath::Max(0.0f, PawnData->UltimateCharge.DamageChargePerPoint);
	HealingChargePerPoint = FMath::Max(0.0f, PawnData->UltimateCharge.HealingChargePerPoint);

	const float MaxCharge = FMath::Max(1.0f, PawnData->UltimateCharge.MaxCharge);
	ASC->SetNumericAttributeBase(UAscenduntUltimateSet::GetMaxUltimateChargeAttribute(), MaxCharge);

	if (bHeroChanged)
	{
		// Match contribution totals are player-wide statistics. Only hero-specific ultimate resource resets on a hero swap.
		ASC->SetNumericAttributeBase(UAscenduntUltimateSet::GetUltimateChargeAttribute(), 0.0f);
	}
	else
	{
		const float CurrentCharge = ASC->GetNumericAttribute(UAscenduntUltimateSet::GetUltimateChargeAttribute());
		ASC->SetNumericAttributeBase(
			UAscenduntUltimateSet::GetUltimateChargeAttribute(),
			FMath::Clamp(CurrentCharge, 0.0f, MaxCharge));
	}
}

void UAscCombatContributionComponent::RecordDamageDealt(float ActualDamage, bool bKilledTarget)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ActualDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TotalDamageDealt += ActualDamage;
	OnDamageDealtNative.Broadcast(ActualDamage);
	OnTotalDamageDealtChanged.Broadcast(TotalDamageDealt);

	AddUltimateCharge(ActualDamage * DamageChargePerPoint);

	if (bKilledTarget)
	{
		++TotalKills;
		OnKillNative.Broadcast();
		OnTotalKillsChanged.Broadcast(TotalKills);
	}

	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::RecordHealingDone(float ActualHealing)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ActualHealing <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TotalHealingDone += ActualHealing;
	OnHealingDoneNative.Broadcast(ActualHealing);
	OnTotalHealingDoneChanged.Broadcast(TotalHealingDone);
	AddUltimateCharge(ActualHealing * HealingChargePerPoint);
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::RecordHealingReceived(float ActualHealing)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ActualHealing <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TotalHealingReceived += ActualHealing;
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::RecordDowned()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	++DownedCount;
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::RecordRevive()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	++ReviveCount;
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::RecordWeaponShotsFired(int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Count <= 0)
	{
		return;
	}

	WeaponShotsFired += Count;
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::RecordWeaponHit(bool bCriticalHit)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	++WeaponHits;
	if (bCriticalHit)
	{
		++CriticalWeaponHits;
	}
	BroadcastContributionChanged();
}

float UAscCombatContributionComponent::GetWeaponAccuracyPercent() const
{
	return WeaponShotsFired > 0
		? FMath::Clamp((static_cast<float>(WeaponHits) / static_cast<float>(WeaponShotsFired)) * 100.0f, 0.0f, 100.0f)
		: 0.0f;
}

float UAscCombatContributionComponent::GetCriticalHitAccuracyPercent() const
{
	return WeaponHits > 0
		? FMath::Clamp((static_cast<float>(CriticalWeaponHits) / static_cast<float>(WeaponHits)) * 100.0f, 0.0f, 100.0f)
		: 0.0f;
}

void UAscCombatContributionComponent::ResetContribution()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TotalDamageDealt = 0.0f;
	TotalHealingDone = 0.0f;
	TotalHealingReceived = 0.0f;
	TotalKills = 0;
	DownedCount = 0;
	ReviveCount = 0;
	WeaponShotsFired = 0;
	WeaponHits = 0;
	CriticalWeaponHits = 0;

	OnTotalDamageDealtChanged.Broadcast(TotalDamageDealt);
	OnTotalHealingDoneChanged.Broadcast(TotalHealingDone);
	OnTotalKillsChanged.Broadcast(TotalKills);
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::AddUltimateCharge(float Amount)
{
	UAscenduntAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC || Amount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Combat contribution statistics continue to accumulate during an ultimate,
	// but the ultimate must never charge itself from damage/healing it produces.
	if (ASC->HasMatchingGameplayTag(AscenduntGameplayTags::Status_Ultimate_Active))
	{
		return;
	}

	const float MaxCharge = FMath::Max(1.0f, ASC->GetNumericAttribute(UAscenduntUltimateSet::GetMaxUltimateChargeAttribute()));
	const float CurrentCharge = ASC->GetNumericAttribute(UAscenduntUltimateSet::GetUltimateChargeAttribute());
	ASC->SetNumericAttributeBase(
		UAscenduntUltimateSet::GetUltimateChargeAttribute(),
		FMath::Clamp(CurrentCharge + Amount, 0.0f, MaxCharge));
}

void UAscCombatContributionComponent::BroadcastContributionChanged()
{
	OnContributionChanged.Broadcast();
}

void UAscCombatContributionComponent::OnRep_TotalDamageDealt()
{
	OnTotalDamageDealtChanged.Broadcast(TotalDamageDealt);
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_TotalHealingDone()
{
	OnTotalHealingDoneChanged.Broadcast(TotalHealingDone);
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_TotalHealingReceived()
{
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_TotalKills()
{
	OnTotalKillsChanged.Broadcast(TotalKills);
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_DownedCount()
{
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_ReviveCount()
{
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_WeaponShotsFired()
{
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_WeaponHits()
{
	BroadcastContributionChanged();
}

void UAscCombatContributionComponent::OnRep_CriticalWeaponHits()
{
	BroadcastContributionChanged();
}
