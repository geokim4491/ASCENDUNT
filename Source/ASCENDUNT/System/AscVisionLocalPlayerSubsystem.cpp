// Fill out your copyright notice in the Description page of Project Settings.

#include "AscVisionLocalPlayerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"

#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/AscVisionMarkerComponent.h"
#include "ASCENDUNT/Character/AscTargetStatusDisplayComponent.h"
#include "ASCENDUNT/Character/BaseCharacter.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscTargetAwarenessComponent.h"


void UAscVisionLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &ThisClass::HandlePostWorldInit);

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

	DetachFromWorld();
	Super::Deinitialize();
}

void UAscVisionLocalPlayerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	RefreshLocalContext();
	RefreshAllVisuals();
}

void UAscVisionLocalPlayerSubsystem::HandlePostWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (!World || World != GetWorld())
	{
		return;
	}

	AttachToWorld(World);
}

void UAscVisionLocalPlayerSubsystem::AttachToWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (BoundWorld.Get() == World)
	{
		return;
	}

	DetachFromWorld();
	BoundWorld = World;

	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));

	World->GetTimerManager().SetTimer(
		RefreshTimerHandle,
		this,
		&ThisClass::RefreshTrackedCharacters,
		RefreshInterval,
		true);

	RefreshLocalContext();
	RefreshTrackedCharacters();
	RefreshAllVisuals();
}

void UAscVisionLocalPlayerSubsystem::DetachFromWorld()
{
	if (CachedTargetAwarenessComponent.IsValid())
	{
		CachedTargetAwarenessComponent->OnObservedTargetsChanged.RemoveAll(this);
		CachedTargetAwarenessComponent.Reset();
	}

	if (CachedLocalPlayerState.IsValid())
	{
		CachedLocalPlayerState->OnTeamIdChanged.RemoveAll(this);
		CachedLocalPlayerState.Reset();
	}

	if (UWorld* World = BoundWorld.Get())
	{
		if (ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
			ActorSpawnedHandle.Reset();
		}

		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	for (const TWeakObjectPtr<ABaseCharacter>& WeakCharacter : TrackedCharacters)
	{
		if (ABaseCharacter* Character = WeakCharacter.Get())
		{
			ApplyDisabledState(Character, false);

			if (UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent())
			{
				HealthComp->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleTrackedHealthChanged);
				HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleTrackedDeathStarted);
				HealthComp->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleTrackedDeathFinished);
			}
		}
	}

	TrackedCharacters.Reset();
	BoundWorld.Reset();
	CachedPlayerController.Reset();
}

void UAscVisionLocalPlayerSubsystem::RefreshLocalContext()
{
	APlayerController* NewPC = nullptr;
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		NewPC = LP->GetPlayerController(GetWorld());
	}

	if (CachedPlayerController.Get() != NewPC)
	{
		if (CachedTargetAwarenessComponent.IsValid())
		{
			CachedTargetAwarenessComponent->OnObservedTargetsChanged.RemoveAll(this);
			CachedTargetAwarenessComponent.Reset();
		}

		if (CachedLocalPlayerState.IsValid())
		{
			CachedLocalPlayerState->OnTeamIdChanged.RemoveAll(this);
			CachedLocalPlayerState.Reset();
		}

		CachedPlayerController = NewPC;

		if (AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(NewPC))
		{
			CachedTargetAwarenessComponent = AscPC->GetTargetAwarenessComponent();
			if (CachedTargetAwarenessComponent.IsValid())
			{
				CachedTargetAwarenessComponent->OnObservedTargetsChanged.AddUObject(
					this, &ThisClass::HandleObservedTargetsChanged);
			}
		}
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
	}
}

