#include "AscenduntCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AscenduntCharacterMovementComponent)

namespace AscenduntCharacter
{
	static float GroundTraceDistance = 100000.0f;
	FAutoConsoleVariableRef CVar_GroundTraceDistance(
		TEXT("AscenduntCharacter.GroundTraceDistance"),
		GroundTraceDistance,
		TEXT("Distance to trace down when generating ground information."),
		ECVF_Cheat);
}

UAscenduntCharacterMovementComponent::UAscenduntCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// BP_Hakuko 에 흩어져 있던 이동 튜닝 값을 코드 기준값으로 승격.
	// 라이라는 이 값들을 전부 UE 기본값으로 두지만, 이 프로젝트는 조작감을 위해 조정했다.
	MaxAcceleration = 2400.0f;             // UE 기본 2048. 출발 반응성 확보
	BrakingFrictionFactor = 1.0f;          // UE 기본 2.0. 정지 거리를 늘려 Stop 애님 재생 시간 확보
	BrakingDecelerationWalking = 1400.0f;  // UE 기본 2048
}

void UAscenduntCharacterMovementComponent::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

const FAscenduntCharacterGroundInfo& UAscenduntCharacterMovementComponent::GetGroundInfo()
{
	// 같은 프레임에 여러 번 물어봐도 트레이스는 한 번만 돈다.
	if (!CharacterOwner || (GFrameCounter == CachedGroundInfo.LastUpdateFrame))
	{
		return CachedGroundInfo;
	}

	if (MovementMode == MOVE_Walking)
	{
		// 접지 중에는 무브먼트가 이미 바닥 정보를 들고 있다. 트레이스 불필요.
		CachedGroundInfo.GroundHitResult = CurrentFloor.HitResult;
		CachedGroundInfo.GroundDistance = 0.0f;
	}
	else
	{
		const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = (UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn);
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, (TraceStart.Z - AscenduntCharacter::GroundTraceDistance - CapsuleHalfHeight));

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AscenduntCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParam);

		CachedGroundInfo.GroundHitResult = HitResult;

		// 지면을 못 찾아도 음수를 남기지 않는다.
		CachedGroundInfo.GroundDistance = AscenduntCharacter::GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking)
		{
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit)
		{
			// Hit.Distance는 캡슐 중심 기준이므로 반높이를 빼야 발밑 기준 거리가 된다.
			CachedGroundInfo.GroundDistance = FMath::Max((HitResult.Distance - CapsuleHalfHeight), 0.0f);
		}
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;

	return CachedGroundInfo;
}

void UAscenduntCharacterMovementComponent::SimulateMovement(float DeltaSeconds)
{
	if (!bHasReplicatedAcceleration)
	{
		Super::SimulateMovement(DeltaSeconds);
		return;
	}

	// SimulateMovement 는 시뮬레이티드 프록시의 Acceleration 을 속도 기반 근사치로 덮어쓴다.
	// 복제받은 실제 가속도를 되돌려놔야 Start / Stop / Pivot 판정이 맞는다.
	const FVector PreservedAcceleration = Acceleration;
	Super::SimulateMovement(DeltaSeconds);
	Acceleration = PreservedAcceleration;
}

void UAscenduntCharacterMovementComponent::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASC_AI_FindFloor);

	Super::FindFloor(
		CapsuleLocation,
		OutFloorResult,
		bCanUseCachedLocation,
		DownwardSweepResult);
}
