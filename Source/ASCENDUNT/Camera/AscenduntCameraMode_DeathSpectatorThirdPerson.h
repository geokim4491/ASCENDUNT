// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode_ThirdPerson.h"
#include "AscenduntCameraMode_DeathSpectatorThirdPerson.generated.h"

/**
 * Third-person camera used while the owning player is waiting to respawn.
 *
 * - A living allied target is followed from its normal upper-body camera pivot.
 * - A ragdoll target is followed from its pelvis, but the ragdoll's rotation is ignored.
 * - View rotation always comes from the local spectating PlayerController, keeping Roll at zero.
 */
UCLASS()
class ASCENDUNT_API UAscenduntCameraMode_DeathSpectatorThirdPerson : public UAscenduntCameraMode_ThirdPerson
{
	GENERATED_BODY()

public:
	UAscenduntCameraMode_DeathSpectatorThirdPerson();

	virtual void OnActivation() override;
	virtual void OnDeactivation() override;

protected:
	virtual void UpdateView(float DeltaTime) override;

private:
	FVector ResolveDesiredPivotLocation(bool& bOutRagdollTarget) const;
	FRotator ResolveSpectatorViewRotation() const;
	class APlayerController* FindLocalViewingController() const;

private:
	/** Socket/bone used while viewing a living ally. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathSpectator|Pivot")
	FName LivingPivotSocketName;

	/** Local-space offset from LivingPivotSocketName. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathSpectator|Pivot")
	FVector LivingPivotSocketOffset;

	/** Bone/socket followed while the target mesh is ragdolling. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathSpectator|Pivot")
	FName RagdollPivotBoneName;

	/** Local-space offset from RagdollPivotBoneName. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathSpectator|Pivot")
	FVector RagdollPivotBoneOffset;

	/** Camera offset used for a living allied target, in spectator-view space. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathSpectator|Camera")
	FVector LivingViewSpaceCameraOffset;

	/** Camera offset used for the player's ragdoll corpse, in spectator-view space. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathSpectator|Camera")
	FVector RagdollViewSpaceCameraOffset;

	/** Smooths pelvis movement caused by ragdoll simulation. Zero disables interpolation. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathSpectator|Camera", meta = (ClampMin = "0.0"))
	float RagdollPivotInterpSpeed;

	FVector SmoothedPivotLocation = FVector::ZeroVector;
	bool bHasSmoothedPivot = false;
};
