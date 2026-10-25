// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "AscenduntProjectilePoolSubsystem.generated.h"

USTRUCT()
struct FAscenduntProjectilePoolKey
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AAscenduntProjectile> ProjectileClass;

	// 같은 투사체 클래스를 쓰더라도 캐릭터별로 Pool을 분리하기 위한 소유자 키
	UPROPERTY()
	TObjectPtr<AActor> Owner = nullptr;

	FAscenduntProjectilePoolKey() = default;

	FAscenduntProjectilePoolKey(TSubclassOf<AAscenduntProjectile> InProjectileClass, AActor* InOwner)
		: ProjectileClass(InProjectileClass)
		, Owner(InOwner)
	{
	}

	bool operator==(const FAscenduntProjectilePoolKey& Other) const
	{
		return ProjectileClass == Other.ProjectileClass && Owner == Other.Owner;
	}
};

FORCEINLINE uint32 GetTypeHash(const FAscenduntProjectilePoolKey& Key)
{
	return HashCombine(GetTypeHash(Key.ProjectileClass.Get()), GetTypeHash(Key.Owner.Get()));
}

USTRUCT()
struct FAscenduntProjectilePool
{
	GENERATED_BODY()

	// 비활성 상태로 대기 중인 투사체
	UPROPERTY()
	TArray<TObjectPtr<AAscenduntProjectile>> Inactive;

	// 현재 사용 중인 투사체. 앞쪽 요소가 가장 오래전에 사용된 투사체다.
	UPROPERTY()
	TArray<TObjectPtr<AAscenduntProjectile>> Active;

	UPROPERTY()
	int32 PoolSize = 1;

	UPROPERTY()
	bool bInitialized = false;
};

/**
 * Fixed-size projectile object pool.
 *
 * 같은 투사체 클래스를 사용하더라도 Owner별로 Pool을 분리한다.
 * Inactive 배열에서 투사체를 꺼내고, 부족하면 해당 Owner Pool의 Active 배열에서 가장 오래된 투사체를 재사용한다.
 * 런타임 중 PoolSize를 늘리거나 새 fallback 투사체를 생성하지 않는다.
 */
UCLASS()
class ASCENDUNT_API UAscenduntProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	/** Called once all UWorldSubsystems are about to be deinitialized (소멸)*/
	virtual void Deinitialize() override;

	AAscenduntProjectile* Acquire(
		TSubclassOf<AAscenduntProjectile> Class,
		const FTransform& SpawnTM,
		AActor* Owner,
		APawn* Instigator,
		int32 PoolSize);

	void Release(AAscenduntProjectile* Projectile);

private:
	UPROPERTY()
	TMap<FAscenduntProjectilePoolKey, FAscenduntProjectilePool> Pools;

	FAscenduntProjectilePoolKey MakePoolKey(TSubclassOf<AAscenduntProjectile> Class, AActor* Owner, APawn* Instigator) const;
	void InitializePool(TSubclassOf<AAscenduntProjectile> Class, FAscenduntProjectilePool& Pool, int32 PoolSize);
	void RemoveInvalidEntries(FAscenduntProjectilePool& Pool);
	AAscenduntProjectile* SpawnPooledProjectile(TSubclassOf<AAscenduntProjectile> Class);
	AAscenduntProjectile* TakeOldestActive(FAscenduntProjectilePool& Pool);
	bool ReturnToPool(FAscenduntProjectilePool& Pool, AAscenduntProjectile* Projectile);
	void SetPooledState(AAscenduntProjectile* Projectile, bool bActive);
};
