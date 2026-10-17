// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayEffectTypes.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySourceInterface.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySet.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AscenduntWeapon.generated.h"

class UWeaponDefinitionData;
class UAscenduntAbilitySystemComponent;
class UMaterialInterface;
class UMeshComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, int, CurrentAmmo, int, MaxAmmo);
DECLARE_MULTICAST_DELEGATE_TwoParams(FAscMeleeHitWindowNative, bool /*bBegin*/, int32 /*StepIndex*/);

UCLASS()
class ASCENDUNT_API AAscenduntWeapon : public AActor, public IAscenduntAbilitySourceInterface
{
	GENERATED_BODY()
	
public:	
	AAscenduntWeapon();

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UWeaponDefinitionData> WeaponData;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> StaticWeaponMesh;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Weapon|XRay")
	TObjectPtr<USkeletalMeshComponent> XRayWeaponMesh;

	/**
	 * 1인칭 전용 무기 메시. 로컬 오너에게만 보이고, Mesh1P 의 손 소켓에 부착된다.
	 *
	 * 3P 메시(WeaponMesh)와 같은 애셋을 BeginPlay 에서 자동 복사하고, 포즈는
	 * LeaderPoseComponent 로 따라간다. 같은 총이 같은 동작을 하는 것이므로
	 * 1P 전용 무기 몽타주 경로를 따로 만들 필요가 없다. (XRayWeaponMesh 와 같은 패턴)
	 *
	 * 부착과 가시성은 UHeroFirstPersonComponent 가 소유한다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Weapon|FirstPerson")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh1P;
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Weapon|XRay")
	TObjectPtr<UStaticMeshComponent> XRayStaticWeaponMesh;

	UPROPERTY(BlueprintAssignable)
	FOnAmmoChanged OnAmmoChanged;

	// GA_
	// 서버 인스턴스가 이 delegate를 바인딩해서 HitWindow 시작/종료를 받음
	FAscMeleeHitWindowNative OnMeleeHitWindow;

	// 투사체 발사 시작점(총구)
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Projectile")
	FName MuzzleSocketName = TEXT("Muzzle");

public:
	// Equip / Unequip (서버에서 호출 권장)
	void EquipToASC(UAscenduntAbilitySystemComponent* AscenduntASC);
	void UnequipFromASC(UAscenduntAbilitySystemComponent* AscenduntASC);

	/**
	 * 오브젝트 풀 재사용 전에 무기의 런타임 상태를 초기화한다. 서버 전용.
	 *
	 * AI는 폰과 무기를 파괴하지 않고 재사용하므로, 이전 생애의 상태가 그대로 넘어간다.
	 * 특히 bReloading이 true로 고착되면 다음 생애에 발사가 아예 안 된다.
	 *
	 * 탄약(CurrentAmmoInMag / MagSize)은 여기서 건드리지 않는다.
	 * 재장착 시 EquipToASC -> InitAmmoFromAttributes 가 복원한다.
	 *
	 * 델리게이트 정리는 원칙적으로 "바인딩한 어빌리티가 EndAbility에서 언바인딩"한다
	 * (GA_Melee::UnbindWeaponHitWindow 참고). 여기의 Clear는 그게 누락됐을 때의 안전망이다.
	 * 무기에 새 런타임 상태나 델리게이트를 추가하면 여기에도 반영할 것.
	 *
	 * OnAmmoChanged는 일부러 건드리지 않는다. HUD가 소유하고 스스로 해제하는 바인딩이다.
	 */
	void ResetForReuse();

	/** 1P 무기 메시에 3P 애셋과 LeaderPose 를 물린다. BeginPlay 에서 호출. */
	void SyncWeaponMesh1P();

	UFUNCTION(BlueprintCallable)
	USkeletalMeshComponent* GetWeaponMesh() const;

	UFUNCTION(BlueprintCallable)
	UStaticMeshComponent* GetStaticWeaponMesh() const;

	// 실제 에셋이 지정된 무기 메시를 반환한다.
	// 기존 무기 호환성을 위해 둘 다 지정된 경우 SkeletalMesh를 우선한다.
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	UMeshComponent* GetActiveWeaponMesh() const;

