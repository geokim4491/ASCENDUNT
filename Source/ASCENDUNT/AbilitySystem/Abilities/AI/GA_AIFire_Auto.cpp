#include "GA_AIFire_Auto.h"
// Fill out your copyright notice in the Description page of Project Settings.

#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

FVector UGA_AIFire_Auto::ComputeProjectileLaunchDir(const FAscenduntWeaponAttackContext& Ctx, const FVector& MuzzleLoc, const FVector& AimPoint) const
{
	const FVector StraightDir = Super::ComputeProjectileLaunchDir(Ctx, MuzzleLoc, AimPoint);

	AAscenduntWeapon* Weapon = Ctx.Weapon.Get();
	if (!Weapon)
	{
		return StraightDir;
	}

	const FAscenduntWeaponModeStats& S = Weapon->GetCurrentModeStats();
	if (!S.bUseBallisticArc || !S.ProjectileClass || S.ProjectileSpeed <= KINDA_SMALL_NUMBER)
	{
		return StraightDir;
	}

	UWorld* World = Weapon->GetWorld();
	if (!World)
	{
		return StraightDir;
	}

	// 중력은 투사체 BP 가 소유한다. 여기서 별도 값을 들고 있으면 두 설정이 갈라진다.
	const AAscenduntProjectile* ProjectileCDO = S.ProjectileClass->GetDefaultObject<AAscenduntProjectile>();
	const UProjectileMovementComponent* MovementCDO = ProjectileCDO ? ProjectileCDO->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
	const float GravityScale = MovementCDO ? MovementCDO->ProjectileGravityScale : 0.f;

	if (FMath::IsNearlyZero(GravityScale))
	{
		// 중력이 없으면 포물선이 생기지 않는다. 직사로 두는 것이 맞다.
		return StraightDir;
	}

	// 발사 속도는 데이터가 정한 값을 유지하고, 그 속도로 도달 가능한 각도만 푼다.
	// 속도까지 해가 정하게 하면 디자이너가 잡은 탄속/사거리 밸런스가 무너진다.
	FVector TossVelocity = FVector::ZeroVector;
	const bool bSolved = UGameplayStatics::SuggestProjectileVelocity(World, TossVelocity, MuzzleLoc, AimPoint, S.ProjectileSpeed, S.bBallisticHighArc,
	    /*CollisionRadius*/ 0.f,
	    /*OverrideGravityZ*/ World->GetGravityZ() * GravityScale, ESuggestProjVelocityTraceOption::DoNotTrace);

	// 해가 없으면 그 탄속으로는 닿지 않는 거리라는 뜻이다.
	// 직사로 쏘면 짧게 떨어지지만 발사가 통째로 막히는 것보다는 낫고,
	// BT 의 교전 거리 조건이 이 상황 자체를 줄여준다.
	return bSolved ? TossVelocity.GetSafeNormal() : StraightDir;
}

bool UGA_AIFire_Auto::BuildViewRayAndMuzzle(const FGameplayAbilityActorInfo* ActorInfo, AAscenduntWeapon* Weapon, FVector& OutViewStart, FVector& OutViewDir,
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

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"

UGA_AIFire_Auto::UGA_AIFire_Auto()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_AIFire_Auto::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	bStopRequested = true;

	// 딜레이가 걸려있으면 끊고 즉시 종료
	if (TickTask)
	{
		TickTask->EndTask();
		TickTask = nullptr;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_AIFire_Auto::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bStopRequested = false;
	ShotIndex = 0;

	// 즉시 1발 쏘고, 능력은 종료하지 말고 루프 유지
	HandleTickFire();
}

void UGA_AIFire_Auto::HandleTickFire()
{
	if (bStopRequested || !CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 1회 트리거 발사 (Base에 만든 TryPrepareShot 사용)
	// 실패(탄 부족 등)면 종료
	FAscenduntWeaponAttackContext Ctx;
	if (!TryPrepareShot(/*DamageCoef*/ 1.f, ShotIndex, Ctx, /*bCommitAbility*/ true))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// FireType 분기
	const AAscenduntWeapon* Weapon = Ctx.Weapon.Get();
	if (!Weapon)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const EAscenduntWeaponFireType FireType = Weapon->GetCurrentModeStats().FireType;

	bool bOk = false;
	switch (FireType)
	{
	case EAscenduntWeaponFireType::Projectile:
		bOk = FireProjectile(Ctx);
		break;
	case EAscenduntWeaponFireType::Hitscan:
	default:
		bOk = FireHitscan(Ctx);
		break;
	}

	if (!bOk)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	++ShotIndex;
	ScheduleNextTick();
}

void UGA_AIFire_Auto::ScheduleNextTick()
{
	if (bStopRequested)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// ROF -> interval
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const float Interval = GetShotIntervalSeconds(ASC);

	TickTask = UAbilityTask_WaitDelay::WaitDelay(this, Interval);
	TickTask->OnFinish.AddDynamic(this, &ThisClass::HandleTickFire);
	TickTask->ReadyForActivation();
}

void UGA_AIFire_Auto::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bStopRequested = true;

	if (TickTask)
	{
		TickTask->EndTask();
		TickTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
