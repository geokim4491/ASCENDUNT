#pragma once
#include "CoreMinimal.h"
#include "AscenduntGameplayCueNotify_Burst.h"
#include "AscWeaponFireGameplayCueNotify_Burst.generated.h"

UCLASS(Blueprintable)
class ASCENDUNT_API UAscWeaponFireGameplayCueNotify_Burst : public UAscenduntGameplayCueNotify_Burst
{
	GENERATED_BODY()

public:
	UAscWeaponFireGameplayCueNotify_Burst();

protected:
	virtual bool ExecuteCueNative(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
