// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"

#include "CoreMinimal.h"
#include "ASCENDUNT/Character/BaseCharacter.h"
#include "ASCENDUNT/Interaction/AscInteractable.h"
#include "HeroCharacter.generated.h"

class AAscenduntPlayerState;
class UAbilitySystemComponent;
class UAscenduntAbilitySystemComponent;
class UHeroInputComponent;
class UHeroCameraComponent;
class UHeroFirstPersonComponent;
class UAscenduntCameraComponent;
class UHeroInitComponent;
class UAscenduntHealthComponent;
class UAscPvEDownedComponent;
class UAscInteractionComponent;

class UWeaponEquipmentComponent;

class UWidgetComponent;
class UUserWidget;
class UAnimInstance;
class UAnimMontage;
struct FOnAttributeChangeData;

/**
 * 
 */
UCLASS()
class ASCENDUNT_API AHeroCharacter : public ABaseCharacter, public IAbilitySystemInterface, public IAscInteractable
{
	GENERATED_BODY()
	
public:
	AHeroCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Implement IAbilitySystemInterface
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character")
	UAscenduntAbilitySystemComponent* GetAscenduntAbilitySystemComponent() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character")
	AAscenduntPlayerController* GetAscenduntPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character")
	AAscenduntPlayerState* GetAscenduntPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character")
	UHeroInputComponent* GetHeroInputComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character")
	UHeroCameraComponent* GetHeroCameraComponent() const;

	/**
	 * 1인칭 전용 메시. CameraComponent 에 부착되어 최종 카메라 트랜스폼을 그대로 따라간다.
	 * 3P 메시(GetMesh())와 스켈레톤이 달라도 무방하다.
	 *
	 * 가시성과 렌더 상태는 UHeroFirstPersonComponent 가 단독으로 소유한다.
	 * 다른 코드에서 SetVisibility / FirstPersonPrimitiveType 을 직접 만지지 말 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|FirstPerson")
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|FirstPerson")
	UHeroFirstPersonComponent* GetFirstPersonComponent() const { return FirstPersonComponent; }

	/** Presentation-only montage path for server-authoritative abilities.
	 *  Every client plays the 3P body montage; only the owning client also plays the 1P arms montage.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAbilityPresentationMontage(UAnimMontage* Montage3P, UAnimMontage* Montage1P, float PlayRate, FName StartSection);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character")
	UWeaponEquipmentComponent* GetWeaponEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Character")
	bool IsGameplayReady() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|PvE|Downed")
	UAscPvEDownedComponent* GetPvEDownedComponent() const { return PvEDownedComponent; }

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Interaction")
	UAscInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	// IAscInteractable: heroes only become valid targets while PvE Downed.
	virtual bool CanInteract_Implementation(AHeroCharacter* Interactor) const override;
	virtual FAscInteractionOption GetInteractionOption_Implementation(AHeroCharacter* Interactor) const override;
	virtual void BeginInteraction_Implementation(AHeroCharacter* Interactor) override;
	virtual void CancelInteraction_Implementation(AHeroCharacter* Interactor) override;
	virtual void CompleteInteraction_Implementation(AHeroCharacter* Interactor) override;

	/** Applies/removes the movement-speed override used while the PvE hero is Downed. */
	void SetDownedMovementOverride(bool bEnabled, float CrawlSpeed);

	// High-level Pawn replacement hook. Actual teardown order is owned by HeroInitComponent.
	void DeinitializeForPawnReplacement(bool bPreserveHeroState = false);

	/** Immediately disables capsule movement and enables mesh ragdoll. Safe to call repeatedly. */
	void EnterDeathRagdoll();

	/** 1인칭 판정에 쓰인다. UHeroFirstPersonComponent 참고. */
	bool IsDeathRagdollActive() const { return bDeathRagdollActive; }

	// Initializes the main animation instance and links the configured anim layer once.
	// This is intentionally driven by HeroInitComponent instead of BeginPlay so that
	// animation readiness becomes part of the pawn initialization state machine.
	bool TryInitializeAnimationForGameplay();

	bool IsAnimationInitializedForGameplay() const { return bAnimationInitializedForGameplay; }

