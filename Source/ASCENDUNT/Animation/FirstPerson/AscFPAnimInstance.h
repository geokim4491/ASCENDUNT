// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Animation/BaseAnimInstance.h"
#include "AscFPAnimInstance.generated.h"

/** Mesh1P 컴포넌트 공간에서의 무기 자세. Mesh1P 가 카메라에 부착돼 있어 곧 화면상 위치다. */
USTRUCT(BlueprintType)
struct FAscFPWeaponPose
{
	GENERATED_BODY()

	/** X=전방, Y=우측, Z=상단. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson")
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * 1인칭 전용 AnimInstance.
 *
 * ===== 이 클래스가 하는 일 =====
 * 애니메이션이 만든 손 위치를 무시하고 ik_hand_gun 을 화면 기준 고정 자세로 덮어쓴다.
 * 3인칭 애님을 그대로 재생하면 달릴 때 총이 내려가고 사격할 때만 올라오는 "3인칭을 머리에서
 * 보는" 화면이 되는데, 오버워치식 1인칭은 그 반대다. 무기가 화면에 고정돼 있고 애니메이션은
 * 그 위에 얹히는 양념일 뿐이다.
 *
 * 자세는 두 개다.
 *   ReadyPose  - 대기/사격. 총구가 화면 중앙 크로스헤어를 향한다.
 *   SprintPose - 달리기. 총을 끌어안듯 내린다.
 * 이동 속도로 블렌드하되, 사격 직후에는 ReadyHoldAfterFire 동안 강제로 Ready 를 유지한다.
 * 달리다 쏘면 총이 즉시 올라오는 동작이 여기서 나온다.
 *
 * ===== 왜 상수 트랜스폼으로 되는가 =====
 * Mesh1P 는 카메라 컴포넌트에 부착돼 있다. 따라서 컴포넌트 공간이 곧 화면 공간이고,
 * 고정 트랜스폼 하나가 그대로 "화면 우하단 고정"이 된다. 에픽의 CtrlRig_FPWarp 가 FABRIK 으로
 * 복잡하게 푸는 건 메시가 카메라에 붙어 있지 않은 경우이고, 붙이면 그 계산이 상수로 무너진다.
 *
 * ===== ABP 연결 =====
 *   [로코모션 포즈]
 *     -> [Two Bone IK] hand_r / Component Space / Effector Location  <- GunPinLocation
 *     -> [Two Bone IK] hand_r -> ik_hand_gun
 *     -> [Two Bone IK] hand_l -> ik_hand_l      (왼손이 총열을 잡는다)
 *
 * ik_hand_l 은 표준 UE5 리그에서 ik_hand_gun 의 자식이라, 총을 고정하면 왼손 목표도 같이
 * 따라온다. 별도 계산이 필요 없다.
 */
UCLASS()
class ASCENDUNT_API UAscFPAnimInstance : public UBaseAnimInstance
{
	GENERATED_BODY()

public:
	//~ 자세 ---------------------------------------------------------------
	/** 대기/사격 자세. 총구가 화면 중앙을 향하도록 Rotation 을 맞춘다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose")
	FAscFPWeaponPose ReadyPose = { FVector(32.f, 14.f, -14.f), FRotator::ZeroRotator };

	/** 달리기 자세. 총을 끌어안듯 내리고 비튼다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose")
	FAscFPWeaponPose SprintPose = { FVector(20.f, 8.f, -26.f), FRotator(-40.f, -20.f, 20.f) };

	/**
	 * ADS(정조준) 자세. 조준기가 화면 중앙 크로스헤어에 오도록 맞춘다.
	 *
	 * Ready/Sprint 블렌드 결과 위에 최우선으로 덧씌운다. bIsAiming 은 GA_ADS 가
	 * UHeroFirstPersonComponent::SetADSActive() 로 세팅한 값을 매 프레임 이 클래스가
	 * 읽어온다(다른 상태들과 같은 pull 방식). ADS 중에는 SprintPose 자체를 억제하므로
	 * 조준하며 달려도 자세가 충돌하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose")
	FAscFPWeaponPose ADSPose = { FVector(18.f, 0.f, -6.f), FRotator::ZeroRotator };

	/**
	 * 달릴 때 SprintPose 로 총을 내릴지 여부.
	 *
	 * 끄면 항상 ReadyPose 만 쓴다. 지금 판정이 이동 속도 기반이라 걷기와 달리기 경계에서
	 * 자세가 계속 오르내려 어지럽다. 질주가 별도 입력으로 들어오는 상태가 아니라면 끄는 편이 낫다.
	 *
	 * 나중에 질주 어빌리티가 생기면 속도 대신 그 상태로 판정하도록 바꾸고 다시 켜면 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose")
	bool bUseSprintPose = true;

	/** 이 속도를 넘으면 SprintPose 로 간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose", meta = (ClampMin = "0.0"))
	float SprintSpeedThreshold = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose", meta = (ClampMin = "0.1"))
	float PoseBlendSpeed = 9.f;

	/** 사격 후 이 시간 동안은 달리고 있어도 ReadyPose 를 유지한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose", meta = (ClampMin = "0.0"))
	float ReadyHoldAfterFire = 0.55f;

	/** ADS 나 스킬처럼 무조건 총을 올려야 할 때 BP 에서 켠다. */
	UPROPERTY(BlueprintReadWrite, Category = "FirstPerson|Pose")
	bool bForceReadyPose = false;

