// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h" // FGameplayTagBlueprintPropertyMap 사용을 위해 필수
#include "BaseAnimInstance.generated.h"

class AAscenduntWeapon;
class ACharacter;
class APawn;
class UAbilitySystemComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UWeaponEquipmentComponent;

/**
 * UBaseAnimInstance
 * * 라이라 아키텍처를 기반으로 재설계된 프로젝트의 최상위 베이스 애님 인스턴스입니다.
 */
UCLASS(Config = Game)
class ASCENDUNT_API UBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UBaseAnimInstance(const FObjectInitializer& ObjectInitializer);

	// 어빌리티 시스템(GAS)과 애님 블루프린트의 태그 변수를 연동해주는 함수
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

public:
	//~ 양손 파지 IK 데이터
	// 라이플처럼 하나의 무기를 양손으로 잡는 경우, 왼손을 무기의 파지 소켓에 붙이기 위한 데이터다.
	//
	// [수집 위치] 소켓/컴포넌트 조회 API는 스레드 세이프하지 않아
	//   BlueprintThreadSafeUpdateAnimation에서 호출할 수 없다. 그래서 게임 스레드인
	//   NativeUpdateAnimation에서 수집해 프로퍼티에 담고, 애님 그래프는 Property Access로 읽는다.
	//   위 GroundDistance를 라이라 원본이 무브먼트 컴포넌트에서 읽어 채우는 것과 동일한 패턴이다.
	//
	// [매 프레임인 이유] 장착 시 1회 계산도 가능하지만, 애님 인스턴스 생성이나 리플리케이션이
	//   한 프레임이라도 늦으면 값이 영영 비어버린다. 매 프레임 갱신은 다음 프레임에 자가 복구된다.
	//   실제 비용은 무기 탐색을 캐싱한 뒤 소켓 조회 두 번뿐이다.

	/**
	 * 무기의 왼손 파지 지점. ReferenceBoneName 본 공간 좌표.
	 * Two Bone IK(hand_l)의 Effector Location에 연결한다.
	 * 그 노드의 Effector Location Space는 Bone Space, Effector Target은
	 * ReferenceBoneName과 반드시 일치해야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon|IK")
	FVector LeftHandGripTarget = FVector::ZeroVector;

	/**
	 * 이번 프레임에 유효한 파지 지점을 계산했는지 여부.
	 * false일 때 IK 알파를 0으로 내려야 왼손이 허공이나 낡은 좌표를 붙잡지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon|IK")
	bool bHasLeftHandGripTarget = false;

protected:
#if WITH_EDITOR
	// 에디터에서 컴파일하거나 저장할 때 태그 데이터가 올바른지 검증하는 함수
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * 무브먼트 컴포넌트에서 지면 거리를 읽어와 GroundDistance에 채운다.
	 *
	 * 실제 계산은 UAscenduntCharacterMovementComponent::GetGroundInfo()가 하고
	 * 여기서는 값을 옮기기만 한다. 라이라의 ULyraAnimInstance와 같은 역할 분담이다.
	 * 트레이스 결과가 프레임 단위로 캐싱되므로 발소리 같은 다른 소비자가 생겨도
	 * 트레이스는 프레임당 한 번만 돈다.
	 */
	virtual void UpdateGroundDistance(const ACharacter* Character);

	/** 매 프레임 파지 데이터를 갱신한다. 실패 경로에서는 bHasLeftHandGripTarget이 false로 남는다. */
	void UpdateWeaponGripData();

	/**
	 * 파지 메시 캐시를 무효화한다. 다음 갱신에서 ResolveGripMesh 가 다시 호출된다.
	 *
	 * 기본 경로는 무기가 바뀔 때만 resolve 하는데, 파생 클래스가 "아직 준비되지 않은
	 * 메시" 때문에 폴백을 반환한 경우 그 결과가 영구히 굳는다. 그런 파생 클래스가
	 * 매 프레임 재시도할 수 있게 열어둔다. (UAscFPAnimInstance 참고)
	 */
	void InvalidateGripMeshCache() { bGripMeshResolved = false; }

	/**
	 * 무기에서 파지 소켓을 가진 컴포넌트를 찾는다. 파지 소켓이 없으면 nullptr.
	 * 결과는 캐싱되며 장착 무기가 바뀔 때만 다시 호출된다.
	 */
	virtual USceneComponent* ResolveGripMesh(AAscenduntWeapon* Weapon) const;

	/**
	 * 파지 지점 계산부.
	 * 파지 방식이 근본적으로 다른 캐릭터가 생기면 이 함수만 오버라이드한다.
	 *
	 * @return 유효한 값을 산출했으면 true
	 */
	virtual bool ComputeLeftHandGripTarget(USkeletalMeshComponent* OwnerMesh, const USceneComponent* GripMesh, FVector& OutTarget);

