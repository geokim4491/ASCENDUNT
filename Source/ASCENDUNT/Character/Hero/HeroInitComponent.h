// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HeroInitComponent.generated.h"

class AAscenduntPlayerState;
class AAscenduntWeapon;
class AHeroCharacter;
class UAscenduntAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameplayReady, APawn*, Owner);

/**
 * Coordinates Hero initialization without assuming replication/lifecycle order.
 *
 * State contract:
 *   None
 *    -> Spawned          : Pawn + PlayerState + matching PawnData identify the Hero.
 *    -> GASInitialized  : ASC ActorInfo is bound to this Pawn and PawnData gameplay data is applied.
 *    -> GameplayReady    : gameplay-facing dependencies are prepared; local input/camera are committed last.
 *
 * Every asynchronous dependency owns an event that calls Evaluate().  There is intentionally
 * no retry timer: if a prerequisite can become ready later, its change path must wake this component.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UHeroInitComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Pawn lifecycle entry point. Safe before this component's BeginPlay. */
    void HandlePlayerStateChanged(AAscenduntPlayerState* NewPS);

    /** Called when replicated/local possession state may have changed. */
    void NotifyControllerChanged();

    /** Called from SetupPlayerInputComponent after the engine creates the local InputComponent. */
    void NotifyInputComponentReady();

    /** Called by the owning Pawn before UnPossess/Destroy. Safe to call repeatedly. */
    void DeinitializeForPawnReplacement(bool bPreserveHeroState = false);

    /** EndPlay fallback for destroy paths that did not explicitly request replacement teardown. */
    void Shutdown();

    bool IsGameplayReady() const;
    const FGameplayTag& GetCurrentState() const { return CurrentState; }

protected:
    virtual void BeginPlay() override;

private:
    // ----- State machine -----
    void Evaluate();
    void RefreshDependencies();

    bool CanEnterSpawned() const;
    void EnterSpawned();

    bool TryEnterGASInitialized();

    void PrepareGameplayDependencies();
    bool CanEnterGameplayReady() const;
    bool EnterGameplayReady();

    // ----- Dependency/configuration helpers -----
    bool ValidateStaticConfiguration();
    bool RequiresLocalPlayerInitialization() const;
    bool CanInitializeLocalCamera() const;
    bool InitializeLocalCamera();

    // ----- Event bindings -----
    void BindToPlayerState(AAscenduntPlayerState* PS);
    void UnbindFromPlayerState();

    void BindToWeaponEquipment();
    void UnbindFromWeaponEquipment();

    void BindToHealthComponent();
    void UnbindFromHealthComponent();

    void HandlePawnDataAvailable();
    void HandlePawnDataApplied();
    void HandleCurrentWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PrevWeapon);
    void HandleWeaponInventoryChanged();

    UFUNCTION()
    void HandleDeathStarted(AActor* OwningActor);

    UFUNCTION()
    void HandleDeathFinished(AActor* OwningActor);

    // ----- Teardown -----
    void DeinitializePawn(bool bPreserveHeroState);
    void ResetRuntimeState();

protected:
    UPROPERTY(BlueprintAssignable)
    FOnGameplayReady OnGameplayReady;

private:
    FGameplayTag CurrentState;

    TWeakObjectPtr<AHeroCharacter> OwnerPawn;
    TWeakObjectPtr<AAscenduntPlayerState> CachedPS;
    TWeakObjectPtr<UAscenduntAbilitySystemComponent> CachedASC;

    bool bDeinitialized = false;
    bool bStaticConfigurationChecked = false;
    bool bStaticConfigurationValid = false;

    // These flags describe initialization side effects, not replicated data readiness.
    bool bAnimationInitialized = false;
    bool bHealthCompInitialized = false;
    bool bInputCompInitialized = false;
    bool bCameraInitialized = false;
    bool bOnSpawnAbilitiesActivatedForAvatar = false;
    bool bDeathEventBound = false;

    FDelegateHandle PawnDataAvailHandle;
    FDelegateHandle PawnDataAppliedHandle;
    FDelegateHandle CurrentWeaponChangedHandle;
    FDelegateHandle WeaponInventoryChangedHandle;
};