	/** ADS 진입/이탈 보간 속도. 조준은 총 올리기(PoseBlendSpeed)보다 빠릿해야 손맛이 산다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Pose", meta = (ClampMin = "0.1"))
	float ADSBlendSpeed = 12.f;

	//~ 스웨이 / 바빙 / 반동 ------------------------------------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Sway")
	float SwayTranslationScale = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Sway")
	float SwayRotationScale = 0.25f;

	/** 스웨이 상한. 빠른 시야 전환에서 총이 화면 밖으로 나가는 걸 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Sway")
	float SwayMaxOffset = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Sway")
	float SwayInterpSpeed = 9.f;

	/** 조준 중 스웨이 진폭에 곱해지는 배율. 0 이면 조준 중 스웨이가 완전히 사라진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Sway", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwayAimingScale = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Bob")
	float BobAmplitude = 1.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Bob")
	float BobFrequency = 7.5f;

	/** 이 속도에서 바빙 진폭이 100% 가 된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Bob", meta = (ClampMin = "1.0"))
	float BobSpeedReference = 600.f;

	/** 조준 중 바빙 진폭에 곱해지는 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Bob", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BobAimingScale = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FirstPerson|Recoil")
	float RecoilRecoverySpeed = 14.f;

	//~ ABP 가 읽는 출력 ---------------------------------------------------
	/**
	 * Two Bone IK(hand_r) 의 Effector Location 에 연결한다. Space 는 Component Space.
	 * Mesh1P 컴포넌트 공간의 절대 위치이며, 자세 블렌드 + 스웨이 + 바빙 + 반동이 합성돼 있다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson")
	FVector GunPinLocation = FVector::ZeroVector;

	/**
	 * Transform (Modify) Bone 의 Rotation 에 연결한다. Rotation Mode 는 Add to Existing.
	 *
	 * 절대 회전이 아니라 애니메이션 포즈에 더할 "델타" 다. 라이플 Idle 포즈가 이미 총을
	 * 제대로 쥔 손 회전을 갖고 있으므로, 그걸 버리고 절대값으로 덮으면 손이 꺾인다.
	 * 여기에는 GunPinRotation(저술값) + 스웨이 + 반동이 합성돼 들어간다.
	 *
	 * 회전 델타는 위치와 달리 좌표계 변환이 곱셈이 아니라 켤레(conjugation)라서
	 * GunPinLocation 과 따로 계산한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson")
	FRotator GunPinRotationDelta = FRotator::ZeroRotator;

	/** 0 = Ready, 1 = Sprint. ABP 에서 상체 애님을 같이 블렌드하고 싶을 때 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson")
	float SprintAlpha = 0.f;

	/** 0 = 비조준, 1 = ADS. ABP 에서 상체 애님이나 확산(스프레드) 연출에 같이 쓸 수 있다. */
	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson")
	float AimAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FirstPerson")
	bool bIsFalling = false;

	/** 사격 시 호출. 반동을 누적하고 총을 즉시 Ready 로 올린다. */
	UFUNCTION(BlueprintCallable, Category = "FirstPerson")
	void NotifyFired(float Kickback = 2.5f, float PitchUp = 1.5f);

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;



protected:
	/**
	 * 1인칭에서는 3P 무기 메시가 아니라 WeaponMesh1P 의 파지 소켓을 기준으로 삼는다.
	 *
	 * LeftHandGripTarget 은 hand_r 본 공간 상대 좌표라 포즈와 무관하게 일정하다는 것이
	 * 기존 해법의 핵심인데, 그 전제는 "무기가 hand_r 에 강체로 붙어 있다" 는 것이다.
	 * 1P 무기는 Mesh1P 의 weapon_r 에 붙고 WeaponBoneOffset1P 로 따로 보정되므로,
	 * 3P 메시에서 좌표를 뽑으면 그 오프셋 차이만큼 왼손이 어긋난다.
	 *
	 * 1P 메시가 없거나 소켓이 없으면 부모 구현으로 폴백한다.
	 */
	virtual USceneComponent* ResolveGripMesh(AAscenduntWeapon* Weapon) const override;
private:
	FVector SwayLocation = FVector::ZeroVector;
	FRotator SwayRotation = FRotator::ZeroRotator;

	FVector RecoilLocation = FVector::ZeroVector;
	FRotator RecoilRotation = FRotator::ZeroRotator;

	float BobTime = 0.f;
	float TimeSinceFire = BIG_NUMBER;

	FRotator PreviousControlRotation = FRotator::ZeroRotator;
	bool bHasPreviousControlRotation = false;
};
