#include "HakukoGrenadeProjectile.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Utility/AscTargetUtil.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AHakukoGrenadeProjectile::AHakukoGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(10.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 1.f;
	Movement->OnProjectileStop.AddDynamic(this, &ThisClass::HandleProjectileStop);
}

void AHakukoGrenadeProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDebugDrawProjectileCollision && GetNetMode() != NM_DedicatedServer)
	{
		DrawDebugSphere(
			GetWorld(),
			GetActorLocation(),
			DebugCollisionRadius * GetActorScale3D().GetAbsMax(),
			12,
			FColor::Cyan,
			false,
			0.0f,
			0,
			1.0f);
	}
}

void AHakukoGrenadeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHakukoGrenadeProjectile, bDebugDrawProjectileCollision);
	DOREPLIFETIME(AHakukoGrenadeProjectile, bDebugDrawExplosionRadius);
	DOREPLIFETIME(AHakukoGrenadeProjectile, DebugDrawDuration);
	DOREPLIFETIME(AHakukoGrenadeProjectile, DebugCollisionRadius);
}

void AHakukoGrenadeProjectile::OnRep_DebugDrawProjectileCollision()
{
	SetActorTickEnabled(bDebugDrawProjectileCollision);
}

void AHakukoGrenadeProjectile::Multicast_DrawExplosionDebug_Implementation(
	const FVector& ImpactPoint, float Radius, float Duration)
{
	if (GetNetMode() == NM_DedicatedServer || Radius <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	DrawDebugSphere(
		GetWorld(),
		ImpactPoint,
		Radius,
		24,
		FColor::Yellow,
		false,
		FMath::Max(0.0f, Duration),
		0,
		2.0f);
}

void AHakukoGrenadeProjectile::InitializeGrenade(UAbilitySystemComponent* InSourceASC, UObject* InSourceObject,
	EHakukoGrenadePayload InPayload, TSubclassOf<UGameplayEffect> InDamageEffect, TSubclassOf<UGameplayEffect> InHealEffect,
	float InDamage, float InHeal, float InExplosionRadius, const FVector& InitialVelocity, float GravityScale,
	float ProjectileScale, float CollisionRadius, float ActiveDuration, FGameplayTag InExplosionCueTag,
	bool bInDebugDrawProjectileCollision, bool bInDebugDrawExplosionRadius, float InDebugDrawDuration)
{
	check(HasAuthority());
	SourceASC = InSourceASC;
	SourceObject = InSourceObject;
	Payload = InPayload;
	DamageEffectClass = InDamageEffect;
	HealEffectClass = InHealEffect;
	Damage = FMath::Max(0.f, InDamage);
	Heal = FMath::Max(0.f, InHeal);
	ExplosionRadius = FMath::Max(0.f, InExplosionRadius);
	ExplosionCueTag = InExplosionCueTag;
	bDebugDrawProjectileCollision = bInDebugDrawProjectileCollision;
	bDebugDrawExplosionRadius = bInDebugDrawExplosionRadius;
	DebugDrawDuration = FMath::Max(0.0f, InDebugDrawDuration);
	DebugCollisionRadius = FMath::Max(1.0f, CollisionRadius);
	SetActorTickEnabled(bDebugDrawProjectileCollision);

	SetActorScale3D(FVector(FMath::Max(0.01f, ProjectileScale)));
	Collision->SetSphereRadius(FMath::Max(1.f, CollisionRadius));
	if (GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(), true);
	if (GetInstigator()) Collision->IgnoreActorWhenMoving(GetInstigator(), true);

	Movement->ProjectileGravityScale = FMath::Max(0.f, GravityScale);
	Movement->Velocity = InitialVelocity;
	Movement->InitialSpeed = InitialVelocity.Size();
	Movement->MaxSpeed = FMath::Max(InitialVelocity.Size(), 1.f);
	Movement->Activate(true);

	if (ActiveDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(LifeTimer, this, &ThisClass::Expire, ActiveDuration, false);
	}
}

void AHakukoGrenadeProjectile::Expire()
{
	if (HasAuthority() && !bExploded)
	{
		Destroy();
	}
}

void AHakukoGrenadeProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bExploded || !OtherActor || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}
	// Ally/enemy/full-health does not matter for detonation. Payload rules are applied after the sphere query.
	Explode(&SweepResult, OtherActor);
}

void AHakukoGrenadeProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (HasAuthority() && !bExploded)
	{
		Explode(&ImpactResult, ImpactResult.GetActor());
	}
}

