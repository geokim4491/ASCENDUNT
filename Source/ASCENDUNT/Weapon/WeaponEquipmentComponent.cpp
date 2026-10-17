// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponEquipmentComponent.h"

#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"

UWeaponEquipmentComponent::UWeaponEquipmentComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

// ★ 추가된 함수: 애님 BP가 매 프레임 이 노드를 호출해 무기별 오프셋 수치를 빨아갑니다.
FVector UWeaponEquipmentComponent::GetCurrentWeaponBoneOffset() const
{
	if (!CurrentWeapon) return FVector::ZeroVector;

	// 현재 무기가 인벤토리의 몇 번째 칸에 있는지 확인하여 슬롯 데이터와 매칭합니다.
	const int32 Idx = WeaponInventory.IndexOfByKey(CurrentWeapon);
	if (DefaultWeaponSlots.IsValidIndex(Idx))
	{
		return DefaultWeaponSlots[Idx].WeaponBoneOffset;
	}

	return FVector::ZeroVector;
}

void UWeaponEquipmentComponent::EquipWeaponByIndex(int32 Index)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (!WeaponInventory.IsValidIndex(Index) || !DefaultWeaponSlots.IsValidIndex(Index))
	{
		return;
	}

	// 보조 칼은 비주얼 전용이므로 CurrentWeapon으로 선택하지 않는다.
	if (DefaultWeaponSlots[Index].bVisualOnlyCompanion)
	{
		return;
	}

	if (Owner->HasAuthority())
	{
		ServerEquipWeapon(WeaponInventory[Index]);
		return;
	}

	Server_EquipWeaponByIndex(Index);
}

void UWeaponEquipmentComponent::EquipNextWeapon()
{
	if (WeaponInventory.Num() <= 1) return;

	const int32 CurIndex = WeaponInventory.IndexOfByKey(CurrentWeapon);

	// 비주얼 전용 보조 슬롯은 무기 전환 대상에서 건너뛴다.
	for (int32 Offset = 1; Offset <= WeaponInventory.Num(); ++Offset)
	{
		const int32 NextIndex = (CurIndex >= 0)
			? (CurIndex + Offset) % WeaponInventory.Num()
			: (Offset - 1);

		if (!DefaultWeaponSlots.IsValidIndex(NextIndex) || DefaultWeaponSlots[NextIndex].bVisualOnlyCompanion)
		{
			continue;
		}

		EquipWeaponByIndex(NextIndex);
		return;
	}
}

bool UWeaponEquipmentComponent::HasDefaultWeaponLoadout() const
{
	return DefaultWeaponSlots.ContainsByPredicate([](const FAscenduntWeaponSlot& Slot)
	{
		return Slot.WeaponClass != nullptr;
	});
}

bool UWeaponEquipmentComponent::IsWeaponLoadoutReady() const
{
	int32 RequiredWeaponCount = 0;
	bool bRequiresGameplayWeapon = false;

	for (const FAscenduntWeaponSlot& Slot : DefaultWeaponSlots)
	{
		if (!Slot.WeaponClass)
		{
			continue;
		}

		++RequiredWeaponCount;
		bRequiresGameplayWeapon |= !Slot.bVisualOnlyCompanion;
	}

	if (RequiredWeaponCount == 0)
	{
		return true;
	}

	// GameplayReady means the whole configured loadout is present, including visual-only
	// companion weapons. A partially replicated loadout must not expose the Hero yet.
	if (WeaponInventory.Num() != RequiredWeaponCount)
	{
		return false;
	}

	for (const AAscenduntWeapon* Weapon : WeaponInventory)
	{
		if (!IsValid(Weapon))
		{
			return false;
		}
	}

	if (!bRequiresGameplayWeapon)
	{
		return true;
	}

	return IsValid(CurrentWeapon) &&
		WeaponInventory.Contains(CurrentWeapon) &&
		!IsVisualOnlyCompanion(CurrentWeapon);
}

void UWeaponEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWeaponEquipmentComponent::ServerSpawnDefaultWeapons()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar) return;

	UWorld* World = GetWorld();
	if (!World) return;

	WeaponInventory.Reset();

	for (const FAscenduntWeaponSlot& Slot : DefaultWeaponSlots)
	{
		if (!Slot.WeaponClass) continue;

		FActorSpawnParameters Params;
		Params.Owner = OwnerChar;
		Params.Instigator = OwnerChar;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AAscenduntWeapon* Weapon = World->SpawnActor<AAscenduntWeapon>(Slot.WeaponClass, Params);
		if (!Weapon) continue;

		Weapon->SetOwner(OwnerChar);

		// 일단 해당 무기의 EquippedSocket에 붙여두고(원하는 소켓 위치에 대기),
		// 보이지만 않게 숨김 처리
		AttachWeaponToSocket(Weapon, Slot.EquippedSocket);
		HideWeaponMesh(Weapon);

		WeaponInventory.Add(Weapon);
	}

	OnWeaponInventoryChanged.Broadcast();
}

void UWeaponEquipmentComponent::ServerEquipWeapon(AAscenduntWeapon* NewWeapon)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	if (!NewWeapon || !WeaponInventory.Contains(NewWeapon)) return;
	if (IsVisualOnlyCompanion(NewWeapon)) return;
	if (CurrentWeapon == NewWeapon) return;

	UAscenduntAbilitySystemComponent* ASC = ResolveASC();

	AAscenduntWeapon* Prev = CurrentWeapon;

	// 1) 이전 무기 AbilitySet/스탯 해제(서버만)
	if (Prev && ASC)
	{
		Prev->UnequipFromASC(ASC);
	}

	// 2) 새 무기를 CurrentWeapon으로 교체한다.
	// 실제 표시 여부는 CurrentWeapon + bVisualOnlyCompanion 슬롯을 한 번에 갱신한다.
	CurrentWeapon = NewWeapon;
	RefreshWeaponVisuals();

	// 4) 새 무기 AbilitySet/스탯 적용(서버만) - SourceObject = Weapon이 여기서 박힘
	if (ASC)
	{
		NewWeapon->EquipToASC(ASC);
	}

	OnCurrentWeaponChanged.Broadcast(CurrentWeapon, Prev);
}

void UWeaponEquipmentComponent::ServerClearWeaponsFromASC(bool bDestroyWeapons)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	UAscenduntAbilitySystemComponent* ASC = ResolveASC();
	AAscenduntWeapon* PrevWeapon = CurrentWeapon;

	TArray<AAscenduntWeapon*> WeaponsToClean;
	if (CurrentWeapon)
	{
		WeaponsToClean.AddUnique(CurrentWeapon);
	}

	for (AAscenduntWeapon* Weapon : WeaponInventory)
	{
		if (Weapon)
		{
			WeaponsToClean.AddUnique(Weapon);
		}
	}

	// ASC는 PlayerState에 붙어 있으므로 Pawn 교체 시에도 유지된다.
	// 따라서 이전 Pawn의 무기 AbilitySet / WeaponStatsEffect는 명시적으로 회수해야 한다.
	for (AAscenduntWeapon* Weapon : WeaponsToClean)
	{
		if (!IsValid(Weapon))
		{
			continue;
		}

		if (ASC)
		{
			Weapon->UnequipFromASC(ASC);
		}

		if (bDestroyWeapons)
		{
			Weapon->Destroy();
		}
		else
		{
			HideWeaponMesh(Weapon);
		}
	}

	CurrentWeapon = nullptr;
	WeaponInventory.Reset();

	OnWeaponInventoryChanged.Broadcast();
	OnCurrentWeaponChanged.Broadcast(nullptr, PrevWeapon);
}

