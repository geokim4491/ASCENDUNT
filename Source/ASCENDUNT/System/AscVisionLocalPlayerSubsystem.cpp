// Fill out your copyright notice in the Description page of Project Settings.

#include "AscVisionLocalPlayerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"

#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/AscVisionMarkerComponent.h"
#include "ASCENDUNT/Character/AscTargetStatusDisplayComponent.h"
#include "ASCENDUNT/Character/BaseCharacter.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Player/AscTargetAwarenessComponent.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

UAscVisionLocalPlayerSubsystem::UAscVisionLocalPlayerSubsystem()
{
	XRayMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Materials/M_XRayMesh_Ally.M_XRayMesh_Ally")));
	XRayHiddenMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Materials/M_XRayHidden.M_XRayHidden")));
}

void UAscVisionLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &ThisClass::HandlePostWorldInit);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &ThisClass::HandleWorldCleanup);

	if (UWorld* World = GetWorld())
	{
		AttachToWorld(World);
	}
}

void UAscVisionLocalPlayerSubsystem::Deinitialize()
{
	if (PostWorldInitHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
		PostWorldInitHandle.Reset();
	}
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}

	DetachFromWorld();
	Super::Deinitialize();
}

void UAscVisionLocalPlayerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	NotifyLocalContextChanged();
}

void UAscVisionLocalPlayerSubsystem::HandlePostWorldInit(
	UWorld* World,
	const UWorld::InitializationValues)
{
	if (World && World == GetWorld())
	{
		AttachToWorld(World);
	}
}

void UAscVisionLocalPlayerSubsystem::AttachToWorld(UWorld* World)
{
	if (!World || BoundWorld.Get() == World)
	{
		return;
	}

	DetachFromWorld();
	BoundWorld = World;

	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));

	RefreshLocalContext();

	for (TActorIterator<ABaseCharacter> It(World); It; ++It)
	{
		RegisterCharacter(*It);
	}

	// Match the controller's menu-map rule, including PIE's prefixed map names.
	if (World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer &&
		!World->GetMapName().Contains(TEXT("L_MainMenu")))
	{
		LoadXRayMaterials();
	}
}

void UAscVisionLocalPlayerSubsystem::HandleWorldCleanup(UWorld* World, bool, bool)
{
	if (World && World == BoundWorld.Get())
	{
		// Release the cache before travel's garbage collection, not after the next world attaches.
		DetachFromWorld();
	}
}

void UAscVisionLocalPlayerSubsystem::DetachFromWorld()
{
	// Invalidate callbacks first, including any completion already queued on the game thread.
	const TWeakObjectPtr<UWorld> PreviousWorld = BoundWorld;
	BoundWorld.Reset();
	if (XRayMaterialsLoadHandle.IsValid())
	{
		XRayMaterialsLoadHandle->CancelHandle();
		XRayMaterialsLoadHandle.Reset();
	}
	LoadedXRayMaterial = nullptr;
	LoadedXRayHiddenMaterial = nullptr;

	if (APlayerController* OldPC = CachedPlayerController.Get())
	{
		OldPC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandleLocalPawnChanged);
	}

	if (CachedTargetAwarenessComponent.IsValid())
	{
		CachedTargetAwarenessComponent->OnObservedTargetsChanged.RemoveAll(this);
	}

	if (CachedLocalPlayerState.IsValid())
	{
		CachedLocalPlayerState->OnTeamIdChanged.RemoveAll(this);
	}

	if (UWorld* World = PreviousWorld.Get())
	{
		if (ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		}
	}
	ActorSpawnedHandle.Reset();

	TArray<ABaseCharacter*> CharactersToUnregister;
	CharactersToUnregister.Reserve(TrackedCharacters.Num());
	for (const TWeakObjectPtr<ABaseCharacter>& WeakCharacter : TrackedCharacters)
	{
		if (ABaseCharacter* Character = WeakCharacter.Get())
		{
			CharactersToUnregister.Add(Character);
		}
	}

	for (ABaseCharacter* Character : CharactersToUnregister)
	{
		UnregisterCharacter(Character);
	}

	TrackedCharacters.Reset();
	CachedPlayerController.Reset();
	CachedLocalPlayerState.Reset();
	CachedLocalCharacter.Reset();
	CachedTargetAwarenessComponent.Reset();
}

