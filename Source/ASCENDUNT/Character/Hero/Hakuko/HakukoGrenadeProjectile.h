#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "HakukoGrenadeProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;
class UAbilitySystemComponent;
class UGameplayEffect;

enum class EAscenduntCueRoute : uint8;

UENUM()
enum class EHakukoGrenadePayload : uint8
{
	Biotic,
	HealOnly
};

/** Server-authoritative explosive grenade. Movement replicates; all overlap/team/effect decisions are server-only. */
UCLASS(Blueprintable)
class ASCENDUNT_API AHakukoGrenadeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AHakukoGrenadeProjectile();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeGrenade(
		UAbilitySystemComponent* InSourceASC,
		UObject* InSourceObject,
		EHakukoGrenadePayload InPayload,
		TSubclassOf<UGameplayEffect> InDamageEffect,
		TSubclassOf<UGameplayEffect> InHealEffect,
		float InDamage,
		float InHeal,
		float InExplosionRadius,
		const FVector& InitialVelocity,
		float GravityScale,
		float ProjectileScale,
		float CollisionRadius,
		float ActiveDuration,
		FGameplayTag InExplosionCueTag,
		bool bInDebugDrawProjectileCollision,
		bool bInDebugDrawExplosionRadius,
		float InDebugDrawDuration);

protected:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

private:
	void Explode(const FHitResult* DirectHit, AActor* DirectActor = nullptr);
	void ApplyDamage(AActor* TargetActor, const FVector& ImpactPoint);
	void ApplyHeal(AActor* TargetActor, const FVector& ImpactPoint);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DrawExplosionDebug(const FVector& ImpactPoint, float Radius, float Duration);

	UFUNCTION()
	void OnRep_DebugDrawProjectileCollision();
	void PlayExplosionCue(const FVector& ImpactPoint, const FVector& ImpactNormal) const;
	void Expire();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	UPROPERTY() TObjectPtr<UObject> SourceObject;
	UPROPERTY() TSubclassOf<UGameplayEffect> DamageEffectClass;
	UPROPERTY() TSubclassOf<UGameplayEffect> HealEffectClass;
	EHakukoGrenadePayload Payload = EHakukoGrenadePayload::Biotic;
	float Damage = 0.f;
	float Heal = 0.f;
	float ExplosionRadius = 0.f;
	FGameplayTag ExplosionCueTag;

	UPROPERTY(ReplicatedUsing=OnRep_DebugDrawProjectileCollision)
	bool bDebugDrawProjectileCollision = false;

	UPROPERTY(Replicated)
	bool bDebugDrawExplosionRadius = false;

	UPROPERTY(Replicated)
	float DebugDrawDuration = 2.0f;

	UPROPERTY(Replicated)
	float DebugCollisionRadius = 10.0f;

	FTimerHandle LifeTimer;
	bool bExploded = false;
};
