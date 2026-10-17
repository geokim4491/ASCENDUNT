// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeroFirstPersonComponent.generated.h"

class AHeroCharacter;
class AAscenduntWeapon;
class UAnimMontage;

/**
 * 로컬 플레이어의 1인칭 표현을 소유하는 컴포넌트.
 *
 * ===== 소유권 =====
 * 아래 렌더 프로퍼티는 이 컴포넌트만 변경한다. 다른 코드가 만지면 매 프레임 서로 덮어쓴다.
 *   - AHeroCharacter::Mesh1P 의 Visibility / 숨긴 본 / 숨긴 머티리얼 슬롯
 *   - AHeroCharacter::GetMesh() 의 FirstPersonPrimitiveType
 *   - 현재 무기의 WeaponMesh / StaticWeaponMesh 의 FirstPersonPrimitiveType
 *   - 몸통과 무기 X-Ray 메시의 bOwnerNoSee
 *   - 무기 WeaponMesh1P 의 부착과 Visibility
 *
 * ===== 왜 알림이 아니라 매 프레임 조정인가 =====
 * 1인칭 여부는 로컬 조종 여부 / 사망 래그돌 / 카메라 모드(스킬 오버라이드, PvE Downed,
 * 데스 스펙테이터) / 무기 교체 / 애님 초기화 완료 등 서로 무관한 여러 소스에서 바뀐다.
 * 각 소스가 "바뀌었다" 고 알려주는 방식은 알림 하나만 빠져도 조용히 깨지고, 어느 알림을
 * 빠뜨렸는지는 코드만 봐서 알 수 없다.
 *
 * 대신 매 프레임 목표 상태를 계산해서 현재와 다를 때만 맞춘다. 정상 상태에서는 포인터
 * 비교 몇 개로 끝나므로 비용은 무시할 수 있다.
 *
 * ===== 두 갈래로 나눈 이유 =====
 * ReconcileWeaponAttachment 는 매 프레임, ApplyViewMode 는 값이 바뀔 때만 실행한다.
 * 부착은 Mesh1P 메시 / 무기 액터 / 소켓이 모두 준비돼야 성공하는데 그 시점이 모드 전환
 * 시점과 무관하다. 변경 감지 안에 두면 전환 순간 한 번 시도하고 실패하면 영영 재시도하지
 * 않는다. 반대로 렌더 프로퍼티는 매 프레임 세팅하면 MarkRenderStateDirty 가 낭비된다.
 */