void AHakukoGrenadeProjectile::Explode(const FHitResult* DirectHit, AActor* DirectActor)
{
	if (bExploded || !HasAuthority())
	{
		return;
	}
	bExploded = true;
	GetWorldTimerManager().ClearTimer(LifeTimer);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Movement->StopMovementImmediately();
	Movement->Deactivate();

	FVector ImpactPoint = DirectHit ? FVector(DirectHit->ImpactPoint) : GetActorLocation();
	FVector ImpactNormal = DirectHit ? DirectHit->ImpactNormal : FVector::UpVector;
	if (ImpactPoint.IsNearlyZero())
	{
		ImpactPoint = GetActorLocation();
	}
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}
	AActor* SourceActor = GetInstigator() ? Cast<AActor>(GetInstigator()) : GetOwner();

	TSet<AActor*> Applied;
	auto ApplyPayloadToTarget = [this, SourceActor, &Applied, &ImpactPoint](AActor* Target)
	{
		if (!Target || Applied.Contains(Target))
		{
			return;
		}

		// Bio Grenade may heal its own instigator when the instigator is inside the AOE.
		// Secondary Heal Grenade intentionally never heals the shooter.
		if (Target == SourceActor)
		{
			Applied.Add(Target);
			if (Payload == EHakukoGrenadePayload::Biotic)
			{
				ApplyHeal(Target, ImpactPoint);
			}
			return;
		}

		Applied.Add(Target);

		if (UAscTargetUtil::IsFriendlyActor(SourceActor, Target))
		{
			ApplyHeal(Target, ImpactPoint);
		}
		else if (Payload == EHakukoGrenadePayload::Biotic && UAscTargetUtil::IsEnemyActor(SourceActor, Target))
		{
			ApplyDamage(Target, ImpactPoint);
		}
	};

	// The actor that physically triggered the detonation is always considered, even with a zero/small radius.
	ApplyPayloadToTarget(DirectActor ? DirectActor : (DirectHit ? DirectHit->GetActor() : nullptr));

	if (ExplosionRadius > KINDA_SMALL_NUMBER)
	{
		// Do not depend on the owner's collision object channel for self-heal.
		// Bio Grenade explicitly checks whether its instigator is inside the actual AOE.
		if (Payload == EHakukoGrenadePayload::Biotic && SourceActor
			&& FVector::DistSquared(SourceActor->GetActorLocation(), ImpactPoint) <= FMath::Square(ExplosionRadius))
		{
			ApplyPayloadToTarget(SourceActor);
		}

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HakukoGrenadeExplosion), false, SourceActor);
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByObjectType(Overlaps, ImpactPoint, FQuat::Identity, ObjectParams,
			FCollisionShape::MakeSphere(ExplosionRadius), Params);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			ApplyPayloadToTarget(Overlap.GetActor());
		}
	}

	if (bDebugDrawExplosionRadius)
	{
		Multicast_DrawExplosionDebug(ImpactPoint, ExplosionRadius, DebugDrawDuration);
	}

	PlayExplosionCue(ImpactPoint, ImpactNormal);
	Destroy();
}

void AHakukoGrenadeProjectile::ApplyDamage(AActor* TargetActor, const FVector& ImpactPoint)
{
	UAbilitySystemComponent* Source = SourceASC.Get();
	UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!Source || !Target || !DamageEffectClass || Damage <= 0.f) return;

	FHitResult Hit(TargetActor, nullptr, TargetActor->GetActorLocation(), FVector::UpVector);
	Hit.TraceStart = ImpactPoint;
	FGameplayEffectContextHandle Ctx = Source->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject);
	AActor* InstigatorActor = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	Ctx.AddInstigator(InstigatorActor, InstigatorActor);
	Ctx.AddOrigin(ImpactPoint);
	Ctx.AddHitResult(Hit, true);
	FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(DamageEffectClass, 1.f, Ctx);
	if (!Spec.IsValid()) return;
	Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageOverride, Damage);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target);
}

void AHakukoGrenadeProjectile::ApplyHeal(AActor* TargetActor, const FVector& ImpactPoint)
{
	UAbilitySystemComponent* Source = SourceASC.Get();
	UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!Source || !Target || !HealEffectClass || Heal <= 0.f) return;

	FHitResult Hit(TargetActor, nullptr, TargetActor->GetActorLocation(), FVector::UpVector);
	Hit.TraceStart = ImpactPoint;
	FGameplayEffectContextHandle Ctx = Source->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject);
	AActor* InstigatorActor = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	Ctx.AddInstigator(InstigatorActor, InstigatorActor);
	Ctx.AddOrigin(ImpactPoint);
	Ctx.AddHitResult(Hit, true);
	FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(HealEffectClass, 1.f, Ctx);
	if (!Spec.IsValid()) return;
	Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_HealOverride, Heal);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Target);
}

void AHakukoGrenadeProjectile::PlayExplosionCue(const FVector& ImpactPoint, const FVector& ImpactNormal) const
{
	UAscenduntAbilitySystemComponent* AscASC = Cast<UAscenduntAbilitySystemComponent>(SourceASC.Get());
	if (!AscASC || !AscASC->IsOwnerActorAuthoritative() || !ExplosionCueTag.IsValid()) return;

	FGameplayCueParameters P;
	AActor* InstigatorActor = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	P.Instigator = InstigatorActor;
	P.EffectCauser = InstigatorActor;
	P.SourceObject = SourceObject;
	P.Location = ImpactPoint;
	P.Normal = ImpactNormal;
	FGameplayEffectContextHandle Ctx = AscASC->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject);
	Ctx.AddInstigator(InstigatorActor, InstigatorActor);
	Ctx.AddOrigin(ImpactPoint);
	P.EffectContext = Ctx;
	AscASC->ExecuteGameplayCue(ExplosionCueTag, P);
}
