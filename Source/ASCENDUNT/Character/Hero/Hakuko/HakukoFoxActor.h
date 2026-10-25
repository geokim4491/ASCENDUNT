#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoTypes.h"
#include "HakukoFoxActor.generated.h"

class UNiagaraComponent;
class USceneComponent;
class USkeletalMeshComponent;

/**
 * Replicated visual actor for one Hakuko fox.
 *
 * The fox has no collision, does not sweep, and intentionally travels through walls.
 * It only follows a replicated actor target with a fixed local offset.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AHakukoFoxActor : public AActor
{
	GENERATED_BODY()

public:
	AHakukoFoxActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeFox(EHakukoFoxType InFoxType, AActor* InFollowTarget);
	void SetFollowTarget(AActor* NewFollowTarget);

	UFUNCTION(BlueprintPure, Category = "Hakuko|Fox")
	EHakukoFoxType GetFoxType() const { return FoxType; }

	UFUNCTION(BlueprintPure, Category = "Hakuko|Fox")
	AActor* GetFollowTarget() const { return FollowTarget; }

	UFUNCTION(BlueprintPure, Category = "Hakuko|Fox")
	UNiagaraComponent* GetAuraComponent() const { return AuraComponent; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_FollowTarget();

	FVector ResolveDesiredLocation() const;
	FRotator ResolveDesiredRotation() const;
	void SnapToFollowTarget();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hakuko|Fox")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hakuko|Fox")
	TObjectPtr<UNiagaraComponent> AuraComponent;

	/** Fixed offset relative to the followed character's yaw. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hakuko|Fox|Follow")
	FVector FollowOffset = FVector(0.0f, 120.0f, 105.0f);

	/** Normal follow smoothing after the fox reaches its assigned target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hakuko|Fox|Follow", meta = (ClampMin = "0.0"))
	float FollowInterpSpeed = 10.0f;

	/** Constant travel speed used when the fox is reassigned to another character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hakuko|Fox|Travel", meta = (ClampMin = "0.0"))
	float TransferTravelSpeed = 2200.0f;

	/** Distance at which transfer movement switches back to normal following. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hakuko|Fox|Travel", meta = (ClampMin = "0.0"))
	float TransferArrivalTolerance = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hakuko|Fox|Follow")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hakuko|Fox|Follow")
	bool bMatchTargetYaw = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Hakuko|Fox")
	EHakukoFoxType FoxType = EHakukoFoxType::Attack;

	UPROPERTY(ReplicatedUsing = OnRep_FollowTarget, BlueprintReadOnly, Category = "Hakuko|Fox")
	TObjectPtr<AActor> FollowTarget;

private:
	bool bLocationInitialized = false;
	bool bTravellingToFollowTarget = false;
};
