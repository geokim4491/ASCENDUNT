#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AscTargetAwarenessComponent.generated.h"

class ABaseCharacter;

USTRUCT(BlueprintType)
struct FAscObservedTargetEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ABaseCharacter> Target = nullptr;

	UPROPERTY(BlueprintReadOnly)
	float RevealEndServerTime = 0.0f;
};

DECLARE_MULTICAST_DELEGATE(FAscObservedTargetsChanged);

/**
 * Server-authoritative per-player target awareness.
 *
 * The component exists on PlayerController and replicates only to its owning client.
 * It intentionally sends reveal permission/end time rather than trusting a client hit request.
 */
UCLASS(ClassGroup = (ASCENDUNT), meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UAscTargetAwarenessComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscTargetAwarenessComponent();

	/** Called only after the server has applied real damage to Target. */
	void GrantTargetReveal(ABaseCharacter* Target, float DurationSeconds = -1.0f);

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Target Awareness")
	bool IsTargetRevealed(const ABaseCharacter* Target) const;

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Target Awareness")
	float GetTargetRevealRemainingTime(const ABaseCharacter* Target) const;

	FAscObservedTargetsChanged OnObservedTargetsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_ObservedTargets();

	void PruneExpiredTargets();
	float GetSynchronizedServerTime() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Target Awareness", meta = (ClampMin = "0.1"))
	float DefaultRevealDuration = 5.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ObservedTargets)
	TArray<FAscObservedTargetEntry> ObservedTargets;

	FTimerHandle PruneTimerHandle;
};
