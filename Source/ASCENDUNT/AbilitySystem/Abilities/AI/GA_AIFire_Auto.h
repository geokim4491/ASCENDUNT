// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_Fire_Auto.h"
#include "GA_AIFire_Auto.generated.h"

/**
 * AI 전용 자동 발사 어빌리티.
 *
 * UGA_Fire_Auto 의 발사 루프는 그대로 쓰고 조준선 계산만 교체한다.
 *
 * ── 왜 별도 클래스인가 ──────────────────────────────────────────────
 * UGA_BaseFire::BuildViewRayAndMuzzle 의 기본 구현은 PlayerController 가 있으면
 * 시점을, 없으면 아바타의 액터 회전을 조준선으로 쓴다. AI 는 후자에 걸리는데
 * 캐릭터의 액터 회전은 Yaw 뿐이라 두 가지가 깨진다.
 *
 *   - 높이차가 있는 표적을 조준할 수 없다(항상 수평 발사).
 *   - bOrientRotationToMovement 가 켜져 있으면 폰이 이동 방향을 향하므로,
 *     스트레이핑하는 순간 총알이 표적이 아니라 이동 방향으로 나간다.
 *
 * 이 클래스는 대신 폰의 GetBaseAimRotation()(= 컨트롤러의 ControlRotation)을 쓴다.
 * ABaseAIController::SetTargetActor 가 SetFocus 를 걸어두므로 AAIController 가
 * 매 틱 표적을 향해 ControlRotation 을 갱신하고, 표적이 Pawn 이면 Pitch 도 유지된다.
 *
 * 히어로가 쓰는 BPGA_Fire_Auto / BPGA_FireProjectile_Auto 는 건드리지 않는다.
 */
UCLASS()
class ASCENDUNT_API UGA_AIFire_Auto : public UGA_Fire_Auto
{
	GENERATED_BODY()

protected:
	virtual bool BuildViewRayAndMuzzle(
		const FGameplayAbilityActorInfo* ActorInfo,
		AAscenduntWeapon* Weapon,
		FVector& OutViewStart,
		FVector& OutViewDir,
		FVector& OutMuzzleLoc
	) const override;
};