bool UAscVisionLocalPlayerSubsystem::RefreshLocalContext()
{
	bool bChanged = false;

	APlayerController* NewPC = nullptr;
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		NewPC = LocalPlayer->GetPlayerController(GetWorld());
	}

	if (CachedPlayerController.Get() != NewPC)
	{
		if (APlayerController* OldPC = CachedPlayerController.Get())
		{
			OldPC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandleLocalPawnChanged);
		}

		CachedPlayerController = NewPC;
		if (NewPC)
		{
			NewPC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandleLocalPawnChanged);
		}

		bChanged = true;
	}

	AAscenduntPlayerState* NewPS = NewPC ? NewPC->GetPlayerState<AAscenduntPlayerState>() : nullptr;
	if (CachedLocalPlayerState.Get() != NewPS)
	{
		if (CachedLocalPlayerState.IsValid())
		{
			CachedLocalPlayerState->OnTeamIdChanged.RemoveAll(this);
		}

		CachedLocalPlayerState = NewPS;
		if (CachedLocalPlayerState.IsValid())
		{
			CachedLocalPlayerState->OnTeamIdChanged.AddUObject(this, &ThisClass::HandleLocalTeamChanged);
		}

		bChanged = true;
	}

	UAscTargetAwarenessComponent* NewAwareness = nullptr;
	if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(NewPC))
	{
		NewAwareness = AscPC->GetTargetAwarenessComponent();
	}

	if (CachedTargetAwarenessComponent.Get() != NewAwareness)
	{
		if (CachedTargetAwarenessComponent.IsValid())
		{
			CachedTargetAwarenessComponent->OnObservedTargetsChanged.RemoveAll(this);
		}

		CachedTargetAwarenessComponent = NewAwareness;
		if (CachedTargetAwarenessComponent.IsValid())
		{
			CachedTargetAwarenessComponent->OnObservedTargetsChanged.AddUObject(
				this, &ThisClass::HandleObservedTargetsChanged);
		}

		bChanged = true;
	}

	ABaseCharacter* NewLocalCharacter = NewPC ? Cast<ABaseCharacter>(NewPC->GetPawn()) : nullptr;
	if (CachedLocalCharacter.Get() != NewLocalCharacter)
	{
		CachedLocalCharacter = NewLocalCharacter;
		bChanged = true;
	}

	return bChanged;
}

void UAscVisionLocalPlayerSubsystem::RegisterCharacter(ABaseCharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	const TWeakObjectPtr<ABaseCharacter> WeakCharacter(Character);
	if (TrackedCharacters.Contains(WeakCharacter))
	{
		return;
	}

	TrackedCharacters.Add(WeakCharacter);
	Character->OnDestroyed.AddDynamic(this, &ThisClass::HandleTrackedActorDestroyed);

	if (UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent())
	{
		if (Character->SupportsXRay())
		{
			HealthComp->OnHealthChanged.AddDynamic(this, &ThisClass::HandleTrackedHealthChanged);
		}

		HealthComp->OnDeathStarted.AddDynamic(this, &ThisClass::HandleTrackedLifeStateChanged);
		HealthComp->OnDeathFinished.AddDynamic(this, &ThisClass::HandleTrackedLifeStateChanged);
	}

	if (UAscPvEDownedComponent* DownedComp = Character->FindComponentByClass<UAscPvEDownedComponent>())
	{
		DownedComp->OnDownedStateChanged.AddUObject(this, &ThisClass::HandleTrackedDownedStateChanged);
	}

	if (Character->SupportsXRay())
	{
		if (UWeaponEquipmentComponent* EquipmentComp = Character->FindComponentByClass<UWeaponEquipmentComponent>())
		{
			EquipmentComp->OnCurrentWeaponChanged.AddUObject(this, &ThisClass::HandleTrackedWeaponChanged);
		}
	}

	if (ABaseAICharacter* AICharacter = Cast<ABaseAICharacter>(Character))
	{
		AICharacter->OnPooledActiveStateChanged.AddUObject(
			this, &ThisClass::HandleTrackedPooledActiveChanged);
	}

	if (Character->HasActorBegunPlay())
	{
		RefreshVisualsForCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::UnregisterCharacter(ABaseCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	Character->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrackedActorDestroyed);

	if (UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent())
	{
		HealthComp->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleTrackedHealthChanged);
		HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleTrackedLifeStateChanged);
		HealthComp->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleTrackedLifeStateChanged);
	}

	if (UAscPvEDownedComponent* DownedComp = Character->FindComponentByClass<UAscPvEDownedComponent>())
	{
		DownedComp->OnDownedStateChanged.RemoveAll(this);
	}

	if (Character->SupportsXRay())
	{
		if (UWeaponEquipmentComponent* EquipmentComp = Character->FindComponentByClass<UWeaponEquipmentComponent>())
		{
			EquipmentComp->OnCurrentWeaponChanged.RemoveAll(this);
		}
	}

	if (ABaseAICharacter* AICharacter = Cast<ABaseAICharacter>(Character))
	{
		AICharacter->OnPooledActiveStateChanged.RemoveAll(this);
	}

	const TWeakObjectPtr<ABaseCharacter> WeakCharacter(Character);
	TrackedCharacters.Remove(WeakCharacter);

	if (!IsValid(Character))
	{
		return;
	}

	Character->SetVisionBlockerEnabled(false);
	Character->SetXRayEnabled(false);
	Character->SetEnemyOverlayEnabled(false);

	if (UAscVisionMarkerComponent* MarkerComp = Character->GetVisionMarkerComponent())
	{
		MarkerComp->SetCriticalVisible(false);
	}

	if (UAscTargetStatusDisplayComponent* DisplayComp = Character->GetTargetStatusDisplayComponent())
	{
		DisplayComp->SetTargetHealthBarVisible(false, EAscTargetHealthBarRelation::Enemy);
	}
}

void UAscVisionLocalPlayerSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(SpawnedActor))
	{
		RegisterCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedActorDestroyed(AActor* DestroyedActor)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(DestroyedActor))
	{
		UnregisterCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::RefreshAllVisuals()
{
	for (auto It = TrackedCharacters.CreateIterator(); It; ++It)
	{
		ABaseCharacter* Character = It->Get();
		if (!IsValid(Character))
		{
			It.RemoveCurrent();
			continue;
		}

		RefreshVisualsForCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::RefreshVisualsForCharacter(ABaseCharacter* Character)
{
	if (!IsValid(Character) || !Character->HasActorBegunPlay() ||
		Character->GetWorld() != BoundWorld.Get())
	{
		return;
	}

	UAscVisionMarkerComponent* MarkerComp = Character->GetVisionMarkerComponent();
	UAscTargetStatusDisplayComponent* DisplayComp = Character->GetTargetStatusDisplayComponent();

	auto ApplyDisabled = [&]()
	{
		Character->SetVisionBlockerEnabled(false);
		Character->SetXRayEnabled(false);
		Character->SetEnemyOverlayEnabled(false);

		if (MarkerComp)
		{
			MarkerComp->SetCriticalVisible(false);
		}
		if (DisplayComp)
		{
			DisplayComp->SetTargetHealthBarVisible(false, EAscTargetHealthBarRelation::Enemy);
		}
	};

	if (Character->IsHidden() || !CachedLocalCharacter.IsValid())
	{
		ApplyDisabled();
		return;
	}

	const UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent();
	const bool bDeadOrDying = HealthComp && HealthComp->IsDeadOrDying();
	const UAscPvEDownedComponent* DownedComp = Character->FindComponentByClass<UAscPvEDownedComponent>();
	const bool bIsDowned = DownedComp && DownedComp->IsDowned();

	if (bDeadOrDying && !bIsDowned)
	{
		ApplyDisabled();
		return;
	}

	switch (GetRelationToLocal(Character))
	{
	case ELocalVisionRelation::Local:
		Character->SetXRayEnabled(false);
		Character->SetVisionBlockerEnabled(true);
		Character->SetEnemyOverlayEnabled(false);
		if (MarkerComp)
		{
			MarkerComp->SetCriticalVisible(false);
		}
		if (DisplayComp)
		{
			DisplayComp->SetTargetHealthBarVisible(false, EAscTargetHealthBarRelation::Ally);
		}
		break;

	case ELocalVisionRelation::Ally:
	{
		float HealthNorm = 1.f;
		bool bShowCritical = false;

		if (HealthComp)
		{
			FAscenduntHealthSnapshot Snapshot;
			if (HealthComp->GetHealthSnapshot(Snapshot))
			{
				const float Capacity = Snapshot.GetDisplayCapacityTotal();
				if (Capacity > KINDA_SMALL_NUMBER)
				{
					HealthNorm = FMath::Clamp(Snapshot.GetCurrentTotal() / Capacity, 0.f, 1.f);
					bShowCritical = Character->SupportsXRay() && HealthNorm < CriticalHealthThreshold;
				}
			}
		}

		Character->SetVisionBlockerEnabled(true);
		Character->SetEnemyOverlayEnabled(false);
		PrepareXRayMaterials(Character);
		Character->SetXRayEnabled(true, HealthNorm);

		if (MarkerComp)
		{
			MarkerComp->SetCriticalVisibleForViewer(bShowCritical, CachedPlayerController.Get());
		}
		if (DisplayComp)
		{
			DisplayComp->SetTargetHealthBarVisible(true, EAscTargetHealthBarRelation::Ally);
		}
		break;
	}

	case ELocalVisionRelation::Enemy:
		Character->SetVisionBlockerEnabled(false);
		Character->SetXRayEnabled(false);
		Character->SetEnemyOverlayEnabled(true);
		if (MarkerComp)
		{
			MarkerComp->SetCriticalVisible(false);
		}
		if (DisplayComp)
		{
			const bool bRevealHealthBar = CachedTargetAwarenessComponent.IsValid() &&
				CachedTargetAwarenessComponent->IsTargetRevealed(Character);
			DisplayComp->SetTargetHealthBarVisible(bRevealHealthBar, EAscTargetHealthBarRelation::Enemy);
		}
		break;

	default:
		ApplyDisabled();
		break;
	}
}

void UAscVisionLocalPlayerSubsystem::PrepareXRayMaterials(ABaseCharacter* Character)
{
	if (!Character->SupportsXRay())
	{
		return;
	}

	// This path only applies the world cache; character events never start asset loads.
	Character->SetXRayMaterials(LoadedXRayMaterial, LoadedXRayHiddenMaterial);

	TArray<UMeshComponent*> Meshes;
	Character->GatherXRayMeshes(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		AAscenduntWeapon* Weapon = IsValid(Mesh) ? Cast<AAscenduntWeapon>(Mesh->GetOwner()) : nullptr;
		if (IsValid(Weapon) && Weapon->HasActorBegunPlay())
		{
			Weapon->SetXRayMaterial(LoadedXRayMaterial);
		}
	}
}

void UAscVisionLocalPlayerSubsystem::LoadXRayMaterials()
{
	if (XRayMaterialsLoadHandle.IsValid() ||
		(IsValid(LoadedXRayMaterial) && IsValid(LoadedXRayHiddenMaterial)))
	{
		return;
	}

	const TArray<FSoftObjectPath> Paths = {
		XRayMaterial.ToSoftObjectPath(), XRayHiddenMaterial.ToSoftObjectPath()
	};
	XRayMaterialsLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::HandleXRayMaterialsLoaded, BoundWorld));
	if (!XRayMaterialsLoadHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vision] Could not request shared X-Ray materials for %s"),
			*GetNameSafe(BoundWorld.Get()));
	}
}