	/** 입력 활성화 전에 이동 Attribute와 영웅별 게임플레이 기능을 연결한다. */
	virtual void InitializeGameplayBindings();

	/** 초기화 실패 또는 Pawn 교체 시 게임플레이 연결을 해제한다. */
	virtual void UninitializeGameplayBindings();

	// HealthComponent의 ASC 연결 후 구독하고, 연결 해제 전에 구독을 해제한다.
	void BindHealthEvents();
	void UnbindHealthEvents();

	//~AActor interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of AActor interface

	virtual bool SupportsEnemyOverlay() const override { return true; }
	virtual bool SupportsXRay() const override { return true; }

	// 아래 두 함수 같은 역할임.
	virtual void GatherVisionMeshes(TArray<UMeshComponent*>& OutMeshes) const override;
	virtual void GatherXRayMeshes(TArray<UMeshComponent*>& OutMeshes) const override;
protected:

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;


protected:
	UPROPERTY()
	mutable TObjectPtr<AAscenduntPlayerState> CachedPS;

	UPROPERTY()
	mutable TObjectPtr<UAscenduntAbilitySystemComponent> CachedASC;

	// AbilitySystem avatar가 준비되었을 때(ASC ActorInfo 완료) 호출
	FSimpleMulticastDelegate OnASCInitialized;

	// 해제될 때 호출
	FSimpleMulticastDelegate OnASCUninitialized;
	bool bASCInitialized = false;

	// 디테일 패널에서 어떤 레이어 ABP를 쓸지 선택하는 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TSubclassOf<UAnimInstance> AnimLayerClass;

	// 레이어를 실제로 적용하는 함수. TryInitializeAnimationForGameplay() 안에서만 호출한다.
	bool LinkAnimClassLayers();

private:
	void UnbindMovementAttributes();
	void RefreshMovementFromAttributes();
	void HandleBaseMoveSpeedChanged(const FOnAttributeChangeData& ChangeData);
	void HandleCrouchMoveSpeedScalarChanged(const FOnAttributeChangeData& ChangeData);
	void HandleJumpZVelocityChanged(const FOnAttributeChangeData& ChangeData);
	void HandleAirControlChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	UFUNCTION()
	void HandleDeathFinished(AActor* OwningActor);

	bool bAnimationInitializedForGameplay = false;
	bool bDeathEventBound = false;
	bool bDeathRagdollActive = false;
	bool bDownedMovementOverride = false;
	float DownedMovementSpeedOverride = 0.0f;

	TWeakObjectPtr<UAscenduntAbilitySystemComponent> MovementAttributeASC;
	FDelegateHandle BaseMoveSpeedChangedHandle;
	FDelegateHandle CrouchMoveSpeedScalarChangedHandle;
	FDelegateHandle JumpZVelocityChangedHandle;
	FDelegateHandle AirControlChangedHandle;

	UPROPERTY(EditAnywhere, Category = "Ascendunt|Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeroInputComponent> HeroInputComponent;

	UPROPERTY(EditAnywhere, Category = "Ascendunt|Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscenduntCameraComponent> CameraComponent;

	// 1인칭 전용 메시. 스켈레탈 메시와 AnimClass 는 히어로 BP 에서 지정한다.
	// 지정하지 않은 히어로는 UHeroFirstPersonComponent 가 조용히 3인칭으로 동작시킨다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|FirstPerson", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	// 1인칭 렌더 상태를 매 프레임 조정한다. 소유 프로퍼티 목록은 헤더 주석 참고.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|FirstPerson", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeroFirstPersonComponent> FirstPersonComponent;

	UPROPERTY(EditAnywhere, Category = "Ascendunt|Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeroCameraComponent> HeroCameraComponent;

	UPROPERTY(EditAnywhere, Category = "Ascendunt|Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHeroInitComponent> HeroInitComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|PvE|Downed", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscPvEDownedComponent> PvEDownedComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscInteractionComponent> InteractionComponent;

	UPROPERTY(EditAnywhere, Category = "Ascendunt|Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWeaponEquipmentComponent> WeaponEquipmentComponent;
};
