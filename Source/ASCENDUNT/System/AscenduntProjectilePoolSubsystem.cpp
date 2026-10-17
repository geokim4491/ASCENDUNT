// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntProjectilePoolSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UAscenduntProjectilePoolSubsystem::Deinitialize()
{
	Pools.Empty();
	Super::Deinitialize();
}

FAscenduntProjectilePoolKey UAscenduntProjectilePoolSubsystem::MakePoolKey(
	TSubclassOf<AAscenduntProjectile> Class,
	AActor* Owner,
	APawn* Instigator) const
{
	// 일반적으로 OwnerActor는 AvatarActor이므로 캐릭터별 Pool 분리에 사용한다.
	// Owner가 비어 있는 예외 상황에서는 Instigator를 키로 사용한다.
	AActor* PoolOwner = Owner ? Owner : Cast<AActor>(Instigator);
	return FAscenduntProjectilePoolKey(Class, PoolOwner);
}

void UAscenduntProjectilePoolSubsystem::InitializePool(
	TSubclassOf<AAscenduntProjectile> Class,
	FAscenduntProjectilePool& Pool,
	int32 PoolSize)
{
	if (!GetWorld() || !Class)
	{
		return;
	}

	RemoveInvalidEntries(Pool);

	if (!Pool.bInitialized)
	{
		Pool.PoolSize = FMath::Max(1, PoolSize);
		Pool.bInitialized = true;
	}

	while (Pool.Inactive.Num() + Pool.Active.Num() < Pool.PoolSize)
	{
		AAscenduntProjectile* NewProjectile = SpawnPooledProjectile(Class);
		if (!IsValid(NewProjectile))
		{
			break;
		}

		Pool.Inactive.Add(NewProjectile);
	}
}

void UAscenduntProjectilePoolSubsystem::RemoveInvalidEntries(FAscenduntProjectilePool& Pool)
{
	for (int32 i = Pool.Inactive.Num() - 1; i >= 0; --i)
	{
		if (!IsValid(Pool.Inactive[i]))
		{
			Pool.Inactive.RemoveAt(i, 1, EAllowShrinking::No);
		}
	}

	for (int32 i = Pool.Active.Num() - 1; i >= 0; --i)
	{
		if (!IsValid(Pool.Active[i]))
		{
			Pool.Active.RemoveAt(i, 1, EAllowShrinking::No);
		}
	}
}

AAscenduntProjectile* UAscenduntProjectilePoolSubsystem::SpawnPooledProjectile(TSubclassOf<AAscenduntProjectile> Class)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAscenduntProjectile* Projectile = World->SpawnActor<AAscenduntProjectile>(Class, FTransform::Identity, Params);
	if (!IsValid(Projectile))
	{
		return nullptr;
	}

	Projectile->SetPoolSubsystem(this);
	SetPooledState(Projectile, false);
	return Projectile;
}

AAscenduntProjectile* UAscenduntProjectilePoolSubsystem::Acquire(
	TSubclassOf<AAscenduntProjectile> Class,
	const FTransform& SpawnTM,
	AActor* Owner,
	APawn* Instigator,
	int32 PoolSize)
{
	if (!GetWorld() || !Class)
	{
		return nullptr;
	}

	const FAscenduntProjectilePoolKey PoolKey = MakePoolKey(Class, Owner, Instigator);
	FAscenduntProjectilePool& Pool = Pools.FindOrAdd(PoolKey);
	InitializePool(Class, Pool, PoolSize);

	AAscenduntProjectile* Projectile = nullptr;
	if (Pool.Inactive.Num() > 0)
	{
		Projectile = Pool.Inactive.Pop(EAllowShrinking::No);
	}
	else
	{
		Projectile = TakeOldestActive(Pool);
	}

	if (!IsValid(Projectile))
	{
		return nullptr;
	}

	Pool.Active.Add(Projectile);

	Projectile->SetOwner(Owner);
	Projectile->SetInstigator(Instigator);
	Projectile->SetActorTransform(SpawnTM, false, nullptr, ETeleportType::TeleportPhysics);

	SetPooledState(Projectile, true);
	Projectile->OnAcquiredFromPool();

	return Projectile;
}

AAscenduntProjectile* UAscenduntProjectilePoolSubsystem::TakeOldestActive(FAscenduntProjectilePool& Pool)
{
	while (Pool.Active.Num() > 0)
	{
		AAscenduntProjectile* Projectile = Pool.Active[0];
		Pool.Active.RemoveAt(0, 1, EAllowShrinking::No);

		if (!IsValid(Projectile))
		{
			continue;
		}

		Projectile->OnReturnedToPool();
		SetPooledState(Projectile, false);
		return Projectile;
	}

	return nullptr;
}

bool UAscenduntProjectilePoolSubsystem::ReturnToPool(FAscenduntProjectilePool& Pool, AAscenduntProjectile* Projectile)
{
	if (Pool.Active.RemoveSingle(Projectile) <= 0)
	{
		return false;
	}

	Projectile->OnReturnedToPool();
	SetPooledState(Projectile, false);
	Pool.Inactive.Add(Projectile);
	return true;
}

void UAscenduntProjectilePoolSubsystem::Release(AAscenduntProjectile* Projectile)
{
	if (!IsValid(Projectile))
	{
		return;
	}

	const FAscenduntProjectilePoolKey PoolKey = MakePoolKey(Projectile->GetClass(), Projectile->GetOwner(), Projectile->GetInstigator());
	if (FAscenduntProjectilePool* Pool = Pools.Find(PoolKey))
	{
		if (ReturnToPool(*Pool, Projectile))
		{
			return;
		}
	}

	// Owner가 바뀌었거나 예외적으로 키를 찾지 못한 경우, 전체 Pool에서 한 번 더 찾아 안전하게 반환한다.
	for (TPair<FAscenduntProjectilePoolKey, FAscenduntProjectilePool>& Pair : Pools)
	{
		if (ReturnToPool(Pair.Value, Projectile))
		{
			return;
		}
	}
}

void UAscenduntProjectilePoolSubsystem::SetPooledState(AAscenduntProjectile* Projectile, bool bActive)
{
	if (!Projectile)
	{
		return;
	}

	if (Projectile->GetIsReplicated())
	{
		Projectile->FlushNetDormancy();
		Projectile->SetNetDormancy(DORM_Awake);
	}

	Projectile->SetActiveState(bActive);

	if (Projectile->GetIsReplicated())
	{
		Projectile->ForceNetUpdate();

		if (!bActive)
		{
			FTimerDelegate DormancyDelegate;
			DormancyDelegate.BindUObject(Projectile, &AActor::SetNetDormancy, DORM_DormantAll);
			GetWorld()->GetTimerManager().SetTimerForNextTick(DormancyDelegate);
		}
	}
}
