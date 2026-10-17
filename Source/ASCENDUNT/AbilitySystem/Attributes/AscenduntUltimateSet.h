#pragma once

#include "CoreMinimal.h"
#include "AscenduntAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AscenduntUltimateSet.generated.h"

/**
 * Persistent player ultimate resource.
 * Lives on PlayerState with the ASC so charge survives Pawn replacement for the same hero.
 */
UCLASS()
class ASCENDUNT_API UAscenduntUltimateSet : public UAscenduntAttributeSet
{
	GENERATED_BODY()

public:
	UAscenduntUltimateSet();

	ATTRIBUTE_ACCESSORS(UAscenduntUltimateSet, UltimateCharge);
	ATTRIBUTE_ACCESSORS(UAscenduntUltimateSet, MaxUltimateCharge);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	UFUNCTION()
	void OnRep_UltimateCharge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxUltimateCharge(const FGameplayAttributeData& OldValue);

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_UltimateCharge, Category = "Ascendunt|Ultimate", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData UltimateCharge;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxUltimateCharge, Category = "Ascendunt|Ultimate", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxUltimateCharge;
};
