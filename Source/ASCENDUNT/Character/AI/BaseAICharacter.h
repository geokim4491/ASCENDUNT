// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySet.h"

#include "CoreMinimal.h"
#include "ASCENDUNT/Character/BaseCharacter.h"
#include "BaseAICharacter.generated.h"

class UAIInitComponent;
class UAIPawnData;
class UAnimMontage;
class USkeletalMeshComponent;
class UAscenduntAbilitySystemComponent;
class UAscenduntHealthSet;
class UAscenduntWeaponSet;
class UAscenduntHealthComponent;
class UWeaponEquipmentComponent;

class ABaseAICharacter;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAscPooledActiveStateChangedNative, ABaseAICharacter* /*Character*/, bool /*bActive*/);

/**
 * 
 */
UCLASS()
class ASCENDUNT_API ABaseAICharacter : public ABaseCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ABaseAICharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UAscenduntAbilitySystemComponent* GetAscenduntAbilitySystemComponent() const { return ASC; }
	UAscenduntHealthComponent* GetAscenduntHealthComponent() const { return HealthComponent; }
	UWeaponEquipmentComponent* GetWeaponEquipmentComponent() const { return WeaponEquipmentComponent; }

	// 데이터 에셋을 읽어와 캐릭터의 외형, 스탯, 스킬을 세팅하는 함수
	void InitializeFromPawnData();

	// 컨트롤러가 캐릭터의 데이터를 읽어갈 수 있도록 하는 공개 함수
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Data")
	UAIPawnData* GetPawnData() const { return PawnData; }

	// 풀링을 위해 외부에서 데이터를 교체할 수 있도록 제공
	void SetPawnData(UAIPawnData* InPawnData);

	/**
	 * 풀에서 꺼낸 AI를 SpawnTransform 위치에 안전하게 재배치한다.
	 *
	 * SpawnActor의 AdjustIfPossibleButAlwaysSpawn을 거치지 않는 재사용 경로에서도
	 * 현재 Capsule의 실제 ScaledHalfHeight를 기준으로 바닥을 다시 잡는다.
	 * 따라서 AI별 Capsule 크기/스케일이 달라도 바닥 침투로 인한 CMC depenetration
	 * 진동이 발생하지 않는다. 호출 시점에는 AI가 아직 비활성 상태여야 한다.
	 */
	void PrepareForPoolReuse(const FTransform& SpawnTransform);

	// PawnData의 AbilitySet을 ASC에 부여/회수한다.
	// 호출 주체는 UAIInitComponent 하나뿐이다. InitState 상태 머신이 생애당 1회만
	// 실행하는 것을 보장하므로, 여기에 별도의 중복 방지 플래그를 두지 않는다.
	void GrantPawnDataAbilities();
	void RevokePawnDataAbilities();

	// 사망 후 풀로 돌아갈 때 초기화
	virtual void ResetForPool();

	// 가시성/충돌/틱을 한 번에 토글한다. 서버가 bPooledActive를 바꾸면
	// OnRep을 통해 클라이언트도 같은 처리를 한다.
	// AActor::bActorEnableCollision은 복제되지 않으므로, 이 경로가 없으면
	// 클라이언트에는 "보이지 않는데 단단한" 시체가 남는다.
	void SetPooledActive(bool bActive);

	// 풀 활성/비활성 전환 시 로컬 비주얼 시스템이 즉시 상태를 갱신하도록 알린다.
	FOnAscPooledActiveStateChangedNative OnPooledActiveStateChanged;

	/**
	 * 현재 콜스택을 빠져나간 뒤 HealthComponent::FinishDeath()를 호출한다.
	 *
	 * 사망 연출을 소유한 쪽(UGA_AIDeath 등)이 연출을 끝냈을 때 호출한다.
	 * 몽타주 종료 콜백 안에서 곧바로 FinishDeath를 부르면 풀 반납까지 이어지면서
	 * 같은 AnimInstance의 몽타주를 정지시키고 메시 틱을 꺼 애님 시스템에 재진입한다.
	 * 그래서 항상 다음 틱으로 미룬다. 중복 호출은 FinishDeath 내부 가드가 막는다.
	 */
	void FinishDeathDeferred();

	/**
	 * AI 전용 3P 몽타주 연출을 클라이언트에서만 재생한다.
	 * Dedicated Server는 AI Pose/AnimBP를 계산하지 않으므로 서버에서는 재생하지 않는다.
	 * 연속 사격/재장전/일반 연출처럼 유실되어도 Gameplay에 영향 없는 연출용이다.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayPresentationMontage(UAnimMontage* Montage, float PlayRate, FName StartSection);

	/** 사망 연출은 한 번뿐이고 누락 시 시각적으로 크게 보이므로 Reliable로 전달한다. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeathPresentationMontage(UAnimMontage* Montage, float PlayRate);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnDeathStarted(AActor* OwningActor);

	UFUNCTION()
	void OnDeathFinished(AActor* OwningActor);

	/**
	 * 시체가 플레이어를 막거나 총알을 흡수하지 않도록 상호작용 충돌을 토글한다.
	 *
	 * 끄는 곳은 OnDeathStarted, 켜는 곳은 ApplyPooledActiveState 하나뿐이다.
	 * 어빌리티(GA_AIDeath 등)에서 직접 충돌을 끄면 되돌리는 주체가 없어져서
	 * 다음 생애에 충돌이 꺼진 채로 되살아난다. 실제로 그 버그가 있었다.
	 */
	void SetInteractionCollisionEnabled(bool bEnabled);

	void FinishDeathNow();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_PooledActive();

	// 서버/클라 공통으로 적용되는 활성 상태 처리
	void ApplyPooledActiveState(bool bActive);

	void PlayPresentationMontageLocal(UAnimMontage* Montage, float PlayRate, FName StartSection);

	// GAS의 BaseMoveSpeed 속성이 변경될 때 CharacterMovement의 실제 속도를 업데이트하는 콜백
	virtual void OnBaseMoveSpeedChanged(const struct FOnAttributeChangeData& Data);

	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.f) override;

protected:
	// 에디터에서 할당할 몬스터 데이터 에셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Data")
	TObjectPtr<UAIPawnData> PawnData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Init")
	TObjectPtr<UAIInitComponent> AIInitComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|GAS")
	TObjectPtr<UAscenduntAbilitySystemComponent> ASC;

	// Weapon ammo/stats must also exist for AI loadouts without a WeaponSet grant.
	// Keep this native Set across AbilitySet revocation and pooled reuse.
	UPROPERTY()
	TObjectPtr<UAscenduntWeaponSet> WeaponSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Health")
	TObjectPtr<UAscenduntHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Weapon")
	TObjectPtr<UWeaponEquipmentComponent> WeaponEquipmentComponent;
	
	UPROPERTY()
	FAscenduntAbilitySet_GrantedHandles AbilitySetHandles;

	// 풀에서 활성화되어 있는지. 클라이언트가 충돌을 끌 수 있도록 복제한다.
	UPROPERTY(ReplicatedUsing = OnRep_PooledActive)
	bool bPooledActive = true;
};
