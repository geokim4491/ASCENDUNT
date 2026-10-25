// Fill out your copyright notice in the Description page of Project Settings.


#include "HeroCharacter.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntCharacterStatSet.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/System/AscVisionLocalPlayerSubsystem.h"
#include "ASCENDUNT/Input/AscenduntEnhancedInputComponent.h"
#include "ASCENDUNT/Camera/AscenduntCameraComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCameraComponent.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "ASCENDUNT/Character/Hero/HeroInitComponent.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedConfig.h"
#include "ASCENDUNT/Character/HitCollision/AscHitCollisionComponent.h"
#include "ASCENDUNT/Interaction/AscInteractionComponent.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameMode.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameState.h"
#include "ASCENDUNT/GameModes/PVP/MatchGameMode.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "HeroInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"


AHeroCharacter::AHeroCharacter(const FObjectInitializer& ObjectInitializer)
{
	// Avoid ticking characters if possible.
	// PrimaryActorTick.bCanEverTick = false;
	// PrimaryActorTick.bStartWithTickEnabled = false;

	USkeletalMeshComponent* MeshComp = GetMesh();
	check(MeshComp);
	MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// 3P Mesh pose is gameplay data for Hero hit detection.
	// Hero hit-collision components may be attached directly to bones, so the dedicated
	// server must evaluate the AnimBP and refresh bones even though nothing is rendered.
	// This also preserves the existing first-person self-shadow requirement on clients.
	MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	HeroInputComponent = CreateDefaultSubobject<UHeroInputComponent>(TEXT("HeroInputComponent"));
	HeroCameraComponent = CreateDefaultSubobject<UHeroCameraComponent>(TEXT("HeroCameraComponent"));
	CameraComponent = CreateDefaultSubobject<UAscenduntCameraComponent>(TEXT("CameraComponent"));

	// Mesh1P 는 카메라에 부착되는 1인칭 표현이다.
	//
	// 카메라 부착이 핵심이다. 메시가 카메라의 자식이라 무기와 손이 화면 좌표계에 고정되고,
	// 오버워치식 "총이 화면 우하단에 붙어 있는" 프레이밍이 워프 릭 없이 그냥 나온다.
	//
	// Z 오프셋은 스켈레탈 메시의 원점(발)을 카메라 아래로 내려서 머리가 카메라 높이에
	// 오도록 맞추는 값이다. 캐릭터 비율에 따라 다르므로 히어로 BP 에서 조정한다.
	//
	// 카메라 부착이라 피치를 상속하지만, 하체는 UHeroFirstPersonComponent 가 본 단위로
	// 숨기므로 위를 올려다볼 때 다리가 도는 문제는 발생하지 않는다.
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(CameraComponent);
	Mesh1P->SetRelativeLocation(FVector(0.f, 0.f, -150.f));
	Mesh1P->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Mesh1P->SetOnlyOwnerSee(true);

	// 기본 off. UHeroFirstPersonComponent 만 켠다.
	Mesh1P->SetVisibility(false);

	Mesh1P->bReceivesDecals = false;
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetGenerateOverlapEvents(false);
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// CastShadow 는 일부러 건드리지 않는다. FirstPerson 프리미티브는 씬 프록시에서
	// 그림자 플래그가 전부 강제로 꺼진다. 자기 그림자는 3P 메시가 담당한다.
	Mesh1P->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	// 1인칭 렌더 상태 조정자. Mesh1P 를 만든 뒤에 붙인다.
	FirstPersonComponent = CreateDefaultSubobject<UHeroFirstPersonComponent>(TEXT("FirstPersonComponent"));
	HeroInitComponent = CreateDefaultSubobject<UHeroInitComponent>(TEXT("HeroInitComponent"));
	PvEDownedComponent = CreateDefaultSubobject<UAscPvEDownedComponent>(TEXT("PvEDownedComponent"));

	// PvE Downed/Revive rules are shared by every hero. Keep the asset path centralized here
	// so individual Hero Blueprints never need to configure their DownedComponent.
	static ConstructorHelpers::FObjectFinder<UAscPvEDownedConfig> PvEDownedConfigFinder(
		TEXT("/Game/GameModes/PvE/Data/DA_PvEDownedConfig.DA_PvEDownedConfig"));
	if (PvEDownedConfigFinder.Succeeded())
	{
		PvEDownedComponent->SetConfig(PvEDownedConfigFinder.Object);
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[HeroCharacter] Missing shared PvE Downed config: /Game/GameModes/PvE/Data/DA_PvEDownedConfig"));
	}

	InteractionComponent = CreateDefaultSubobject<UAscInteractionComponent>(TEXT("InteractionComponent"));
	WeaponEquipmentComponent = CreateDefaultSubobject<UWeaponEquipmentComponent>(TEXT("WeaponEquipmentComponent"));
	AscenduntHealthComponent = CreateDefaultSubobject<UAscenduntHealthComponent>(TEXT("AscenduntHealthComponent"));


	OverrideInputComponentClass = UAscenduntEnhancedInputComponent::StaticClass();
}

