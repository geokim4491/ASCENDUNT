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
 *   캐릭터의 초기화 관리
 *
 *   PS, PawnData, 무기 준비 등의 이벤트가 발생할 때마다 Evaluate() 진행
 *
 *   Spawned          : PS, PawnData 준비
 *   GASInitialized   : ASC 초기화 및 AbilitySet 적용
 *   GameplayReady    : 기타 초기화, 로컬 플레이어의 입력과 카메라 연결
 *
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UHeroInitComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    void NotifyPlayerStateChanged();

    void NotifyControllerChanged();

    void NotifyInputComponentReady();


    void DeinitializeForPawnReplacement(bool bPreserveHeroState = false);

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

    bool TryEnterGASInitialized();

    void PrepareGameplayDependencies();
    bool CanEnterGameplayReady() const;
    bool TryEnterGameplayReady();

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

    void HandleCurrentWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PrevWeapon);

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

    // These flags describe initialization side effects, not replicated data readiness.
    bool bAnimationInitialized = false;
    bool bHealthCompInitialized = false;
    bool bOnSpawnAbilitiesActivatedForAvatar = false;

    FDelegateHandle PawnDataAvailHandle;
    FDelegateHandle PawnDataAppliedHandle;
    FDelegateHandle CurrentWeaponChangedHandle;
    FDelegateHandle WeaponInventoryChangedHandle;
};