	/**
	 * 무기 모드를 전환한다. 서버 전용.
	 *
	 * 사격 리듬 / 데미지 / 탄창 / 몽타주 / 투사체 / VFX 는 모두
	 * GetCurrentModeStats() 를 거쳐 읽으므로, 인덱스만 바꾸면 전부 함께 전환된다.
	 *
	 * 재장전 중에는 거부한다. 탄약 상태가 어긋나기 때문이다.
	 * 모드 전환이 재장전을 대신하지 않도록 잔탄은 새 탄창 크기로 자르기만 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Mode")
	bool SetWeaponModeByIndex(int32 NewModeIndex);

	/** ModeTag 로 전환한다. 일치하는 Mode 가 없으면 false. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Mode")
	bool SetWeaponModeByTag(FGameplayTag ModeTag);

	UFUNCTION(BlueprintPure, Category = "Weapon|Mode")
	FGameplayTag GetCurrentModeTag() const;

	UFUNCTION(BlueprintPure, Category = "Weapon|Mode")
	int32 GetCurrentModeIndex() const { return CurrentModeIndex; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|XRay")
	USkeletalMeshComponent* GetXRayWeaponMesh() const { return XRayWeaponMesh; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|FirstPerson")
	USkeletalMeshComponent* GetWeaponMesh1P() const { return WeaponMesh1P; }

	// X-Ray 전용 무기 메시를 OutMeshes에 추가한다. Character의 GatherXRayMeshes에서 호출한다.
	void GatherXRayMeshes(TArray<UMeshComponent*>& OutMeshes) const;

	void SetXRayMeshEnabled(bool bEnabled, float HealthNorm = 1.f, float XRayAlpha = 1.f);
	void SetXRayMaterial(UMaterialInterface* Material);
	bool AreXRayMaterialsReady() const { return bXRayMaterialReady; }

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayWeaponMontage(UAnimMontage* Montage, float PlayRate);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_StopWeaponMontage(float BlendOutTime);

	/** Development-only visualization for snapshot weapon AOE impacts. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DrawAOEDebugSphere(FVector_NetQuantize ImpactPoint, float Radius, float Duration);

	// NotifyState가 호출하는 엔트리 (클라면 Server RPC로 전달)
	void NotifyMeleeHitWindow(bool bBegin, int32 StepIndex);

	//  ======== Reload ========
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetCurrentAmmoInMag() const { return CurrentAmmoInMag; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetMagSize() const { return MagSize; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool IsReloading() const { return bReloading; }

	// 서버 권한에서 확정 상태 변경(복제됨)
	void SetReloading_Server(bool bInReloading);
	void SetAmmoInMag_Server(int32 NewAmmo);

	// 오너클라 예측용(로컬 즉시 반영 + UI 훅 호출)
	void SetReloading_Predicted(bool bInReloading);
	void SetAmmoInMag_Predicted(int32 NewAmmo);

	// IAscenduntAbilitySourceInterface
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;
	virtual float GetHeadshotDamageMultiplier(const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;

	// ========== Helpers ===============
	UFUNCTION(BlueprintCallable)
	const struct FAscenduntWeaponModeStats& GetCurrentModeStats() const;

	/**
	 * 게임플레이용 총구 Transform. 항상 3P WeaponMesh 기준이다.
	 * 서버 투사체 스폰/판정처럼 모든 머신에서 동일한 기준이 필요한 경로에서 사용한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Projectile")
	FTransform GetMuzzleTransform() const;

	/**
	 * 연출용 총구 Transform.
	 * 로컬에서 1P 무기가 실제로 보이는 중이면 WeaponMesh1P의 Muzzle을 사용하고,
	 * 그 외(서버/원격 클라이언트/3P)는 기존 WeaponMesh의 Muzzle을 사용한다.
	 *
	 * Muzzle Flash / Trail처럼 화면에 보이는 연출 전용으로만 사용한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|FirstPerson")
	FTransform GetMuzzleVFXTransform() const;


	/**
	 * 총구 이펙트를 붙일 메시. 1인칭으로 보이는 중이면 1P 무기, 아니면 3P 무기를 준다.
	 *
	 * 게임플레이 큐(GCN_Predicted_Weapon_Fire 등)에서 GetWeaponMesh 대신 이걸 쓴다.
	 * 큐가 소켓에 직접 Attach 하는 구조라, 3P 메시를 잡으면 1인칭에서 총구 화염이
	 * 몸 쪽 3인칭 총에서 나온다.
	 *
	 * 연출 전용이다. 투사체 스폰이나 발사 방향 계산에 쓰면 서버와 예측이 어긋난다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|FirstPerson")
	USkeletalMeshComponent* GetMuzzleAttachMesh() const;

	/**
	 * 이 머신이 이 무기를 1인칭으로 그리고 있는가.
	 *
	 * WeaponMesh1P 는 UHeroFirstPersonComponent 가 1인칭일 때만 부착하고 보이게 한다.
	 * 따라서 "부착됨 + 보임" 이 곧 1인칭 렌더 중이라는 뜻이다.
	 * 서버와 다른 클라이언트에서는 숨겨진 채 액터 루트에 있으므로 false 다.
	 */
	bool IsFirstPersonWeaponVisible() const;