protected:
	/** 무기 메시에서 찾을 왼손 파지 소켓 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|IK")
	FName GripSocketName = TEXT("LeftHandSocket");

	/** 파지 지점을 표현할 기준 본. Two Bone IK의 Effector Target과 반드시 일치해야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|IK")
	FName ReferenceBoneName = TEXT("hand_r");

	/** 시각 전용 계산이므로 데디케이티드 서버에서는 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|IK", AdvancedDisplay)
	bool bSkipGripUpdateOnDedicatedServer = true;

protected:
	/**
	 * 무브먼트 컴포넌트에서 값을 못 받았을 때 쓸 대체값.
	 * 0이나 음수를 남기면 "GroundDistance < N" 전이 조건이 항상 참이 되므로
	 * 반드시 충분히 큰 값이어야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Character State Data", AdvancedDisplay)
	float GroundDistanceFallback = 100000.0f;

	/** 지면 거리는 시각 전용이므로 데디케이티드 서버에서는 조회를 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Character State Data", AdvancedDisplay)
	bool bSkipGroundTraceOnDedicatedServer = true;

protected:
	// [핵심] 게임플레이 태그를 블루프린트 변수에 다이렉트로 매핑해주는 마법의 맵입니다.
	// 이벤트 그래프에서 매 프레임 태그를 검사하는 노드를 노가다로 짤 필요를 없애줍니다.
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	// 지면과의 거리. UpdateGroundDistance()가 매 프레임 채운다.
	// 접지 중이면 0, 공중이면 캡슐 하단부터 지면까지의 거리, 지면을 못 찾으면 GroundTraceDistance.
	// FallLoop -> FallLand 같은 "GroundDistance < N" 형태의 전이 조건이 이 값을 읽으므로
	// 음수가 남으면 조건이 항상 참이 되어 낙하 루프가 즉시 끊긴다.
	UPROPERTY(BlueprintReadOnly, Category = "Character State Data")
	float GroundDistance = -1.0f;

	// 태그 맵이 정상적으로 초기화되었는지 여부
	bool bIsTagMapInitialized = false;

private:
	// 매 프레임 컴포넌트를 재탐색하지 않기 위한 캐시.
	// 폰이 교체되면(리스폰/캐릭터 변경) 무효화한다.
	TWeakObjectPtr<UWeaponEquipmentComponent> CachedEquipment;
	TWeakObjectPtr<APawn> CachedGripPawn;

	// 무기 컴포넌트가 없는 폰에서 매 프레임 FindComponentByClass를 반복하지 않도록
	// 폰당 1회만 탐색한다. 컴포넌트는 생성자에서 만들어지므로 뒤늦게 생길 일은 없다.
	bool bEquipmentSearchDone = false;

	// 파지 메시 탐색 결과 캐시. 장착 무기가 바뀔 때만 갱신한다.
	// 파지 소켓이 없는 무기(메시 없는 AI 무기, 쌍검 등)는 nullptr이 캐싱되어
	// 매 프레임 소켓 조회와 컴포넌트 순회를 반복하지 않는다.
	TWeakObjectPtr<AAscenduntWeapon> CachedGripWeapon;
	TWeakObjectPtr<USceneComponent> CachedGripMesh;
	bool bGripMeshResolved = false;

	// 설정 오류 경고를 매 프레임 찍지 않기 위한 1회성 플래그.
	bool bLoggedMissingReferenceBone = false;
};