UAscenduntAbilitySystemComponent* AHeroCharacter::GetAscenduntAbilitySystemComponent() const
{
	if (!CachedASC)
	{
		if (const AAscenduntPlayerState* PS = GetPlayerState<AAscenduntPlayerState>())
		{
			CachedASC = PS->GetAscenduntAbilitySystemComponent();
		}
	}
	return CachedASC;
}

UAbilitySystemComponent* AHeroCharacter::GetAbilitySystemComponent() const
{
	if (!CachedASC)
	{
		if (const AAscenduntPlayerState* PS = GetPlayerState<AAscenduntPlayerState>())
		{
			CachedASC = PS->GetAscenduntAbilitySystemComponent();
		}
	}
	return CachedASC;
}

AAscenduntPlayerController* AHeroCharacter::GetAscenduntPlayerController() const
{
	return Cast<AAscenduntPlayerController>(GetController());
}

AAscenduntPlayerState* AHeroCharacter::GetAscenduntPlayerState() const
{
	return GetPlayerState<AAscenduntPlayerState>();
}

UHeroInputComponent* AHeroCharacter::GetHeroInputComponent() const
{
	return HeroInputComponent;
}

UHeroCameraComponent* AHeroCharacter::GetHeroCameraComponent() const
{
	return HeroCameraComponent;
}

void AHeroCharacter::Multicast_PlayAbilityPresentationMontage_Implementation(
	UAnimMontage* Montage3P, UAnimMontage* Montage1P, float PlayRate, FName StartSection)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float SafeRate = FMath::Max(0.01f, PlayRate);

	if (Montage3P)
	{
		if (USkeletalMeshComponent* Mesh3P = GetMesh())
		{
			if (UAnimInstance* Anim = Mesh3P->GetAnimInstance())
			{
				const float Duration = Anim->Montage_Play(Montage3P, SafeRate);
				if (Duration > 0.0f && StartSection != NAME_None)
				{
					Anim->Montage_JumpToSection(StartSection, Montage3P);
				}
			}
		}
	}

	if (Montage1P && IsLocallyControlled() && FirstPersonComponent)
	{
		FirstPersonComponent->PlayMontage1P(Montage1P, SafeRate, StartSection);
	}
}

UWeaponEquipmentComponent* AHeroCharacter::GetWeaponEquipmentComponent() const
{
	return WeaponEquipmentComponent;
}

bool AHeroCharacter::IsGameplayReady() const
{
	return HeroInitComponent && HeroInitComponent->IsGameplayReady();
}


bool AHeroCharacter::CanInteract_Implementation(AHeroCharacter* Interactor) const
{
	return PvEDownedComponent && PvEDownedComponent->CanBeRevivedBy(Interactor);
}

FAscInteractionOption AHeroCharacter::GetInteractionOption_Implementation(AHeroCharacter* Interactor) const
{
	return PvEDownedComponent
		? PvEDownedComponent->GetReviveInteractionOption()
		: FAscInteractionOption();
}

void AHeroCharacter::BeginInteraction_Implementation(AHeroCharacter* Interactor)
{
	if (PvEDownedComponent)
	{
		PvEDownedComponent->BeginRevive(Interactor);
	}
}