void UAscVisionLocalPlayerSubsystem::HandleXRayMaterialsLoaded(TWeakObjectPtr<UWorld> LoadingWorld)
{
	if (!LoadingWorld.IsValid() || LoadingWorld != BoundWorld)
	{
		return;
	}

	// Hold strong references before releasing the streamable handle.
	LoadedXRayMaterial = XRayMaterial.Get();
	LoadedXRayHiddenMaterial = XRayHiddenMaterial.Get();
	XRayMaterialsLoadHandle.Reset();
	if (!IsValid(LoadedXRayMaterial) || !IsValid(LoadedXRayHiddenMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vision] Failed to load shared X-Ray materials for %s"),
			*GetNameSafe(BoundWorld.Get()));
	}

	// A single refresh after preload; future BeginPlay/weapon/context events use this cache.
	RefreshAllVisuals();
}

UAscVisionLocalPlayerSubsystem::ELocalVisionRelation
UAscVisionLocalPlayerSubsystem::GetRelationToLocal(const ABaseCharacter* Target) const
{
	const ABaseCharacter* LocalCharacter = CachedLocalCharacter.Get();
	if (!IsValid(LocalCharacter) || !IsValid(Target))
	{
		return ELocalVisionRelation::Invalid;
	}

	if (LocalCharacter == Target)
	{
		return ELocalVisionRelation::Local;
	}

	if (!IsValid(Target->GetPlayerState()))
	{
		return ELocalVisionRelation::Enemy;
	}

	return LocalCharacter->GetGenericTeamId() == Target->GetGenericTeamId()
		? ELocalVisionRelation::Ally
		: ELocalVisionRelation::Enemy;
}