	bool IsInfiniteMag() const;
	bool CanFire(int32 AmmoCost) const { return !bReloading && (IsInfiniteMag() || CurrentAmmoInMag >= AmmoCost); }

	void ConsumeAmmo_Server(int32 AmmoCost);

	UAscenduntAbilitySystemComponent* GetCachedAscASC() const;

	bool CanFireByTime(bool bAuthority, double Now) const
	{
		const double Next = bAuthority ? NextAllowedFireTimeServer : NextAllowedFireTimePredicted;
		return Now >= Next;
	}

	double GetTimeUntilFire(bool bAuthority, double Now) const
	{
		const double Next = bAuthority ? NextAllowedFireTimeServer : NextAllowedFireTimePredicted;
		return FMath::Max(0.0, Next - Now);
	}

	void MarkFired(bool bAuthority, double Now, double Interval)
	{
		if (bAuthority) NextAllowedFireTimeServer = Now + Interval;
		else            NextAllowedFireTimePredicted = Now + Interval;
	}

	float GetMaxRange() const; // WeaponData 기반

	void GetServerMeleeWindowState(bool& OutActive, int32& OutStepIndex) const
	{
		OutActive = bServerMeleeWindowActive;
		OutStepIndex = ServerMeleeWindowStepIndex;
	}

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_NotifyMeleeHitWindow(bool bBegin, int32 StepIndex);

	void HandleMeleeHitWindow_Server(bool bBegin, int32 StepIndex);

	UFUNCTION()
	void OnRep_CurrentModeIndex();

	/** 모드가 바뀐 뒤 서버/클라 공통으로 필요한 갱신. */
	void HandleModeChanged();

	UFUNCTION()
	void OnRep_CurrentAmmoInMag();

	UFUNCTION()
	void OnRep_MagSize();

	UFUNCTION()
	void OnRep_Reloading();

private:
	// AbilitySet 회수용
	UPROPERTY()
	FAscenduntAbilitySet_GrantedHandles GrantedHandles;

	// 무기 스탯 GE 회수용
	FActiveGameplayEffectHandle WeaponStatsGEHandle;

	// --- Replicated runtime state ---
	UPROPERTY(ReplicatedUsing = OnRep_CurrentModeIndex)
	int32 CurrentModeIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmoInMag)
	int32 CurrentAmmoInMag = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MagSize)
	int32 MagSize = 0;

	// 예측 setter가 OnRep를 직접 호출할 수 있게 ReplicatedUsing
	UPROPERTY(ReplicatedUsing = OnRep_Reloading)
	bool bReloading = false;

	// 아래 두 변수는 발사 가능 여부 판단용으로, 서버는 실제 발사 시점 기준으로, 클라는 발사 시점 예측용으로 사용.
	double NextAllowedFireTimeServer = 0.0;      // 서버용
	double NextAllowedFireTimePredicted = 0.0;   // 오너클라 예측용

	bool bServerMeleeWindowActive = false;
	int32 ServerMeleeWindowStepIndex = 0;

	UPROPERTY()
	TObjectPtr<UAscenduntAbilitySystemComponent> CachedAscASC;
private:
	// 내부: WeaponDef -> SetByCaller로 스탯을 밀어넣는 함수
	void ApplyWeaponStatsEffect(UAscenduntAbilitySystemComponent* AscenduntASC);
	void RemoveWeaponStatsEffect(UAscenduntAbilitySystemComponent* AscenduntASC);

	void InitAmmoFromAttributes(UAscenduntAbilitySystemComponent* AscenduntASC);

	void CreateRuntimeXRayMeshes();
	void SyncXRayWeaponMesh();
	void ApplyXRayMaterialToMesh(UMeshComponent* MeshComponent, UMaterialInterface* Material) const;
	bool bXRayMaterialReady = false;
};
