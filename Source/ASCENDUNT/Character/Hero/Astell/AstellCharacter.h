#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "AstellCharacter.generated.h"

class UAscSkillStockComponent;
class UAnimMontage;
class UNiagaraSystem;

UCLASS()
class ASCENDUNT_API AAstellCharacter : public AHeroCharacter
{
	GENERATED_BODY()

public:
	AAstellCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Astell")
	UAscSkillStockComponent* GetSkillStockComponent() const { return SkillStockComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void OnJumped_Implementation() override;

private:
	void ApplyDoubleJumpLaunch();
	void PlayDoubleJumpCosmeticsLocal(const FVector& VFXLocation, const FRotator& VFXRotation);
	FVector GetDoubleJumpVFXLocation() const;
	FRotator GetDoubleJumpVFXRotation() const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDoubleJumpCosmetics(FVector_NetQuantize10 VFXLocation, FRotator VFXRotation);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Astell", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscSkillStockComponent> SkillStockComponent;

	// Played only when the second jump is actually accepted by ACharacter.
	// Keep this montage in-place. Character movement is driven by DoubleJumpVerticalVelocity.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Astell|DoubleJump", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DoubleJumpMontage;

	// Replaces the current vertical velocity when the second jump succeeds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Astell|DoubleJump", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float DoubleJumpVerticalVelocity = 850.0f;

	// One-shot platform-like effect spawned at the feet when the second jump succeeds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Astell|DoubleJump|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> DoubleJumpVFX;

	// Optional mesh socket used as the VFX origin. NAME_None or a missing socket falls back to the capsule bottom.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Astell|DoubleJump|VFX", meta = (AllowPrivateAccess = "true"))
	FName DoubleJumpVFXSocketName = NAME_None;

	// World origin is resolved first, then this local actor-space offset is applied.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Astell|DoubleJump|VFX", meta = (AllowPrivateAccess = "true"))
	FVector DoubleJumpVFXLocationOffset = FVector::ZeroVector;
};