void UAscVisionLocalPlayerSubsystem::RefreshTrackedCharacters()
{
	UWorld* World = BoundWorld.Get();
	if (!World)
	{
		return;
	}

	RefreshLocalContext();

	TSet<TWeakObjectPtr<ABaseCharacter>> SeenThisFrame;

	for (TActorIterator<ABaseCharacter> It(World); It; ++It)
	{
		ABaseCharacter* Character = *It;
		if (!IsValid(Character))
		{
			continue;
		}

		RegisterCharacter(Character);
		SeenThisFrame.Add(Character);
	}

	for (auto It = TrackedCharacters.CreateIterator(); It; ++It)
	{
		if (!It->IsValid() || !SeenThisFrame.Contains(*It))
		{
			if (ABaseCharacter* Character = It->Get())
			{
				UnregisterCharacter(Character);
			}
			It.RemoveCurrent();
		}
	}

	RefreshAllVisuals();
}

void UAscVisionLocalPlayerSubsystem::RefreshAllVisuals()
{
	for (auto It = TrackedCharacters.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
			continue;
		}

		RefreshVisualsForCharacter(It->Get());
	}
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

	if (UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent())
	{
		HealthComp->OnHealthChanged.AddDynamic(this, &ThisClass::HandleTrackedHealthChanged);
		HealthComp->OnDeathStarted.AddDynamic(this, &ThisClass::HandleTrackedDeathStarted);
		HealthComp->OnDeathFinished.AddDynamic(this, &ThisClass::HandleTrackedDeathFinished);
	}

	RefreshVisualsForCharacter(Character);
}

void UAscVisionLocalPlayerSubsystem::UnregisterCharacter(ABaseCharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	if (UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent())
	{
		HealthComp->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleTrackedHealthChanged);
		HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleTrackedDeathStarted);
		HealthComp->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleTrackedDeathFinished);
	}

	ApplyDisabledState(Character, false);
}

void UAscVisionLocalPlayerSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(SpawnedActor))
	{
		RegisterCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::RefreshVisualsForCharacter(ABaseCharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	// 렌더링되지 않는 캐릭터에 X-Ray나 적 오버레이를 적용할 이유가 없다.
	// 오브젝트 풀에 반납된 AI가 여기 해당한다. 가드가 없으면 0.15초마다
	// 숨겨진 액터의 머티리얼을 갱신하고 MarkRenderStateDirty를 호출하게 된다.
	if (Character->IsHidden())
	{
		UpdateTargetHealthBar(Character, false, false);
		return;
	}

	ABaseCharacter* LocalCharacter = GetLocalBaseCharacter();
	if (!LocalCharacter)
	{
		ApplyDisabledState(Character, false);
		return;
	}

	if (Character == LocalCharacter)
	{
		ApplyLocalBlockerState(Character);
		return;
	}

	UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent();
	const bool bDeadOrDying = HealthComp ? HealthComp->IsDeadOrDying() : false;

	// PvE heroes intentionally remain in DeathStarted while they are Downed.
	// Do not treat that intermediate life state as full death for local vision/UI.
	const UAscPvEDownedComponent* DownedComp = Character->FindComponentByClass<UAscPvEDownedComponent>();
	const bool bIsDowned = DownedComp && DownedComp->IsDowned();

	if (bDeadOrDying && !bIsDowned)
	{
		ApplyDisabledState(Character, false);
		return;
	}

	if (IsFriendlyToLocal(Character))
	{
		const float HealthNorm = GetTargetHealthNormalized(Character);
		const bool bShowCritical = (HealthNorm < CriticalThreshold);

		if (Character->SupportsXRay())
		{
			ApplyAllyXRayState(Character, HealthNorm, bShowCritical);
		}
		else
		{
			// AI 등 X-Ray 비지원 캐릭터는 원본 메시의 렌더 상태를 건드리지 않는다.
			UpdateCriticalWarningScale(Character, false);
		}

		UpdateTargetHealthBar(Character, true, true);
		return;
	}

	if (IsEnemyToLocal(Character))
	{
		if (Character->SupportsEnemyOverlay())
		{
			ApplyEnemyOverlayState(Character);
		}
		else
		{
			// PvE AI는 Enemy Overlay/X-Ray 렌더 상태를 전혀 사용하지 않는다.
			// 체력바/타깃 UI만 기존대로 유지한다.
			UpdateCriticalWarningScale(Character, false);
		}

		UpdateTargetHealthBar(Character, IsEnemyHealthBarRevealed(Character), false);
		return;
	}

	ApplyDisabledState(Character, false);
}

void UAscVisionLocalPlayerSubsystem::ApplyDisabledState(ABaseCharacter* Character, bool bShowCriticalWidget)
{
	if (!IsValid(Character))
	{
		return;
	}

	SetCharacterBlockerCustomDepth(Character, false);
	SetCharacterXRayEnabled(Character, false);

	Character->SetEnemyOverlayEnabled(false);

	UpdateCriticalWarningScale(Character, bShowCriticalWidget);
	UpdateTargetHealthBar(Character, false, false);
}

void UAscVisionLocalPlayerSubsystem::ApplyLocalBlockerState(ABaseCharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	SetCharacterXRayEnabled(Character, false);
	SetCharacterBlockerCustomDepth(Character, true);
	Character->SetEnemyOverlayEnabled(false);

	if (UAscVisionMarkerComponent* MarkerComp = Character->GetVisionMarkerComponent())
	{
		MarkerComp->SetCriticalVisible(false);
	}

	UpdateTargetHealthBar(Character, false, false);
}

void UAscVisionLocalPlayerSubsystem::ApplyAllyXRayState(ABaseCharacter* Character, float HealthNorm, bool bShowCriticalWidget)
{
	if (!IsValid(Character))
	{
		return;
	}

	// Simulated Proxy의 원본 Body / Weapon도 CustomDepth에 기록한다.
	// 목적:
	// - 원본 메시가 XRayMesh를 가리는 경우 X-Ray가 위에 덮이지 않도록 차단
	// - 벽 뒤에 있을 때는 SceneDepth와 CustomDepth가 다르므로 Blocker로 인정되지 않음
	SetCharacterBlockerCustomDepth(Character, true);
	Character->SetEnemyOverlayEnabled(false);

	SetCharacterXRayEnabled(Character, true, HealthNorm, 1.f);

	UpdateCriticalWarningScale(Character, bShowCriticalWidget);
}

void UAscVisionLocalPlayerSubsystem::ApplyEnemyOverlayState(ABaseCharacter* Character)
{
	if (!IsValid(Character))
	{
		return;
	}

	SetCharacterBlockerCustomDepth(Character, false);
	SetCharacterXRayEnabled(Character, false);

	Character->SetEnemyOverlayEnabled(true);

	UpdateCriticalWarningScale(Character, false);
}

void UAscVisionLocalPlayerSubsystem::SetMeshBlockerCustomDepth(UMeshComponent* Mesh, bool bEnableCustomDepth) const
{
	if (!IsValid(Mesh))
	{
		return;
	}

	// Mesh-based X-Ray에서는 CustomDepth를 로컬 Blocker depth buffer로만 사용한다.
	Mesh->SetRenderCustomDepth(bEnableCustomDepth);
	Mesh->MarkRenderStateDirty();
}

void UAscVisionLocalPlayerSubsystem::SetCharacterBlockerCustomDepth(ABaseCharacter* Character, bool bEnableCustomDepth) const
{
	if (!IsValid(Character) || !Character->SupportsXRay())
	{
		return;
	}

	TArray<UMeshComponent*> VisionMeshes;
	Character->GatherVisionMeshes(VisionMeshes);

	for (UMeshComponent* Mesh : VisionMeshes)
	{
		SetMeshBlockerCustomDepth(Mesh, bEnableCustomDepth);
	}
}

void UAscVisionLocalPlayerSubsystem::SetMeshXRayEnabled(UMeshComponent* Mesh, bool bEnabled, float HealthNorm, float XRayAlpha) const
{
	if (!IsValid(Mesh))
	{
		return;
	}

	// 머티리얼은 BaseCharacter / Weapon 쪽에서 초기화한다.
	// Subsystem은 X-Ray 표시 상태와 per-mesh 데이터만 갱신해야 한다.
	Mesh->SetVisibility(bEnabled, true);
	Mesh->SetHiddenInGame(!bEnabled, true);
	Mesh->SetRenderCustomDepth(false);

	Mesh->SetCustomPrimitiveDataFloat(XRayHealthPrimitiveDataIndex, FMath::Clamp(HealthNorm, 0.f, 1.f));
	Mesh->SetCustomPrimitiveDataFloat(XRayAlphaPrimitiveDataIndex, FMath::Clamp(XRayAlpha, 0.f, 1.f));
	Mesh->MarkRenderStateDirty();
}

void UAscVisionLocalPlayerSubsystem::SetCharacterXRayEnabled(ABaseCharacter* Character, bool bEnabled, float HealthNorm, float XRayAlpha) const
{
	if (!IsValid(Character) || !Character->SupportsXRay())
	{
		return;
	}

	TArray<UMeshComponent*> XRayMeshes;
	Character->GatherXRayMeshes(XRayMeshes);

	for (UMeshComponent* Mesh : XRayMeshes)
	{
		SetMeshXRayEnabled(Mesh, bEnabled, HealthNorm, XRayAlpha);
	}
}

float UAscVisionLocalPlayerSubsystem::ComputeCriticalWarningScale(const ABaseCharacter* Target) const
{
	if (!IsValid(Target))
	{
		return CriticalScaleMax;
	}

	APlayerController* PC = CachedPlayerController.Get();
	if (!PC)
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			PC = LP->GetPlayerController(GetWorld());
		}
	}

	if (!IsValid(PC))
	{
		return CriticalScaleMax;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const float Distance = FVector::Dist(ViewLocation, Target->GetActorLocation());

	const float Alpha = FMath::GetRangePct(
		CriticalScaleNearDistance,
		CriticalScaleFarDistance,
		Distance);

	const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);

	return FMath::Lerp(CriticalScaleMax, CriticalScaleMin, ClampedAlpha);
}

