// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "WeaponEquipmentComponent.generated.h"

class AAscenduntWeapon;
class UAscenduntAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAscCurrentWeaponChangedNative, AAscenduntWeapon* /*New*/, AAscenduntWeapon* /*Prev*/);
DECLARE_MULTICAST_DELEGATE(FOnAscWeaponInventoryChangedNative);


USTRUCT(BlueprintType)
struct FAscenduntWeaponSlot
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AAscenduntWeapon> WeaponClass;

	// 장착할 소켓. 기본값은 모든 캐릭터 메시가 공통으로 가진 weapon_r 이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName EquippedSocket = TEXT("weapon_r");

	// 캐릭터에 따른 무기별 오프셋 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FVector WeaponBoneOffset = FVector::ZeroVector;

	// 1P 메시(Mesh1P)의 부착 소켓. Mesh1P 는 3P 와 같은 스켈레톤이므로 기본값도 동일하다.
	// NAME_None 이면 1P 무기 부착 자체를 건너뛴다. (무기가 1P 메시에 이미 포함된 애셋용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName EquippedSocket1P = TEXT("weapon_r");

	// 1P 에서만 적용할 무기 위치 보정. 화면 프레이밍을 맞출 때 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FVector WeaponBoneOffset1P = FVector::ZeroVector;
	// 쌍검의 왼손 칼처럼 CurrentWeapon이 아닌 보조 비주얼 무기 슬롯.
	// 다른 실제 무기가 장착되어 있는 동안 함께 표시되지만 AbilitySet / WeaponStats는 적용하지 않는다.
	// EquipNextWeapon / EquipWeaponByIndex의 선택 대상에서도 제외된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bVisualOnlyCompanion = false;
};

UCLASS( ClassGroup=(ASCENDUNT), meta=(BlueprintSpawnableComponent) )
class ASCENDUNT_API UWeaponEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponEquipmentComponent();

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TArray<FAscenduntWeaponSlot> DefaultWeaponSlots;
	
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AAscenduntWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	/** 무기에 대응하는 슬롯 정의. 인벤토리 인덱스가 DefaultWeaponSlots 인덱스와 1:1 대응한다. */
	const FAscenduntWeaponSlot* GetSlotForWeapon(const AAscenduntWeapon* Weapon) const;

	// 현재 장착된 무기의 뼈 오프셋 값을 애님 BP로 던져주는 블루프린트 노드
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	FVector GetCurrentWeaponBoneOffset() const;

	// ===== Actions =====
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EquipWeaponByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EquipNextWeapon();


	// 서버에서만: 로드아웃 스폰
	void ServerSpawnDefaultWeapons();
	// 서버에서만: 실제 Equip 수행
	void ServerEquipWeapon(AAscenduntWeapon* NewWeapon);

	// 서버에서만: 현재 Pawn이 ASC에 부여한 무기 AbilitySet / WeaponStatsEffect를 회수하고 무기 Actor를 정리한다.
	// 캐릭터 변경으로 Pawn을 Destroy하기 전 반드시 호출해야 발사 Ability / 탄창 스탯이 누적되지 않는다.
	void ServerClearWeaponsFromASC(bool bDestroyWeapons = true);

	/**
	 * 서버에서만: 오브젝트 풀 반납용. 무기 Actor를 파괴하지 않고 ASC 연결만 회수한다.
	 *
	 * 폰이 풀에서 재사용되면 클래스가 같으므로 DefaultWeaponSlots도 같다.
	 * 즉 같은 무기를 다시 스폰하게 되므로, 파괴했다가 새로 만들 이유가 없다.
	 * 사이클마다 반복되던 무기 Actor spawn/destroy를 없앤다.
	 *
	 * Pawn 교체(Hero)에는 쓰지 말 것. 그쪽은 ServerClearWeaponsFromASC(true)가 맞다.
	 */
	void ServerReleaseWeaponsForPool();

	/**
	 * 서버에서만: 로드아웃이 이미 갖춰져 있으면 아무것도 하지 않는다.
	 *
	 * ServerSpawnDefaultWeapons()는 WeaponInventory를 Reset만 하고 기존 Actor를
	 * 파괴하지 않아, 두 번 호출되면 첫 배치가 메시에 붙은 채 인벤토리에서만 사라진다
	 * (파괴 불가능한 유령 무기). 재사용 경로에서는 반드시 이 래퍼를 쓴다.
	 */
	void ServerEnsureDefaultWeapons();

	// 기본 무기를 확보하고, 장착된 무기가 없으면 첫 게임플레이 무기를 장착한다.
	void ServerInitializeDefaultLoadout();

	const TArray<TObjectPtr<AAscenduntWeapon>>& GetWeaponInventory() const { return WeaponInventory; }

	bool HasDefaultWeaponLoadout() const;
	bool IsWeaponLoadoutReady() const;

public:
	FOnAscCurrentWeaponChangedNative OnCurrentWeaponChanged;
	FOnAscWeaponInventoryChangedNative OnWeaponInventoryChanged;

protected:
	virtual void BeginPlay() override;

	// 비주얼 유틸
	void HideWeaponMesh(AAscenduntWeapon* Weapon);
	void ShowWeaponMesh(AAscenduntWeapon* Weapon);
	void AttachWeaponToSocket(AAscenduntWeapon* Weapon, FName Socket);
	void RefreshWeaponVisuals();

	bool IsVisualOnlyCompanion(AAscenduntWeapon* Weapon) const;
	FName GetEquippedSocketForWeapon(AAscenduntWeapon* Weapon) const;
	UAscenduntAbilitySystemComponent* ResolveASC() const;

	// Replication
	UPROPERTY(ReplicatedUsing = OnRep_WeaponInventory)
	TArray<TObjectPtr<AAscenduntWeapon>> WeaponInventory;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AAscenduntWeapon> CurrentWeapon;

	UFUNCTION()
	void OnRep_WeaponInventory();

	UFUNCTION()
	void OnRep_CurrentWeapon(AAscenduntWeapon* PrevWeapon);

	// RPC
	UFUNCTION(Server, Reliable)
	void Server_EquipWeaponByIndex(int32 Index);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