void UWeaponEquipmentComponent::ServerReleaseWeaponsForPool()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	UAscenduntAbilitySystemComponent* ASC = ResolveASC();
	AAscenduntWeapon* PrevWeapon = CurrentWeapon;

	for (AAscenduntWeapon* Weapon : WeaponInventory)
	{
		if (!IsValid(Weapon))
		{
			continue;
		}

		if (ASC)
		{
			Weapon->UnequipFromASC(ASC);
		}

		Weapon->ResetForReuse();

		// 메시 단위가 아니라 액터 단위로 숨긴다.
		// HideWeaponMesh는 WeaponMesh(스켈레탈)만 숨겨서 StaticWeaponMesh를 쓰는
		// 무기가 죽은 자리에 남는다. 무기는 폰과 별개 액터라 폰을 숨겨도 따라오지 않는다.
		Weapon->SetActorHiddenInGame(true);
		if (Weapon->GetNetMode() != NM_DedicatedServer)
		{
			Weapon->SetXRayMeshEnabled(false);
		}
	}

	// WeaponInventory는 유지한다. 재사용 시 그대로 다시 장착한다.
	CurrentWeapon = nullptr;

	OnCurrentWeaponChanged.Broadcast(nullptr, PrevWeapon);
}

void UWeaponEquipmentComponent::ServerEnsureDefaultWeapons()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 풀에 있는 동안 무기가 외부에서 파괴됐을 수 있다.
	WeaponInventory.RemoveAll([](const TObjectPtr<AAscenduntWeapon>& W)
	{
		return !IsValid(W);
	});

	if (WeaponInventory.Num() > 0)
	{
		// 보유 중인 무기를 다시 쓴다. 다시 보이게만 해준다.
		for (AAscenduntWeapon* Weapon : WeaponInventory)
		{
			Weapon->SetActorHiddenInGame(false);
		}
		return;
	}

	ServerSpawnDefaultWeapons();
}

void UWeaponEquipmentComponent::ServerInitializeDefaultLoadout()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	ServerEnsureDefaultWeapons();
	if (CurrentWeapon)
	{
		return;
	}

	// 비주얼 전용 보조 무기는 ServerEquipWeapon에서 제외된다.
	for (AAscenduntWeapon* Weapon : WeaponInventory)
	{
		ServerEquipWeapon(Weapon);
		if (CurrentWeapon)
		{
			break;
		}
	}
}

void UWeaponEquipmentComponent::HideWeaponMesh(AAscenduntWeapon* Weapon)
{
	if (!Weapon) return;

	if (USkeletalMeshComponent* SkeletalMesh = Weapon->GetWeaponMesh())
	{
		SkeletalMesh->SetHiddenInGame(true);
	}
	if (UStaticMeshComponent* StaticMesh = Weapon->GetStaticWeaponMesh())
	{
		StaticMesh->SetHiddenInGame(true);
	}

	if (Weapon->GetNetMode() != NM_DedicatedServer)
	{
		Weapon->SetXRayMeshEnabled(false);
	}
}

void UWeaponEquipmentComponent::ShowWeaponMesh(AAscenduntWeapon* Weapon)
{
	if (!Weapon) return;

	// 둘 다 지정되는 실수를 해도 실제 사용 메시 하나만 보이도록 한다.
	if (USkeletalMeshComponent* SkeletalMesh = Weapon->GetWeaponMesh())
	{
		SkeletalMesh->SetHiddenInGame(true);
	}
	if (UStaticMeshComponent* StaticMesh = Weapon->GetStaticWeaponMesh())
	{
		StaticMesh->SetHiddenInGame(true);
	}
	if (UMeshComponent* ActiveMesh = Weapon->GetActiveWeaponMesh())
	{
		ActiveMesh->SetHiddenInGame(false);
	}

	// X-Ray 무기 메시의 표시 여부는 AscVisionLocalPlayerSubsystem이 팀/로컬 상태에 따라 결정한다.
	if (Weapon->GetNetMode() != NM_DedicatedServer)
	{
		Weapon->SetXRayMeshEnabled(false);
	}
}