UCLASS(ClassGroup = (ASCENDUNT), meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UHeroFirstPersonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroFirstPersonComponent();

	UFUNCTION(BlueprintPure, Category = "Ascendunt|FirstPerson")
	static UHeroFirstPersonComponent* FindFirstPersonComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UHeroFirstPersonComponent>() : nullptr);
	}

	/** 마지막으로 적용된 상태. 판정이 아니라 결과다. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|FirstPerson")
	bool IsFirstPersonActive() const { return bAppliedFirstPerson; }

	/**
	 * Mesh1P 에서만 재생하는 몽타주. 로컬 오너만 보므로 복제하지 않는다.
	 *
	 * 3P 몽타주(GAS 태스크 경로)를 대체하는 것이 아니라 추가되는 것이다. 3P 메시는 1인칭에서도
	 * WorldSpaceRepresentation 으로 그림자를 만들기 때문에, 3P 재생을 멈추면 그림자만 가만히
	 * 서 있게 된다.
	 *
	 * 재생 배율은 호출부가 3P와 같은 식으로 계산해서 넘겨야 두 메시가 어긋나지 않는다.
	 * 1인칭이 아니거나 몽타주가 비어 있으면 아무 일도 하지 않고 0 을 돌려준다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|FirstPerson")
	float PlayMontage1P(UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSection = NAME_None);

	/**
	 * 사격 시 호출. Mesh1P 의 AnimInstance 에 반동을 넣고 총을 즉시 Ready 자세로 올린다.
	 *
	 * 달리는 중에 쏘면 SprintPose(총을 끌어안은 자세)가 적용 중인데, 이 호출이
	 * ReadyHoldAfterFire 동안 강제로 ReadyPose 를 유지시켜 총이 올라오게 만든다.
	 *
	 * 1인칭이 아니거나 Mesh1P 의 AnimInstance 가 UAscFPAnimInstance 가 아니면 아무 일도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|FirstPerson")
	void NotifyFired1P();

	/** Local ADS state supplied by GA_ADS. AnimInstance reads this every frame. */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|FirstPerson|ADS")
	void SetADSActive(bool bEnabled) { bADSActive = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|FirstPerson|ADS")
	bool IsADSActive() const { return bADSActive; }

	/**
	 * 1인칭일 때 Mesh1P 에서 숨길 본들. 각 본과 그 자식이 통째로 사라진다.
	 *
	 * Mesh1P 는 카메라에 부착된 풀바디 메시라, 그대로 두면 머리 내부 폴리곤이 화면을 가리고
	 * 다리가 카메라 피치를 따라 돈다.
	 *
	 * 몸통은 이 방식으로 지울 수 없다. 팔이 spine 의 자손이라 몸통을 숨기면 팔까지 사라진다.
	 * 팔만 남기려면 1인칭 전용으로 잘라낸 메시가 필요하다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|FirstPerson")
	TArray<FName> HiddenBones1P = { TEXT("head"), TEXT("thigh_l"), TEXT("thigh_r") };
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 1인칭이어야 하는가. 하나라도 어긋나면 false.
	 *
	 * GameplayReady 전에는 항상 false 를 돌려 부분적으로 준비된 1P 표현이 먼저 노출되지 않게 한다.
	 * GameplayReady 이후에도 Mesh1P 에 스켈레탈 메시나 AnimClass 가 없으면 false 이므로,
	 * 1P 애셋을 지정하지 않은 히어로는 조용히 3인칭으로 동작한다.
	 */
	bool EvaluateDesiredFirstPerson() const;

	/** 렌더 프로퍼티를 맞춘다. 목표와 현재가 다를 때만 실제로 만진다. */
	void ApplyViewMode(AHeroCharacter* Hero, bool bFirstPerson);

	/** 1P 무기 메시를 Mesh1P 의 손 소켓에 붙이고 가시성을 맞춘다. 멱등하므로 매 프레임 호출한다. */
	void ReconcileWeaponAttachment(AHeroCharacter* Hero, bool bFirstPerson);

	AHeroCharacter* GetHeroCharacter() const;

private:
	static AAscenduntWeapon* GetCurrentWeapon(const AHeroCharacter* Hero);

	/** Mesh1P 의 가시성, 숨긴 본, 숨긴 머티리얼 슬롯을 함께 맞춘다. */
	void ApplyMesh1PVisibility(AHeroCharacter* Hero, bool bFirstPerson);

	/** 무기 본체 메시와 무기 X-Ray 메시의 1인칭 렌더 상태를 함께 맞춘다. */
	static void SetWeaponFirstPersonState(AAscenduntWeapon* Weapon, bool bFirstPerson);

	bool bAppliedFirstPerson = false;

	/** 한 번도 적용한 적이 없으면 목표가 false 여도 초기 상태를 확정해야 한다. */
	bool bHasAppliedOnce = false;

	/**
	 * 마지막으로 상태를 적용한 무기.
	 *
	 * 캐시 키에 무기를 포함하지 않으면, 무기를 바꿔도 모드가 같다는 이유로 early-out 되어
	 * 새 무기의 3P 메시가 None 인 채로 남는다. 1인칭에서 총이 두 개 보이는 증상이 된다.
	 */
	TWeakObjectPtr<AAscenduntWeapon> AppliedWeapon;

	/** Presentation-only; never replicated. */
	bool bADSActive = false;

#if !UE_BUILD_SHIPPING
	/** asc.FP.Mesh.* 콘솔 오프셋을 Mesh1P 에 적용한다. 튜닝 전용. */
	void ApplyDebugMeshOffset(AHeroCharacter* Hero);

	/** 콘솔 오프셋의 기준이 되는 BP 원본 위치. 오프셋 누적을 막는다. */
	FVector BaseMesh1PRelativeLocation = FVector::ZeroVector;
	bool bCachedBaseMeshLocation = false;
#endif
};