void AHeroCharacter::CancelInteraction_Implementation(AHeroCharacter* Interactor)
{
	if (PvEDownedComponent)
	{
		PvEDownedComponent->CancelRevive(Interactor);
	}
}

void AHeroCharacter::CompleteInteraction_Implementation(AHeroCharacter* Interactor)
{
	if (PvEDownedComponent)
	{
		PvEDownedComponent->CompleteRevive(Interactor);
	}
}


void AHeroCharacter::SetDownedMovementOverride(bool bEnabled, float CrawlSpeed)
{
	bDownedMovementOverride = bEnabled;
	DownedMovementSpeedOverride = bEnabled ? FMath::Max(0.0f, CrawlSpeed) : 0.0f;
	RefreshMovementFromAttributes();
}

void AHeroCharacter::UninitializeGameplayBindings()
{
	UnbindMovementAttributes();
}

void AHeroCharacter::InitializeGameplayBindings()
{
	UnbindMovementAttributes();

	UAscenduntAbilitySystemComponent* ASC = GetAscenduntAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	MovementAttributeASC = ASC;
	BaseMoveSpeedChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UAscenduntCharacterStatSet::GetBaseMoveSpeedAttribute()).AddUObject(
			this, &ThisClass::HandleBaseMoveSpeedChanged);

	CrouchMoveSpeedScalarChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UAscenduntCharacterStatSet::GetCrouchMoveSpeedScalarAttribute()).AddUObject(
			this, &ThisClass::HandleCrouchMoveSpeedScalarChanged);

	JumpZVelocityChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UAscenduntCharacterStatSet::GetJumpZVelocityAttribute()).AddUObject(
			this, &ThisClass::HandleJumpZVelocityChanged);

	AirControlChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UAscenduntCharacterStatSet::GetAirControlAttribute()).AddUObject(
			this, &ThisClass::HandleAirControlChanged);

	RefreshMovementFromAttributes();
}

void AHeroCharacter::UnbindMovementAttributes()
{
	if (UAscenduntAbilitySystemComponent* ASC = MovementAttributeASC.Get())
	{
		if (BaseMoveSpeedChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UAscenduntCharacterStatSet::GetBaseMoveSpeedAttribute()).Remove(BaseMoveSpeedChangedHandle);
		}

		if (CrouchMoveSpeedScalarChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UAscenduntCharacterStatSet::GetCrouchMoveSpeedScalarAttribute()).Remove(CrouchMoveSpeedScalarChangedHandle);
		}

		if (JumpZVelocityChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UAscenduntCharacterStatSet::GetJumpZVelocityAttribute()).Remove(JumpZVelocityChangedHandle);
		}

		if (AirControlChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UAscenduntCharacterStatSet::GetAirControlAttribute()).Remove(AirControlChangedHandle);
		}
	}

	MovementAttributeASC.Reset();
	BaseMoveSpeedChangedHandle.Reset();
	CrouchMoveSpeedScalarChangedHandle.Reset();
	JumpZVelocityChangedHandle.Reset();
	AirControlChangedHandle.Reset();
}

void AHeroCharacter::RefreshMovementFromAttributes()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// The Downed override does not depend on AttributeSet replication. This matters for
	// the owning client if the Downed component's replication arrives before a movement
	// attribute refresh.
	if (bDownedMovementOverride)
	{
		Movement->MaxWalkSpeed = DownedMovementSpeedOverride;
		Movement->MaxWalkSpeedCrouched = DownedMovementSpeedOverride;
		return;
	}

	UAscenduntAbilitySystemComponent* ASC = MovementAttributeASC.Get();
	if (!ASC)
	{
		return;
	}

	const float BaseMoveSpeed = FMath::Max(0.0f, ASC->GetNumericAttribute(
		UAscenduntCharacterStatSet::GetBaseMoveSpeedAttribute()));
	const float CrouchScalar = FMath::Max(0.0f, ASC->GetNumericAttribute(
		UAscenduntCharacterStatSet::GetCrouchMoveSpeedScalarAttribute()));

	const float JumpZVelocity = FMath::Max(0.0f, ASC->GetNumericAttribute(
		UAscenduntCharacterStatSet::GetJumpZVelocityAttribute()));
	const float AirControl = FMath::Max(0.0f, ASC->GetNumericAttribute(
		UAscenduntCharacterStatSet::GetAirControlAttribute()));

	Movement->MaxWalkSpeed = BaseMoveSpeed;
	Movement->MaxWalkSpeedCrouched = BaseMoveSpeed * CrouchScalar;
	Movement->JumpZVelocity = JumpZVelocity;
	Movement->AirControl = AirControl;
}

