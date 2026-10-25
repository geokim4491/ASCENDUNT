#include "AscenduntUltimateSet.h"

#include "Net/UnrealNetwork.h"

UAscenduntUltimateSet::UAscenduntUltimateSet()
{
	InitUltimateCharge(0.0f);
	InitMaxUltimateCharge(100.0f);
}

void UAscenduntUltimateSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntUltimateSet, UltimateCharge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntUltimateSet, MaxUltimateCharge, COND_None, REPNOTIFY_Always);
}

void UAscenduntUltimateSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxUltimateChargeAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
	else if (Attribute == GetUltimateChargeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(1.0f, GetMaxUltimateCharge()));
	}
}

void UAscenduntUltimateSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetMaxUltimateChargeAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
	else if (Attribute == GetUltimateChargeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, FMath::Max(1.0f, GetMaxUltimateCharge()));
	}
}

void UAscenduntUltimateSet::OnRep_UltimateCharge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntUltimateSet, UltimateCharge, OldValue);
}

void UAscenduntUltimateSet::OnRep_MaxUltimateCharge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntUltimateSet, MaxUltimateCharge, OldValue);
}
