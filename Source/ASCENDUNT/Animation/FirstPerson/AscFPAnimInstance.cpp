// Fill out your copyright notice in the Description page of Project Settings.


#include "AscFPAnimInstance.h"

#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

#if !UE_BUILD_SHIPPING
// 1인칭 무기 핀 실시간 튜닝용. ReadyPose 값에 더해지는 오프셋이다.
// PIE 콘솔에서 값을 바꾸면 즉시 화면에 반영되므로, 눈으로 맞춘 뒤 그 합을 ABP 기본값에 적는다.
//   asc.FP.Pin.X 8        전방(+) / 후방(-)
//   asc.FP.Pin.Y 4        우측(+) / 좌측(-)
//   asc.FP.Pin.Z -6       상단(+) / 하단(-)
//   asc.FP.Pin.Pitch -2   총구 위(+) / 아래(-)
//   asc.FP.Pin.Yaw -3     총구 우(+) / 좌(-)
//   asc.FP.Pin.Roll 0
//   asc.FP.Pin.Dump 1     현재 최종값을 로그로 출력
static float GAscFPPinX = 0.f;
static float GAscFPPinY = 0.f;
static float GAscFPPinZ = 0.f;
static float GAscFPPinPitch = 0.f;
static float GAscFPPinYaw = 0.f;
static float GAscFPPinRoll = 0.f;
static int32 GAscFPPinDump = 0;