void UAscVisionLocalPlayerSubsystem::UpdateCriticalWarningScale(ABaseCharacter* Character, bool bShowCriticalWidget) const
{
	if (!IsValid(Character))
	{
		return;
	}

	if (UAscVisionMarkerComponent* MarkerComp = Character->GetVisionMarkerComponent())
	{
		MarkerComp->SetCriticalVisible(bShowCriticalWidget);
	}

	if (!bShowCriticalWidget)
	{
		Character->SetCriticalWarningScale(CriticalScaleMax);
		return;
	}

	const float Scale = ComputeCriticalWarningScale(Character);
	Character->SetCriticalWarningScale(Scale);
}

ABaseCharacter* UAscVisionLocalPlayerSubsystem::GetLocalBaseCharacter() const
{
	APlayerController* PC = CachedPlayerController.Get();
	if (!PC)
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			PC = LP->GetPlayerController(GetWorld());
		}
	}

	return PC ? Cast<ABaseCharacter>(PC->GetPawn()) : nullptr;
}

bool UAscVisionLocalPlayerSubsystem::IsFriendlyToLocal(const ABaseCharacter* Target) const
{
	const ABaseCharacter* LocalCharacter = GetLocalBaseCharacter();
	if (!LocalCharacter || !Target || LocalCharacter == Target)
	{
		return false;
	}

	if (!IsPlayerCharacter(Target) && bTreatNonPlayerCharactersAsHostile)
	{
		return false;
	}

	const uint8 LocalTeamId = LocalCharacter->GetGenericTeamId().GetId();
	const uint8 TargetTeamId = Target->GetGenericTeamId().GetId();

	return (LocalTeamId == TargetTeamId);
}

