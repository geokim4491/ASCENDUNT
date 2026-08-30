// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AscenduntCharacterMovementComponent.generated.h"

/**
 * FAscenduntCharacterGroundInfo
 *
 * 캐릭터 발밑 지면 정보. 프레임당 한 번만 계산되고 캐싱된다.
 * 소비자가 여럿이어도(애니메이션, 발소리, 착지 판정 등) 트레이스는 한 번만 돈다.
 */
USTRUCT(BlueprintType)
struct FAscenduntCharacterGroundInfo
{
	GENERATED_BODY()

	FAscenduntCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{
	}

	/** 이 정보를 마지막으로 갱신한 프레임 번호. 같은 프레임의 중복 트레이스를 막는다. */
	uint64 LastUpdateFrame;

	/** 지면 트레이스 결과. 표면 타입이 필요한 발소리/발자국 FX에서 그대로 쓸 수 있다. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|CharacterMovement")
	FHitResult GroundHitResult;

	/**
	 * 캡슐 하단부터 지면까지의 거리.
	 * 접지 중이면 0, 트레이스 범위 안에 지면이 없으면 최대 트레이스 거리.
	 * 절대 음수가 되지 않는다. "GroundDistance < N" 형태의 애님 전이 조건이 이 값을 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|CharacterMovement")
	float GroundDistance;
};

/**
 * UAscenduntCharacterMovementComponent
 *
 * 프로젝트 공용 캐릭터 무브먼트 컴포넌트.
 *
 * [가속도 복제]
 * UCharacterMovementComponent::Acceleration은 엔진이 복제하지 않는다. 시뮬레이티드 프록시
 * (내 화면에 보이는 다른 플레이어)에서는 SimulateMovement가 속도로부터 근사치를 만들어 쓴다.
 * 그런데 라이라 로코모션의 Start / Stop / Pivot 판정이 전부 가속도 기반이라,
 * 근사치를 쓰면 다른 플레이어의 출발·정지·방향전환 모션이 안 나오거나 늦게 나온다.
 *
 * ABaseCharacter가 실제 가속도를 COND_SimulatedOnly로 복제하고,
 * 이 클래스가 SimulateMovement의 근사치 덮어쓰기를 막아 그 값을 보존한다.
 *
 * [기본 수치]
 * 캐릭터 BP마다 흩어져 있던 이동 튜닝 값을 여기로 모았다.
 * 기준값은 코드에 두고, 캐릭터별로 달라야 하는 경우에만 BP에서 덮어쓴다.
 */
UCLASS()
class ASCENDUNT_API UAscenduntCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UAscenduntCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 복제받은 가속도를 주입한다. 시뮬레이티드 프록시에서만 호출된다. */
	void SetReplicatedAcceleration(const FVector& InAcceleration);

	/**
	 * 발밑 지면 정보를 반환한다. 프레임당 최초 호출에서만 계산하고 이후에는 캐시를 돌려준다.
	 *
	 * 접지 중에는 무브먼트가 이미 들고 있는 CurrentFloor를 재사용하므로 트레이스 비용이 없다.
	 * 공중일 때만 아래로 라인 트레이스를 한 번 쏜다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|CharacterMovement")
	const FAscenduntCharacterGroundInfo& GetGroundInfo();

protected:
	virtual void SimulateMovement(float DeltaSeconds) override;

	/** GetGroundInfo()의 프레임 캐시. */
	FAscenduntCharacterGroundInfo CachedGroundInfo;

	virtual void FindFloor(
		const FVector& CapsuleLocation,
		FFindFloorResult& OutFloorResult,
		bool bCanUseCachedLocation,
		const FHitResult* DownwardSweepResult
	) const override;

private:
	/** 복제 가속도를 한 번이라도 받았는지. 받기 전에는 엔진 기본 동작을 유지한다. */
	bool bHasReplicatedAcceleration = false;
};