static FAutoConsoleVariableRef CVarAscFPPinX(TEXT("asc.FP.Pin.X"), GAscFPPinX, TEXT("1인칭 무기 핀 전방 오프셋(cm)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPPinY(TEXT("asc.FP.Pin.Y"), GAscFPPinY, TEXT("1인칭 무기 핀 우측 오프셋(cm)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPPinZ(TEXT("asc.FP.Pin.Z"), GAscFPPinZ, TEXT("1인칭 무기 핀 상단 오프셋(cm)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPPinPitch(TEXT("asc.FP.Pin.Pitch"), GAscFPPinPitch, TEXT("1인칭 무기 핀 피치 오프셋(도)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPPinYaw(TEXT("asc.FP.Pin.Yaw"), GAscFPPinYaw, TEXT("1인칭 무기 핀 요 오프셋(도)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPPinRoll(TEXT("asc.FP.Pin.Roll"), GAscFPPinRoll, TEXT("1인칭 무기 핀 롤 오프셋(도)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPPinDump(TEXT("asc.FP.Pin.Dump"), GAscFPPinDump, TEXT("1이면 현재 최종 핀 값을 로그로 출력한다."), ECVF_Cheat);
#endif

void UAscFPAnimInstance::NotifyFired(float Kickback, float PitchUp)
{
	// 뒤로(-X) 밀리고 총구가 위로(-Pitch) 들린다. 회복은 NativeUpdateAnimation 이 처리한다.
	RecoilLocation.X -= Kickback;
	RecoilRotation.Pitch -= PitchUp;

	// 달리는 중에 쏘면 총이 즉시 올라와야 한다.
	TimeSinceFire = 0.f;
}

void UAscFPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 파지 메시 캐시를 매 프레임 무효화한다.
	//
	// 기본 구현은 무기가 바뀔 때만 resolve 하는데, WeaponMesh1P 가 Mesh1P 에 부착되기
	// 전에 그 한 번이 지나가면 ResolveGripMesh 가 3P 메시로 폴백한 결과가 영구히 굳는다.
	// 그러면 왼손이 다른 공간의 좌표를 잡아 총열에서 떨어진다.
	//
	// 1인칭 메시는 로컬 플레이어 하나뿐이고 재탐색 비용은 소켓 조회 두 번이다.
	InvalidateGripMeshCache();

	Super::NativeUpdateAnimation(DeltaSeconds);

	const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	if (!Character)
	{
		return;
	}

	TimeSinceFire += DeltaSeconds;

	if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		GroundSpeed = Movement->Velocity.Size2D();
		bIsFalling = Movement->IsFalling();
	}

	// ----- ADS 상태 pull -----
	//
	// GA_ADS 가 UHeroFirstPersonComponent::SetADSActive() 로 세팅한 값을 여기서 매 프레임
	// 읽어온다. GroundSpeed/bIsFalling 과 같은 pull 패턴이다. GA_ADS 는 LocalOnly 어빌리티라
	// 이 값도 로컬 전용이며 복제되지 않는다.
	bool bIsAiming = false;
	if (const AHeroCharacter* HeroCharacter = Cast<AHeroCharacter>(Character))
	{
		if (const UHeroFirstPersonComponent* FirstPerson = HeroCharacter->GetFirstPersonComponent())
		{
			bIsAiming = FirstPerson->IsADSActive();
		}
	}
	AimAlpha = FMath::FInterpTo(AimAlpha, bIsAiming ? 1.f : 0.f, DeltaSeconds, ADSBlendSpeed);

	// ----- 자세 블렌드: Ready <-> Sprint -----
	const bool bWantSprintPose =
		bUseSprintPose &&
		!bForceReadyPose &&
		!bIsAiming &&
		TimeSinceFire >= ReadyHoldAfterFire &&
		GroundSpeed > SprintSpeedThreshold;

	SprintAlpha = FMath::FInterpTo(SprintAlpha, bWantSprintPose ? 1.f : 0.f, DeltaSeconds, PoseBlendSpeed);

	const FVector BasePoseLocation = FMath::Lerp(ReadyPose.Location, SprintPose.Location, SprintAlpha);
	const FRotator BasePoseRotation = FMath::Lerp(ReadyPose.Rotation, SprintPose.Rotation, SprintAlpha);

	// ADS 는 Ready/Sprint 블렌드 결과 위에 최우선으로 덧씌운다. AimAlpha 가 1 이면
	// SprintAlpha 가 뭐든 상관없이(위에서 !bIsAiming 으로 이미 0에 수렴시켰다) ADSPose 가 된다.
	const FVector AimedPoseLocation = FMath::Lerp(BasePoseLocation, ADSPose.Location, AimAlpha);
	const FRotator AimedPoseRotation = FMath::Lerp(BasePoseRotation, ADSPose.Rotation, AimAlpha);

	// ----- 스웨이: 시야를 돌리면 무기가 한 박자 늦게 따라온다 -----
	const FRotator ControlRotation = Character->GetControlRotation();
	if (bHasPreviousControlRotation && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const FRotator Delta = (ControlRotation - PreviousControlRotation).GetNormalized();

		// 조준 중에는 스웨이를 줄인다. AimAlpha=0 -> 1배, AimAlpha=1 -> SwayAimingScale 배.
		const float SwayDamp = FMath::Lerp(1.f, SwayAimingScale, AimAlpha);

		const FVector TargetLocation(
			0.f,
			FMath::Clamp(-Delta.Yaw * SwayTranslationScale, -SwayMaxOffset, SwayMaxOffset) * SwayDamp,
			FMath::Clamp(Delta.Pitch * SwayTranslationScale, -SwayMaxOffset, SwayMaxOffset) * SwayDamp);

		const FRotator TargetRotation(
			Delta.Pitch * SwayRotationScale * SwayDamp,
			Delta.Yaw * SwayRotationScale * SwayDamp,
			0.f);

		SwayLocation = FMath::VInterpTo(SwayLocation, TargetLocation, DeltaSeconds, SwayInterpSpeed);
		SwayRotation = FMath::RInterpTo(SwayRotation, TargetRotation, DeltaSeconds, SwayInterpSpeed);
	}
	PreviousControlRotation = ControlRotation;
	bHasPreviousControlRotation = true;

	// ----- 바빙: 이동 속도에 비례한 8자 흔들림 -----
	FVector BobLocation = FVector::ZeroVector;
	if (!bIsFalling && GroundSpeed > 1.f)
	{
		const float SpeedRatio = FMath::Clamp(GroundSpeed / BobSpeedReference, 0.f, 1.f);
		BobTime += DeltaSeconds * BobFrequency * SpeedRatio;

		// 조준 중에는 바빙도 같은 배율로 줄인다.
		const float BobDamp = FMath::Lerp(1.f, BobAimingScale, AimAlpha);
		const float Amplitude = BobAmplitude * SpeedRatio * BobDamp;
		BobLocation.Y = FMath::Sin(BobTime) * Amplitude;
		BobLocation.Z = FMath::Sin(BobTime * 2.f) * Amplitude * 0.5f;
	}
	else
	{
		BobTime = 0.f;
	}

	// ----- 반동 회복 -----
	RecoilLocation = FMath::VInterpTo(RecoilLocation, FVector::ZeroVector, DeltaSeconds, RecoilRecoverySpeed);
	RecoilRotation = FMath::RInterpTo(RecoilRotation, FRotator::ZeroRotator, DeltaSeconds, RecoilRecoverySpeed);

	// ----- 합성 -----
	//
	// 핀 값은 "카메라 공간"으로 저술한다. X=전방, Y=우측, Z=상단. 그래야 디자이너가
	// 화면을 보면서 직관적으로 맞출 수 있다.
	//
	// 그런데 Transform (Modify) Bone / Two Bone IK 가 쓰는 Component Space 는 Mesh1P 의
	// 공간이고, Mesh1P 는 카메라에 Yaw -90 회전과 Z 오프셋을 두고 붙어 있다. 즉 컴포넌트
	// 공간의 X 는 카메라 전방이 아니다. 이 변환을 빠뜨리면 총이 옆으로 눕는다.
	//
	// Mesh1P 의 부모가 카메라이므로, 카메라 공간 트랜스폼에 상대 트랜스폼의 역을 곱하면
	// 컴포넌트 공간이 된다. 오프셋과 회전이 BP 에서 바뀌어도 자동으로 따라간다.
	FRotator RotationInCameraSpace = AimedPoseRotation + SwayRotation + RecoilRotation;
	FVector LocationInCameraSpace = AimedPoseLocation + SwayLocation + BobLocation + RecoilLocation;

#if !UE_BUILD_SHIPPING
	// 콘솔 오프셋. 눈으로 맞춘 뒤 이 값을 ABP 의 ReadyPose 에 합산해 적는다.
	LocationInCameraSpace += FVector(GAscFPPinX, GAscFPPinY, GAscFPPinZ);
	RotationInCameraSpace += FRotator(GAscFPPinPitch, GAscFPPinYaw, GAscFPPinRoll);

	if (GAscFPPinDump != 0)
	{
		GAscFPPinDump = 0;

		// 회전은 반드시 "에디터 Details 패널의 칸 순서" 로 출력한다.
		//
		// FRotator 의 생성자 순서는 (Pitch, Yaw, Roll) 인데, 에디터 Details 패널의 세 칸은
		// X=Roll, Y=Pitch, Z=Yaw 순서로 표시된다. Pitch/Yaw/Roll 순으로 찍어 주면 그대로
		// 옮겨 적었을 때 Pitch 와 Yaw 가 뒤바뀐다. 값이 저장되지 않은 것처럼 보이는 원인이다.
		const FRotator ReadyOut(
			ReadyPose.Rotation.Pitch + GAscFPPinPitch,
			ReadyPose.Rotation.Yaw + GAscFPPinYaw,
			ReadyPose.Rotation.Roll + GAscFPPinRoll);

		const FRotator SprintOut(
			SprintPose.Rotation.Pitch + GAscFPPinPitch,
			SprintPose.Rotation.Yaw + GAscFPPinYaw,
			SprintPose.Rotation.Roll + GAscFPPinRoll);

		const FRotator ADSOut(
			ADSPose.Rotation.Pitch + GAscFPPinPitch,
			ADSPose.Rotation.Yaw + GAscFPPinYaw,
			ADSPose.Rotation.Roll + GAscFPPinRoll);

		UE_LOG(LogTemp, Warning,
			TEXT("[FP-PIN] ReadyPose  Location 칸에 = %.1f , %.1f , %.1f   |   Rotation 칸에 = %.1f , %.1f , %.1f"),
			ReadyPose.Location.X + GAscFPPinX, ReadyPose.Location.Y + GAscFPPinY, ReadyPose.Location.Z + GAscFPPinZ,
			ReadyOut.Roll, ReadyOut.Pitch, ReadyOut.Yaw);

		UE_LOG(LogTemp, Warning,
			TEXT("[FP-PIN] SprintPose Location 칸에 = %.1f , %.1f , %.1f   |   Rotation 칸에 = %.1f , %.1f , %.1f"),
			SprintPose.Location.X + GAscFPPinX, SprintPose.Location.Y + GAscFPPinY, SprintPose.Location.Z + GAscFPPinZ,
			SprintOut.Roll, SprintOut.Pitch, SprintOut.Yaw);

		UE_LOG(LogTemp, Warning,
			TEXT("[FP-PIN] ADSPose    Location 칸에 = %.1f , %.1f , %.1f   |   Rotation 칸에 = %.1f , %.1f , %.1f   (조준 중 값을 맞출 땐 이 줄을 쓸 것)"),
			ADSPose.Location.X + GAscFPPinX, ADSPose.Location.Y + GAscFPPinY, ADSPose.Location.Z + GAscFPPinZ,
			ADSOut.Roll, ADSOut.Pitch, ADSOut.Yaw);

		UE_LOG(LogTemp, Warning,
			TEXT("[FP-PIN] Rotation 칸은 왼쪽부터 Roll, Pitch, Yaw 순서다. 위 숫자를 그대로 왼쪽부터 넣으면 된다."));

		// 지금 실제로 적용 중인 값. SprintAlpha 가 0 이 아니면 화면은 두 자세의 혼합이다.
		UE_LOG(LogTemp, Warning,
			TEXT("[FP-PIN] 현재 적용 중  Location=(%.1f, %.1f, %.1f)  Pitch=%.1f Yaw=%.1f Roll=%.1f  SprintAlpha=%.2f  AimAlpha=%.2f"),
			LocationInCameraSpace.X, LocationInCameraSpace.Y, LocationInCameraSpace.Z,
			RotationInCameraSpace.Pitch, RotationInCameraSpace.Yaw, RotationInCameraSpace.Roll,
			SprintAlpha, AimAlpha);

		// Two Bone IK 는 목표가 팔 길이 밖이면 그 위치에 손을 놓지 못하고 최대로 뻗은 채 멈춘다.
		if (const USkeletalMeshComponent* ReachMesh = GetOwningComponent())
		{
			if (ReachMesh->GetBoneIndex(TEXT("upperarm_r")) != INDEX_NONE &&
				ReachMesh->GetBoneIndex(TEXT("lowerarm_r")) != INDEX_NONE &&
				ReachMesh->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE)
			{
				const FVector UpperArm = ReachMesh->GetBoneLocation(TEXT("upperarm_r"), EBoneSpaces::ComponentSpace);
				const FVector LowerArm = ReachMesh->GetBoneLocation(TEXT("lowerarm_r"), EBoneSpaces::ComponentSpace);
				const FVector Hand = ReachMesh->GetBoneLocation(TEXT("hand_r"), EBoneSpaces::ComponentSpace);

				const float ArmReach = FVector::Dist(UpperArm, LowerArm) + FVector::Dist(LowerArm, Hand);
				const float NeededDistance = FVector::Dist(UpperArm, GunPinLocation);

				UE_LOG(LogTemp, Warning,
					TEXT("[FP-PIN] 팔 길이=%.1f  필요 거리=%.1f  -> %s"),
					ArmReach, NeededDistance,
					(NeededDistance > ArmReach)
						? TEXT("사거리 초과. 팔이 다 펴져서 총이 이 값대로 놓이지 않는다.")
						: TEXT("도달 가능."));
			}
		}
	}
#endif

	if (const USkeletalMeshComponent* OwningMesh = GetOwningComponent())
	{
		const FTransform MeshRelative = OwningMesh->GetRelativeTransform();

		// 위치는 절대값이므로 카메라 공간 -> 컴포넌트 공간 변환을 그대로 적용한다.
		// Two Bone IK 의 Effector Location(Component Space)에 연결한다.
		GunPinLocation = MeshRelative.InverseTransformPosition(LocationInCameraSpace);

		// 회전은 애니메이션 포즈에 더할 델타다. 델타의 좌표계 변환은 곱셈이 아니라 켤레다.
		// R_comp = M^-1 * R_cam * M
		const FQuat MeshQuat = MeshRelative.GetRotation();
		GunPinRotationDelta = (MeshQuat.Inverse() * RotationInCameraSpace.Quaternion() * MeshQuat).Rotator();
	}
	else
	{
		GunPinLocation = LocationInCameraSpace;
		GunPinRotationDelta = RotationInCameraSpace;
	}
}

USceneComponent* UAscFPAnimInstance::ResolveGripMesh(AAscenduntWeapon* Weapon) const
{
	if (Weapon)
	{
		if (USkeletalMeshComponent* Weapon1P = Weapon->GetWeaponMesh1P())
		{
			// 부착 여부를 반드시 확인한다. 부착에 실패하면 이 메시는 무기 액터 루트,
			// 즉 3P 무기 위치에 남아 있다. 그대로 반환하면 왼손이 몸통 쪽 허공을 잡는다.
			const USkeletalMeshComponent* OwnerMesh = GetOwningComponent();
			if (OwnerMesh &&
				Weapon1P->GetAttachParent() == OwnerMesh &&
				Weapon1P->DoesSocketExist(GripSocketName))
			{
				return Weapon1P;
			}
		}
	}

	return Super::ResolveGripMesh(Weapon);
}
