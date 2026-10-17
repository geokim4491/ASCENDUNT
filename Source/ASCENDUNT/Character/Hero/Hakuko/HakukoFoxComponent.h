#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoTypes.h"
#include "HakukoFoxComponent.generated.h"

class AAscenduntPlayerState;
class AHakukoCharacter;
class AHakukoFoxActor;
class UAbilitySystemComponent;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHakukoSelectedFoxChanged, EHakukoFoxType, SelectedFoxType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHakukoTargetChanged, AActor*, TargetActor, EHakukoFoxType, SelectedFoxType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHakukoFoxAssignmentChanged, EHakukoFoxType, FoxType, AActor*, AssignedTarget);

/**
 * Owns Hakuko's two-fox gameplay state.
 *
 * Local client responsibilities:
 * - mouse-wheel fox selection
 * - editable cone targeting and debug drawing
 * - local target state publication through OnTargetChanged
 *
 * Server responsibilities:
 * - gameplay-effect transfer
 * - fox visual actor spawning / follow-target changes
 * - invalid or dead target recall
 * - global ultimate application
 */
UCLASS(ClassGroup = (ASCENDUNT), meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UHakukoFoxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHakukoFoxComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeForGameplay();
	void DeinitializeForGameplay();

	/** Selects the fox used by the E ability. Local-only UI state. */
	void SelectFox(EHakukoFoxType NewFoxType);

	/** Called by the E ability. No target recalls the currently selected fox. */
	void RequestSendSelectedFox();

	/** Called by the ultimate ability on the authoritative instance. */
	void ActivateGlobalUltimate();

	UFUNCTION(BlueprintPure, Category = "Hakuko|Fox")
	EHakukoFoxType GetSelectedFoxType() const { return SelectedFoxType; }

	UFUNCTION(BlueprintPure, Category = "Hakuko|Fox")
	AActor* GetLocalTargetActor() const { return LocalTargetActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Hakuko|Fox")
	FText GetSelectedFoxDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Hakuko|Fox")
	AActor* GetAssignedTarget(EHakukoFoxType FoxType) const;

	UPROPERTY(BlueprintAssignable, Category = "Hakuko|Fox")
	FOnHakukoSelectedFoxChanged OnSelectedFoxChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hakuko|Fox")
	FOnHakukoTargetChanged OnTargetChanged;

	/** Owning-client HUD notification for replicated normal-fox assignments. */
	UPROPERTY(BlueprintAssignable, Category = "Hakuko|Fox")
	FOnHakukoFoxAssignmentChanged OnFoxAssignmentChanged;

protected:
	UFUNCTION(Server, Reliable)
	void ServerAssignSelectedFox(EHakukoFoxType FoxType, AActor* RequestedTarget);

	UFUNCTION()
	void HandleOwnerDeathStarted(AActor* OwningActor);

	UFUNCTION()
	void OnRep_AttackFoxTarget();

	UFUNCTION()
	void OnRep_SupportFoxTarget();

private:
	AHakukoCharacter* GetHakukoOwner() const;
	AAscenduntPlayerState* GetOwnerPlayerState() const;
	bool ShouldRunLocalTargeting() const;

	void UpdateLocalTarget();
	void SetLocalTarget(AActor* NewTarget);
	AActor* FindBestFriendlyTarget(FVector& OutViewLocation, FVector& OutViewForward);
	bool IsCandidateVisible(const FVector& ViewLocation, AActor* Candidate) const;
	void DrawTargetingDebug(const FVector& ViewLocation, const FVector& ViewForward, const TArray<AActor*>& Candidates,
		AActor* BestTarget, const TSet<TWeakObjectPtr<AActor>>& BlockedCandidates) const;

	void SpawnPersistentFoxes_Server();
	AHakukoFoxActor* SpawnFoxActor_Server(TSubclassOf<AHakukoFoxActor> FoxClass, EHakukoFoxType FoxType,
		AActor* FollowTarget, float LifeSpanSeconds = 0.0f);
	void DestroyPersistentFoxes_Server();

	bool IsValidRecipient_Server(AActor* TargetActor, bool bAllowOwner) const;
	bool IsTargetAlive(AActor* TargetActor) const;
	void ValidateAssignedTargets_Server();
	void AssignFox_Server(EHakukoFoxType FoxType, AActor* TargetActor);
	void RemoveFoxEffect_Server(EHakukoFoxType FoxType);
	void ApplyFoxEffect_Server(EHakukoFoxType FoxType, AActor* TargetActor);
	void RecallFox_Server(EHakukoFoxType FoxType);
	void ClearAllFoxEffects_Server();

	void GetNormalEffectClasses(EHakukoFoxType FoxType, TArray<TSubclassOf<UGameplayEffect>>& OutEffectClasses) const;
	void GetUltimateEffectClasses(EHakukoFoxType FoxType, TArray<TSubclassOf<UGameplayEffect>>& OutEffectClasses) const;
	TSubclassOf<AHakukoFoxActor> GetFoxActorClass(EHakukoFoxType FoxType) const;
	AHakukoFoxActor* GetPersistentFoxActor(EHakukoFoxType FoxType) const;
	void SetPersistentFoxActor(EHakukoFoxType FoxType, AHakukoFoxActor* FoxActor);

	bool ApplyUltimateEffectToTarget_Server(AActor* TargetActor, EHakukoFoxType FoxType);

protected:
	// ---------- Fox classes / effects ----------
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Fox|Visual")
	TSubclassOf<AHakukoFoxActor> AttackFoxActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Fox|Visual")
	TSubclassOf<AHakukoFoxActor> SupportFoxActorClass;

	/** Infinite GE. Expected to modify DamageDealtScalar and RateOfFire. */
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Fox|Effects")
	TSubclassOf<UGameplayEffect> AttackFoxEffectClass;

	/** Infinite non-periodic GE. Expected to modify BaseMoveSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Fox|Effects")
	TSubclassOf<UGameplayEffect> SupportFoxEffectClass;

	/** Infinite periodic GE. Expected to add to HealthSet.Healing each period. */
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Fox|Effects")
	TSubclassOf<UGameplayEffect> SupportFoxPeriodicHealEffectClass;

	/** Duration GE used by the global ultimate. */
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Ultimate")
	TSubclassOf<UGameplayEffect> UltimateAttackFoxEffectClass;

	/** Duration non-periodic movement GE used by the global ultimate. */
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Ultimate")
	TSubclassOf<UGameplayEffect> UltimateSupportFoxEffectClass;

	/** Duration periodic healing GE used by the global ultimate. */
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Ultimate")
	TSubclassOf<UGameplayEffect> UltimateSupportFoxPeriodicHealEffectClass;

	/** Lifespan for temporary ultimate fox visuals. Set this to the GE duration. */
	UPROPERTY(EditDefaultsOnly, Category = "Hakuko|Ultimate", meta = (ClampMin = "0.0"))
	float UltimateVisualDuration = 5.0f;

	// ---------- Targeting ----------
	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting", meta = (ClampMin = "0.0"))
	float TargetingRange = 3000.0f;

	/** Half-angle from the camera centerline. 8 means a 16-degree full cone. */
	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting", meta = (ClampMin = "0.1", ClampMax = "89.0"))
	float TargetingHalfAngleDegrees = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting")
	FVector TargetPointOffset = FVector(0.0f, 0.0f, 70.0f);

	/** Additional server tolerance for client/server position differences. */
	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting", meta = (ClampMin = "0.0"))
	float ServerRangeTolerance = 250.0f;

	// ---------- Debug ----------
	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting|Debug")
	bool bDrawDebugTargetCone = false;

	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting|Debug", meta = (EditCondition = "bDrawDebugTargetCone", ClampMin = "4"))
	int32 DebugConeSegments = 24;

	UPROPERTY(EditAnywhere, Category = "Hakuko|Targeting|Debug", meta = (EditCondition = "bDrawDebugTargetCone", ClampMin = "0.1"))
	float DebugLineThickness = 1.5f;

private:
	UPROPERTY(Transient)
	EHakukoFoxType SelectedFoxType = EHakukoFoxType::Attack;

	TWeakObjectPtr<AActor> LocalTargetActor;

	UPROPERTY(ReplicatedUsing = OnRep_AttackFoxTarget, Transient)
	TObjectPtr<AActor> AttackFoxTarget;

	UPROPERTY(ReplicatedUsing = OnRep_SupportFoxTarget, Transient)
	TObjectPtr<AActor> SupportFoxTarget;

	UPROPERTY(Transient)
	TObjectPtr<AHakukoFoxActor> AttackFoxActor;

	UPROPERTY(Transient)
	TObjectPtr<AHakukoFoxActor> SupportFoxActor;

	TArray<FActiveGameplayEffectHandle> AttackFoxEffectHandles;
	TArray<FActiveGameplayEffectHandle> SupportFoxEffectHandles;
	TWeakObjectPtr<UAbilitySystemComponent> AttackFoxEffectASC;
	TWeakObjectPtr<UAbilitySystemComponent> SupportFoxEffectASC;

	bool bGameplayInitialized = false;
};