void UWeaponEquipmentComponent::RefreshWeaponVisuals()
{
	for (AAscenduntWeapon* Weapon : WeaponInventory)
	{
		if (!IsValid(Weapon))
		{
			continue;
		}

		const bool bShouldBeVisible =
			Weapon == CurrentWeapon ||
			(IsValid(CurrentWeapon) && IsVisualOnlyCompanion(Weapon));

		AttachWeaponToSocket(Weapon, GetEquippedSocketForWeapon(Weapon));

		if (bShouldBeVisible)
		{
			ShowWeaponMesh(Weapon);
		}
		else
		{
			HideWeaponMesh(Weapon);
		}
	}

	// Replication 순서상 CurrentWeapon이 WeaponInventory보다 먼저 도착할 수 있다.
	// 그 경우에도 주 무기 하나는 즉시 보이도록 보정한다.
	if (IsValid(CurrentWeapon) && !WeaponInventory.Contains(CurrentWeapon))
	{
		AttachWeaponToSocket(CurrentWeapon, GetEquippedSocketForWeapon(CurrentWeapon));
		ShowWeaponMesh(CurrentWeapon);
	}
}

bool UWeaponEquipmentComponent::IsVisualOnlyCompanion(AAscenduntWeapon* Weapon) const
{
	const int32 Idx = WeaponInventory.IndexOfByKey(Weapon);
	return DefaultWeaponSlots.IsValidIndex(Idx) &&
		DefaultWeaponSlots[Idx].bVisualOnlyCompanion;
}

void UWeaponEquipmentComponent::AttachWeaponToSocket(AAscenduntWeapon* Weapon, FName Socket)
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !Weapon) return;

	// 순정 상태(SnapToTarget)를 유지하여 총을 소켓 원점에 붙입니다.
	Weapon->AttachToComponent(
		OwnerChar->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		Socket
	);

	// 부착된 무기의 위치는 AttachmentReplication이 부모/소켓/상대 트랜스폼으로 처리한다.
	// 이동 복제까지 켜두면 무기마다 위치를 따로 복제하게 되어 순수 낭비다.
	// 나중에 무기를 떨어뜨리는(Detach) 기능을 넣으면 그 시점에 다시 켜야 한다.
	Weapon->SetReplicateMovement(false);
}

const FAscenduntWeaponSlot* UWeaponEquipmentComponent::GetSlotForWeapon(const AAscenduntWeapon* Weapon) const
{
	const int32 Idx = WeaponInventory.IndexOfByKey(Weapon);
	return DefaultWeaponSlots.IsValidIndex(Idx) ? &DefaultWeaponSlots[Idx] : nullptr;
}

FName UWeaponEquipmentComponent::GetEquippedSocketForWeapon(AAscenduntWeapon* Weapon) const
{
	if (const FAscenduntWeaponSlot* Slot = GetSlotForWeapon(Weapon))
	{
		return Slot->EquippedSocket;
	}

	// fallback. weapon_r 은 모든 캐릭터 메시가 공통으로 가진 부착점이다.
	return TEXT("weapon_r");
}

UAscenduntAbilitySystemComponent* UWeaponEquipmentComponent::ResolveASC() const
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar) return nullptr;

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerChar))
	{
		return Cast<UAscenduntAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
	}

	return nullptr;
}

void UWeaponEquipmentComponent::OnRep_WeaponInventory()
{
	RefreshWeaponVisuals();
	OnWeaponInventoryChanged.Broadcast();
}

void UWeaponEquipmentComponent::OnRep_CurrentWeapon(AAscenduntWeapon* PrevWeapon)
{
	// 클라: CurrentWeapon과 비주얼 전용 보조 슬롯을 함께 동기화한다.
	// ASC / AbilitySet 적용은 서버에서만 수행한다.
	RefreshWeaponVisuals();

	OnCurrentWeaponChanged.Broadcast(CurrentWeapon, PrevWeapon);
}

void UWeaponEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWeaponEquipmentComponent, WeaponInventory);
	DOREPLIFETIME(UWeaponEquipmentComponent, CurrentWeapon);
}

void UWeaponEquipmentComponent::Server_EquipWeaponByIndex_Implementation(int32 Index)
{
	if (WeaponInventory.IsValidIndex(Index))
	{
		ServerEquipWeapon(WeaponInventory[Index]);
	}
}