void AHeroCharacter::HandleBaseMoveSpeedChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshMovementFromAttributes();
}

void AHeroCharacter::HandleCrouchMoveSpeedScalarChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshMovementFromAttributes();
}

void AHeroCharacter::HandleJumpZVelocityChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshMovementFromAttributes();
}

void AHeroCharacter::HandleAirControlChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshMovementFromAttributes();
}

void AHeroCharacter::EnterDeathRagdoll()
{
	if (bDeathRagdollActive)
	{
		return;
	}

	bDeathRagdollActive = true;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// The dead pawn must stop receiving gameplay hits through the custom hit zones.
	UAscHitCollisionComponent::SetHitCollisionsEnabled(this, false);

	// Ragdoll PhysicsAsset is presentation-only. Never recreate SkeletalMesh physics
	// bodies/collision on a Dedicated Server.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
		MeshComp->bBlendPhysics = true;
	}
}

void AHeroCharacter::BindHealthEvents()
{
	if (bDeathEventBound)
	{
		return;
	}

	if (UAscenduntHealthComponent* HealthComp = GetAscenduntHealthComponent())
	{
		HealthComp->SetAutoFinishDeath(false);
		HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
		HealthComp->OnDeathFinished.AddUniqueDynamic(this, &ThisClass::HandleDeathFinished);
		bDeathEventBound = true;
	}
}

void AHeroCharacter::UnbindHealthEvents()
{
	if (UAscenduntHealthComponent* HealthComp = GetAscenduntHealthComponent())
	{
		HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
		HealthComp->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleDeathFinished);
		HealthComp->SetAutoFinishDeath(true);
	}

	bDeathEventBound = false;
}

void AHeroCharacter::HandleDeathStarted(AActor* OwningActor)
{
	if (OwningActor != this)
	{
		return;
	}

	const bool bIsPvE = GetWorld() && GetWorld()->GetGameState<AAscPvEGameState>() != nullptr;
	if (bIsPvE)
	{
		if (HasAuthority())
		{
			if (UAscPvEDownedComponent* DownedComp = GetPvEDownedComponent())
			{
				DownedComp->EnterDowned();
			}
		}
		return;
	}

	EnterDeathRagdoll();

	if (HasAuthority())
	{
		if (UAscenduntHealthComponent* HealthComp = GetAscenduntHealthComponent())
		{
			HealthComp->FinishDeath();
		}
	}
}

void AHeroCharacter::HandleDeathFinished(AActor* OwningActor)
{
	if (OwningActor != this)
	{
		return;
	}

	EnterDeathRagdoll();

	if (!HasAuthority())
	{
		return;
	}

	if (AAscPvEGameMode* PvEGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAscPvEGameMode>() : nullptr)
	{
		PvEGameMode->HandleHeroFullDeath(this);
		return;
	}

	if (AMatchGameMode* MatchGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMatchGameMode>() : nullptr)
	{
		MatchGM->HandleHeroDeath(this);
	}
}

void AHeroCharacter::DeinitializeForPawnReplacement(bool bPreserveHeroState)
{
	if (HeroInitComponent)
	{
		HeroInitComponent->DeinitializeForPawnReplacement(bPreserveHeroState);
	}
}


void AHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindMovementAttributes();
	// Normal replacement explicitly deinitializes before UnPossess. Shutdown remains an
	// idempotent fallback for death, disconnect, map teardown, or unexpected Destroy paths.
	if (HeroInitComponent)
	{
		HeroInitComponent->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}


void AHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AHeroCharacter::GatherVisionMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	Super::GatherVisionMeshes(OutMeshes);

	// 1인칭 메시(Mesh1P / WeaponMesh1P)는 절대 추가하지 말 것.
	// 여기서 모인 메시는 AscVisionLocalPlayerSubsystem 이 CustomDepth 를 켜는 대상이라,
	// 1P 메시가 섞이면 자기 팔에 아웃라인/엑스레이 머티리얼이 덮인다.

	if (!WeaponEquipmentComponent) return;
	if (!WeaponEquipmentComponent->GetCurrentWeapon()) return;

	if (UMeshComponent* WeaponMeshComponent = WeaponEquipmentComponent->GetCurrentWeapon()->GetActiveWeaponMesh())
	{
		OutMeshes.Add(WeaponMeshComponent);
	}
}

void AHeroCharacter::GatherXRayMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	Super::GatherXRayMeshes(OutMeshes);

	// 1인칭 메시는 절대 추가하지 말 것. 이유는 GatherVisionMeshes 참고.
	// Overlay는 원본 3P 메시의 가시성을 따른다. 별도 OwnerNoSee나 포즈 연결은 필요 없다.

	if (!WeaponEquipmentComponent) return;
	if (!WeaponEquipmentComponent->GetCurrentWeapon()) return;

	WeaponEquipmentComponent->GetCurrentWeapon()->GatherXRayMeshes(OutMeshes);
}

void AHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HeroInitComponent)
	{
		HeroInitComponent->NotifyPlayerStateChanged();
	}
}

void AHeroCharacter::UnPossessed()
{
	DeinitializeForPawnReplacement();
	Super::UnPossessed();
}

void AHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (HeroInitComponent)
	{
		HeroInitComponent->NotifyPlayerStateChanged();
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (UAscVisionLocalPlayerSubsystem* Vision = LocalPlayer->GetSubsystem<UAscVisionLocalPlayerSubsystem>())
			{
				Vision->NotifyCharacterPlayerStateChanged(this);
			}
		}
	}
}

void AHeroCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (HeroInitComponent)
	{
		HeroInitComponent->NotifyControllerChanged();
	}
}

void AHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (HeroInitComponent)
	{
		HeroInitComponent->NotifyInputComponentReady();
	}
}

bool AHeroCharacter::TryInitializeAnimationForGameplay()
{
	if (bAnimationInitializedForGameplay)
	{
		return true;
	}

	// Hero 3P pose is required on the dedicated server because gameplay hit-collision
	// components can be bone-attached. Only the 1P presentation mesh remains disabled.
	if (GetNetMode() == NM_DedicatedServer && Mesh1P)
	{
		Mesh1P->SetComponentTickEnabled(false);
		Mesh1P->SetVisibility(false, true);
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return false;
	}

	// Bone-attached hit collisions must observe the current server pose every server tick.
	MeshComp->SetComponentTickEnabled(true);
	MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	MeshComp->bEnableUpdateRateOptimizations = false;

	if (!MeshComp->GetSkeletalMeshAsset() || !MeshComp->GetAnimClass())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[HeroAnimInit] Missing mesh or anim class. Character=%s Mesh=%s SkeletalMesh=%s AnimClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MeshComp),
			*GetNameSafe(MeshComp->GetSkeletalMeshAsset()),
			*GetNameSafe(MeshComp->GetAnimClass()));
		return false;
	}

	if (!MeshComp->GetAnimInstance())
	{
		MeshComp->InitAnim(true);
	}

	if (!MeshComp->GetAnimInstance())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[HeroAnimInit] AnimInstance was not created. Character=%s Mesh=%s SkeletalMesh=%s AnimClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MeshComp),
			*GetNameSafe(MeshComp->GetSkeletalMeshAsset()),
			*GetNameSafe(MeshComp->GetAnimClass()));
		return false;
	}

	if (!LinkAnimClassLayers())
	{
		return false;
	}

	bAnimationInitializedForGameplay = true;
	return true;
}

bool AHeroCharacter::LinkAnimClassLayers()
{
	if (!AnimLayerClass)
	{
		return true;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return false;
	}

	UAnimInstance* MainAnimInst = MeshComp->GetAnimInstance();
	if (!MainAnimInst)
	{
		return false;
	}

	MainAnimInst->LinkAnimClassLayers(AnimLayerClass);
	return true;
}
