// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_AIFire_Auto.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

FVector UGA_AIFire_Auto::ComputeProjectileLaunchDir(
	const FAscenduntFireOnceContext& Ctx,
	const FVector& MuzzleLoc,
	const FVector& AimPoint) const
{
	const FVector StraightDir = Super::ComputeProjectileLaunchDir(Ctx, MuzzleLoc, AimPoint);

	if (!Ctx.Weapon)
	{
		return StraightDir;
	}

	const FAscenduntWeaponModeStats& S = Ctx.Weapon->GetCurrentModeStats();
	if (!S.bUseBallisticArc || !S.ProjectileClass || S.ProjectileSpeed <= KINDA_SMALL_NUMBER)
	{
		return StraightDir;
	}

	UWorld* World = Ctx.Weapon->GetWorld();
	if (!World)
	{
		return StraightDir;
	}

	// 중력은 투사체 BP 가 소유한다. 여기서 별도 값을 들고 있으면 두 설정이 갈라진다.
	const AAscenduntProjectile* ProjectileCDO = S.ProjectileClass->GetDefaultObject<AAscenduntProjectile>();
	const UProjectileMovementComponent* MovementCDO =
		ProjectileCDO ? ProjectileCDO->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
	const float GravityScale = MovementCDO ? MovementCDO->ProjectileGravityScale : 0.f;

	if (FMath::IsNearlyZero(GravityScale))
	{
		// 중력이 없으면 포물선이 생기지 않는다. 직사로 두는 것이 맞다.
		return StraightDir;
	}

	// 발사 속도는 데이터가 정한 값을 유지하고, 그 속도로 도달 가능한 각도만 푼다.
	// 속도까지 해가 정하게 하면 디자이너가 잡은 탄속/사거리 밸런스가 무너진다.
	FVector TossVelocity = FVector::ZeroVector;
	const bool bSolved = UGameplayStatics::SuggestProjectileVelocity(
		World,
		TossVelocity,
		MuzzleLoc,
		AimPoint,
		S.ProjectileSpeed,
		S.bBallisticHighArc,
		/*CollisionRadius*/ 0.f,
		/*OverrideGravityZ*/ World->GetGravityZ() * GravityScale,
		ESuggestProjVelocityTraceOption::DoNotTrace);

	// 해가 없으면 그 탄속으로는 닿지 않는 거리라는 뜻이다.
	// 직사로 쏘면 짧게 떨어지지만 발사가 통째로 막히는 것보다는 낫고,
	// BT 의 교전 거리 조건이 이 상황 자체를 줄여준다.
	return bSolved ? TossVelocity.GetSafeNormal() : StraightDir;
}

bool UGA_AIFire_Auto::BuildViewRayAndMuzzle(
	const FGameplayAbilityActorInfo* ActorInfo,
	AAscenduntWeapon* Weapon,
	FVector& OutViewStart,
	FVector& OutViewDir,
	FVector& OutMuzzleLoc) const
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const APawn* AvatarPawn = Cast<APawn>(Avatar);

	if (!AvatarPawn)
	{
		// AI 가 아닌 곳에 잘못 붙은 경우. 기본 구현에 맡긴다.
		return Super::BuildViewRayAndMuzzle(ActorInfo, Weapon, OutViewStart, OutViewDir, OutMuzzleLoc);
	}

	// 조준선: 눈높이에서 컨트롤러가 바라보는 방향.
	// SetFocus 가 걸려 있으면 AAIController 가 표적을 향해 갱신해 둔 값이다.
	OutViewStart = AvatarPawn->GetPawnViewLocation();
	OutViewDir = AvatarPawn->GetBaseAimRotation().Vector().GetSafeNormal();

	// 총구: 무기 액터의 위치.
	//
	// Weapon->GetMuzzleTransform() 을 부르지 않는다. AI 무기는 WeaponMesh 에
	// 스켈레탈 메시를 넣지 않아(총구 소켓은 캐릭터 스켈레톤에 있다) 그 함수가
	// 발사할 때마다 LogSkinnedMeshComp 경고를 찍기 때문이다.
	//
	// 무기 액터는 캐릭터의 무기 소켓에 SnapToTarget 으로 붙으므로
	// 액터 위치가 곧 그 소켓 위치다. 결과값은 같고 경고만 사라진다.
	OutMuzzleLoc = Weapon ? Weapon->GetActorLocation() : OutViewStart;

	return true;
}