void UAscVisionLocalPlayerSubsystem::NotifyLocalContextChanged()
{
	if (RefreshLocalContext())
	{
		RefreshAllVisuals();
	}
}

void UAscVisionLocalPlayerSubsystem::HandleLocalPawnChanged(APawn*, APawn*)
{
	NotifyLocalContextChanged();
}

void UAscVisionLocalPlayerSubsystem::NotifyCharacterBeganPlay(ABaseCharacter* Character)
{
	if (!IsValid(Character) || Character->GetWorld() != BoundWorld.Get())
	{
		return;
	}

	const TWeakObjectPtr<ABaseCharacter> WeakCharacter(Character);
	if (!TrackedCharacters.Contains(WeakCharacter))
	{
		RegisterCharacter(Character);
		// Registration already refreshes characters that have completed BeginPlay.
		if (Character->HasActorBegunPlay())
		{
			return;
		}
	}

	RefreshVisualsForCharacter(Character);
}

void UAscVisionLocalPlayerSubsystem::NotifyCharacterPlayerStateChanged(ABaseCharacter* Character)
{
	if (IsValid(Character) &&
		TrackedCharacters.Contains(TWeakObjectPtr<ABaseCharacter>(Character)))
	{
		RefreshVisualsForCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::NotifyWeaponBeganPlay(AAscenduntWeapon* Weapon)
{
	if (!IsValid(Weapon) || Weapon->GetWorld() != BoundWorld.Get())
	{
		return;
	}

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(Weapon->GetOwner()))
	{
		if (TrackedCharacters.Contains(TWeakObjectPtr<ABaseCharacter>(Character)))
		{
			RefreshVisualsForCharacter(Character);
		}
	}
}

void UAscVisionLocalPlayerSubsystem::HandleLocalTeamChanged(uint8, uint8)
{
	RefreshAllVisuals();
}

void UAscVisionLocalPlayerSubsystem::HandleObservedTargetsChanged()
{
	for (const TWeakObjectPtr<ABaseCharacter>& WeakCharacter : TrackedCharacters)
	{
		ABaseCharacter* Character = WeakCharacter.Get();
		if (!IsValid(Character) || GetRelationToLocal(Character) != ELocalVisionRelation::Enemy)
		{
			continue;
		}

		if (UAscTargetStatusDisplayComponent* DisplayComp = Character->GetTargetStatusDisplayComponent())
		{
			const UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent();
			const bool bDeadOrDying = HealthComp && HealthComp->IsDeadOrDying();
			const UAscPvEDownedComponent* DownedComp = Character->FindComponentByClass<UAscPvEDownedComponent>();
			const bool bIsDowned = DownedComp && DownedComp->IsDowned();

			const bool bRevealHealthBar = !Character->IsHidden() &&
				(!bDeadOrDying || bIsDowned) &&
				CachedTargetAwarenessComponent.IsValid() &&
				CachedTargetAwarenessComponent->IsTargetRevealed(Character);
			DisplayComp->SetTargetHealthBarVisible(bRevealHealthBar, EAscTargetHealthBarRelation::Enemy);
		}
	}
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedWeaponChanged(
	AAscenduntWeapon* NewWeapon,
	AAscenduntWeapon* PreviousWeapon)
{
	AActor* WeaponOwner = IsValid(NewWeapon)
		? NewWeapon->GetOwner()
		: (IsValid(PreviousWeapon) ? PreviousWeapon->GetOwner() : nullptr);

	if (ABaseCharacter* Character = Cast<ABaseCharacter>(WeaponOwner))
	{
		RefreshVisualsForCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedDownedStateChanged(bool)
{
	RefreshAllVisuals();
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedPooledActiveChanged(
	ABaseAICharacter* Character,
	bool)
{
	RefreshVisualsForCharacter(Character);
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedHealthChanged(
	UAscenduntHealthComponent* HealthComponent,
	AActor* OwningActor,
	AActor*,
	bool,
	float,
	const FAscenduntHealthSnapshot&)
{
	if (!IsValid(HealthComponent)) return;

	ABaseCharacter* Character =
		Cast<ABaseCharacter>(HealthComponent->GetOwner());

	if (!IsValid(Character)) return;

	RefreshVisualsForCharacter(Character);
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedLifeStateChanged(AActor* OwningActor)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(OwningActor))
	{
		RefreshVisualsForCharacter(Character);
	}
}