bool UAscVisionLocalPlayerSubsystem::IsEnemyToLocal(const ABaseCharacter* Target) const
{
	const ABaseCharacter* LocalCharacter = GetLocalBaseCharacter();
	if (!LocalCharacter || !Target || LocalCharacter == Target)
	{
		return false;
	}

	if (!IsPlayerCharacter(Target) && bTreatNonPlayerCharactersAsHostile)
	{
		return true;
	}

	return !IsFriendlyToLocal(Target);
}

bool UAscVisionLocalPlayerSubsystem::IsPlayerCharacter(const ABaseCharacter* Target) const
{
	return Target && IsValid(Target->GetPlayerState());
}

float UAscVisionLocalPlayerSubsystem::GetTargetHealthNormalized(const ABaseCharacter* Character) const
{
	if (!Character)
	{
		return 1.f;
	}

	if (const UAscenduntHealthComponent* HealthComp = Character->GetAscenduntHealthComponent())
	{
		// Replication can expose the HealthSet object before its max-health attributes arrive.
		// A missing set or a zero display capacity means "unknown yet", not 0 HP.
		FAscenduntHealthSnapshot Snapshot;
		if (!HealthComp->GetHealthSnapshot(Snapshot))
		{
			return 1.f;
		}

		const float Capacity = Snapshot.GetDisplayCapacityTotal();
		if (Capacity <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}

		return FMath::Clamp(Snapshot.GetCurrentTotal() / Capacity, 0.f, 1.f);
	}

	return 1.f;
}

bool UAscVisionLocalPlayerSubsystem::IsEnemyHealthBarRevealed(const ABaseCharacter* Target) const
{
	return CachedTargetAwarenessComponent.IsValid() &&
		CachedTargetAwarenessComponent->IsTargetRevealed(Target);
}

void UAscVisionLocalPlayerSubsystem::UpdateTargetHealthBar(
	ABaseCharacter* Character, bool bShouldShow, bool bFriendly) const
{
	if (!IsValid(Character))
	{
		return;
	}

	UAscTargetStatusDisplayComponent* DisplayComponent =
		UAscTargetStatusDisplayComponent::FindTargetStatusDisplayComponent(Character);
	if (!DisplayComponent)
	{
		return;
	}

	if (!bShouldShow)
	{
		DisplayComponent->ClearTargetHealthBar();
		return;
	}

	const UAscenduntHealthComponent* HealthComponent = Character->GetAscenduntHealthComponent();
	FAscenduntHealthSnapshot Snapshot;
	if (!HealthComponent || !HealthComponent->GetHealthSnapshot(Snapshot) ||
		Snapshot.GetDisplayCapacityTotal() <= KINDA_SMALL_NUMBER)
	{
		// Do not render an uninitialized replicated health state as a real 0 / 0 bar.
		DisplayComponent->ClearTargetHealthBar();
		return;
	}

	DisplayComponent->SetTargetHealthBar(
		Snapshot,
		bFriendly ? EAscTargetHealthBarRelation::Ally : EAscTargetHealthBarRelation::Enemy);
}

void UAscVisionLocalPlayerSubsystem::HandleLocalTeamChanged(uint8 OldTeamId, uint8 NewTeamId)
{
	RefreshAllVisuals();
}

void UAscVisionLocalPlayerSubsystem::HandleObservedTargetsChanged()
{
	RefreshAllVisuals();
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedHealthChanged(
	UAscenduntHealthComponent* HealthComponent,
	AActor* OwningActor,
	AActor* Instigator,
	bool bWasDamaging,
	float RawValue,
	const FAscenduntHealthSnapshot& HealthSnapshot)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(OwningActor))
	{
		RefreshVisualsForCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedDeathStarted(AActor* OwningActor)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(OwningActor))
	{
		RefreshVisualsForCharacter(Character);
	}
}

void UAscVisionLocalPlayerSubsystem::HandleTrackedDeathFinished(AActor* OwningActor)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(OwningActor))
	{
		RefreshVisualsForCharacter(Character);
	}
}
