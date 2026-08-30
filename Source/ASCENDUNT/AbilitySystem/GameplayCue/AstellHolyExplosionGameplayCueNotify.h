#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "AstellHolyExplosionGameplayCueNotify.generated.h"

class UNiagaraSystem;

/** Replicated Holy Explosion target burst. */
UCLASS(Abstract, Blueprintable)
class ASCENDUNT_API UAstellHolyExplosionGameplayCueNotify : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Astell|HolyExplosion|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> HolyExplosionVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Astell|HolyExplosion|VFX", meta = (AllowPrivateAccess = "true"))
	FName AttachSocketName = TEXT("StatusSocket_Chest");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Astell|HolyExplosion|VFX", meta = (AllowPrivateAccess = "true"))
	FVector RelativeLocation = FVector(0.0f, 0.0f, 20.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Astell|HolyExplosion|VFX", meta = (AllowPrivateAccess = "true"))
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Astell|HolyExplosion|VFX", meta = (AllowPrivateAccess = "true"))
	FVector RelativeScale = FVector::OneVector;
};
