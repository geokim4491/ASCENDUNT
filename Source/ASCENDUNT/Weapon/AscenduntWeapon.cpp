#include "AscenduntWeapon.h"
#include "AbilitySystemGlobals.h"
#include "ASCENDUNT/AbilitySystem/Effects/AscWeaponAmmoCost.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_FireControlBase.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
// Fill out your copyright notice in the Description page of Project Settings.


#include "Net/UnrealNetwork.h"
#include "GameplayEffect.h"
#include "WeaponDefinitionData.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "AbilitySystemGlobals.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/System/AscVisionLocalPlayerSubsystem.h"
#include "ASCENDUNT/System/AscXRayOverlay.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"

AAscenduntWeapon::AAscenduntWeapon()
{
	bReplicates = true;
	SetReplicateMovement(true);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	if (WeaponMesh)
	{
		WeaponMesh->SetupAttachment(RootComponent);
		WeaponMesh->SetIsReplicated(false); // 무기 액터는 복제하지만, 메시는 복제하지 않음. (소유 캐릭터가 복제해서 관리)
	}

	StaticWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticWeaponMesh"));
	if (StaticWeaponMesh)
	{
		StaticWeaponMesh->SetupAttachment(RootComponent);
		StaticWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StaticWeaponMesh->SetGenerateOverlapEvents(false);
		StaticWeaponMesh->SetIsReplicated(false);
	}

	// 1인칭 전용 무기 메시. 소켓 부착과 가시성은 UHeroFirstPersonComponent 가 소유한다.
	// 여기서는 렌더 성질만 확정한다.
	WeaponMesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh1P"));
	if (WeaponMesh1P)
	{
		WeaponMesh1P->SetupAttachment(RootComponent);
		WeaponMesh1P->SetIsReplicated(false);
		WeaponMesh1P->SetOnlyOwnerSee(true);
		WeaponMesh1P->SetVisibility(false);
		WeaponMesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMesh1P->SetGenerateOverlapEvents(false);
		WeaponMesh1P->bReceivesDecals = false;
		WeaponMesh1P->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	}
}

void AAscenduntWeapon::BeginPlay()
{
	Super::BeginPlay();

	const bool bDedicatedServer = GetNetMode() == NM_DedicatedServer;
	if (bDedicatedServer && Cast<ABaseAICharacter>(GetOwner()))
	{
		// AI 무기의 SkeletalMesh는 서버 Gameplay에 필요하지 않다.
		// 서버는 탄약/Ability/Actor 상태만 유지하고, 무기 Pose/Animation은 클라이언트 연출로 분리한다.
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(this);
		for (USkeletalMeshComponent* MeshComp : SkeletalMeshes)
		{
			if (!IsValid(MeshComp))
			{
				continue;
			}

			MeshComp->SetAnimInstanceClass(nullptr);
			MeshComp->SetComponentTickEnabled(false);
		}

		// 1P 동기화는 Dedicated Server에서 불필요하다.
		return;
	}

	SyncWeaponMesh1P();

	if (!bDedicatedServer)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
			{
				if (UAscVisionLocalPlayerSubsystem* Vision = LocalPlayer->GetSubsystem<UAscVisionLocalPlayerSubsystem>())
				{
					Vision->NotifyWeaponBeganPlay(this);
				}
			}
		}
	}
}

FTransform AAscenduntWeapon::GetMuzzleTransform() const
{
	// StaticMesh 무기는 검 등 근접무기 외형 전용으로 사용한다.
	// 게임플레이 기준점은 항상 3P WeaponMesh를 사용한다.
	if (WeaponMesh)
	{
		if (WeaponMesh->DoesSocketExist(MuzzleSocketName))
		{
			return WeaponMesh->GetSocketTransform(MuzzleSocketName);
		}
		return WeaponMesh->GetComponentTransform();
	}
	return GetActorTransform();
}

FTransform AAscenduntWeapon::GetMuzzleVFXTransform() const
{
	// 연출은 이 머신에서 실제로 보이는 무기를 기준으로 한다.
	// 로컬 오너의 1P WeaponMesh1P가 활성화된 경우에만 1P Muzzle을 선택하고,
	// Dedicated Server와 원격 클라이언트에서는 자연스럽게 3P WeaponMesh로 fallback 된다.
	if (const USkeletalMeshComponent* MuzzleMesh = GetMuzzleAttachMesh())
	{
		if (MuzzleMesh->DoesSocketExist(MuzzleSocketName))
		{
			return MuzzleMesh->GetSocketTransform(MuzzleSocketName);
		}
		return MuzzleMesh->GetComponentTransform();
	}

	return GetMuzzleTransform();
}


bool AAscenduntWeapon::IsFirstPersonWeaponVisible() const
{
	return WeaponMesh1P
		&& WeaponMesh1P->IsVisible()
		&& WeaponMesh1P->GetAttachParent() != nullptr
		&& WeaponMesh1P->DoesSocketExist(MuzzleSocketName);
}

USkeletalMeshComponent* AAscenduntWeapon::GetMuzzleAttachMesh() const
{
	return IsFirstPersonWeaponVisible() ? WeaponMesh1P : WeaponMesh;
}
void AAscenduntWeapon::EquipToASC(UAscenduntAbilitySystemComponent* AscenduntASC)
{
	check(AscenduntASC);
	check(WeaponData);

	if (!AscenduntASC->IsOwnerActorAuthoritative())
	{
		// GiveToAbilitySystem이 Authority 전용이라 여기서도 막아둠
		return;
	}

	CachedAscASC = AscenduntASC;
	NextAllowedFireTimeServer = 0.;
	NextAllowedFireTimePredicted = 0.;
	// 1) 무기 능력 부여 (발사/재장전/모드전환 등)
	if (WeaponData->WeaponAbilitySet)
	{
		WeaponData->WeaponAbilitySet->GiveToAbilitySystem(
            AscenduntASC,
            &GrantedHandles,
            /*SourceObject*/ this,
            EAscAttributeSetGrantPolicy::Skip);
	}

	// 2) 무기 스탯을 Attribute로 주입하는 GE 적용(SetByCaller)
	ApplyWeaponStatsEffect(AscenduntASC);

	// 3) 탄 초기화(탄창크기는 Attribute로 들어가므로, 그 값 기준)
	InitAmmoFromAttributes(AscenduntASC);
	BindAmmoAttributes();

	// 4) 소유권: Owner 는 항상 Pawn 으로 잡는다.
	//
	// RPC 라우팅은 Controller 를 Owner 로 잡지 않아도 보장된다.
	// AActor::GetNetConnection() 은 Owner 를 타고 올라가고 APawn::GetNetConnection() 은
	// 자신의 Controller 커넥션을 돌려주므로, Weapon -> Pawn -> Controller -> Connection 으로 이어진다.
	//
	// 반대로 Controller 를 Owner 로 잡으면 소유자 체인이 Weapon -> Controller 에서 끝나
	// 뷰 타깃인 Pawn 이 체인에 들어가지 않는다. 그러면 두 가지가 조용히 깨진다.
	//   1) 1인칭에서 3P 무기 메시를 숨기는 bOwnerNoSee 가 동작하지 않는다.
	//      (UHeroFirstPersonComponent 의 WorldSpaceRepresentation 이 몸통에만 먹고 무기에는 안 먹음)
	//   2) Multicast_PlayWeaponMontage / Multicast_StopWeaponMontage 의
	//      Cast<APawn>(GetOwner()) 로컬 오너 판정이 항상 실패해서, 오너 클라가 예측 재생한
	//      몽타주 위에 멀티캐스트가 한 번 더 덮어쓴다.
	if (APawn* Pawn = Cast<APawn>(AscenduntASC->GetAvatarActor()))
	{
		SetOwner(Pawn);
	}
	else
	{
		SetOwner(AscenduntASC->GetAvatarActor());
	}

	// 5) Instigator
	if (APawn* Pawn = Cast<APawn>(AscenduntASC->GetAvatarActor()))
	{
		SetInstigator(Pawn);
	}

	// 6) CachedAscASC
	CachedAscASC = AscenduntASC;
}

void AAscenduntWeapon::UnequipFromASC(UAscenduntAbilitySystemComponent* AscenduntASC)
{
	check(AscenduntASC);

	if (!AscenduntASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	ClearPredictedWeaponState();
	UnbindAmmoAttributes();
	// 1) 무기 스탯 GE 제거
	RemoveWeaponStatsEffect(AscenduntASC);

	// 2) 무기 능력 회수
	GrantedHandles.TakeFromAbilitySystem(AscenduntASC);
}

void AAscenduntWeapon::ResetForReuse()
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentModeIndex = 0;
	bReloading = false;

	NextAllowedFireTimeServer = 0.0;
	NextAllowedFireTimePredicted = 0.0;

	bServerMeleeWindowActive = false;
	ServerMeleeWindowStepIndex = 0;

	// 근접 공격 중에 죽으면 GA_Melee::UnbindWeaponHitWindow가 누락될 수 있다.
	// 파괴된 어빌리티 인스턴스를 가리키는 바인딩이 남지 않도록 정리한다.
	OnMeleeHitWindow.Clear();

	CachedAscASC = nullptr;
}

USkeletalMeshComponent* AAscenduntWeapon::GetWeaponMesh() const
{
	return WeaponMesh;
}

UStaticMeshComponent* AAscenduntWeapon::GetStaticWeaponMesh() const
{
	return StaticWeaponMesh;
}

UMeshComponent* AAscenduntWeapon::GetActiveWeaponMesh() const
{
	// 기존 BP 호환성: 둘 다 에셋이 지정되어 있으면 SkeletalMesh를 우선한다.
	if (IsValid(WeaponMesh) && WeaponMesh->GetSkeletalMeshAsset())
	{
		return WeaponMesh;
	}

	if (IsValid(StaticWeaponMesh) && StaticWeaponMesh->GetStaticMesh())
	{
		return StaticWeaponMesh;
	}

	return nullptr;
}

void AAscenduntWeapon::GatherXRayMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	if (UMeshComponent* ActiveMesh = GetActiveWeaponMesh())
	{
		OutMeshes.AddUnique(ActiveMesh);
	}
}

void AAscenduntWeapon::SetXRayMeshEnabled(bool bEnabled, float HealthNorm, float XRayAlpha)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UMeshComponent* ActiveMesh = GetActiveWeaponMesh();
	UMeshComponent* SourceMeshes[] = { WeaponMesh.Get(), StaticWeaponMesh.Get() };
	for (UMeshComponent* Mesh : SourceMeshes)
	{
		if (!IsValid(Mesh))
		{
			continue;
		}

		if (bEnabled && IsValid(XRayOverlayMaterial) && Mesh == ActiveMesh && Mesh->IsRegistered())
		{
			Mesh->SetCustomPrimitiveDataFloat(0, FMath::Clamp(HealthNorm, 0.f, 1.f));
			Mesh->SetCustomPrimitiveDataFloat(1, FMath::Clamp(XRayAlpha, 0.f, 1.f));
			TArray<TObjectPtr<UMaterialInterface>> SlotOverlays;
			SlotOverlays.Init(XRayOverlayMaterial, FMath::Max(1, Mesh->GetNumMaterials()));
			AscXRayOverlay::SetMaterials(Mesh, XRayOverlayMaterial, SlotOverlays);
		}
		else
		{
			// Inactive/holstered weapons must not retain an overlay after a weapon change.
			AscXRayOverlay::SetMaterials(Mesh, nullptr);
		}
	}
}

void AAscenduntWeapon::SetXRayMaterial(UMaterialInterface* Material)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		XRayOverlayMaterial = Material;
	}
}

void AAscenduntWeapon::NotifyMeleeHitWindow(bool bBegin, int32 StepIndex)
{
	if (HasAuthority())
	{
		HandleMeleeHitWindow_Server(bBegin, StepIndex);
		return;
	}

	// The owning client is the single notify source for remote players.
	Server_NotifyMeleeHitWindow(bBegin, StepIndex);
}

void AAscenduntWeapon::HandleMeleeHitWindow_Server(bool bBegin, int32 StepIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bBegin)
	{
		// Duplicate Begin for the same montage window must not reset the GA's
		// HitActorsThisWindow set and allow the same attack to damage twice.
		if (bServerMeleeWindowActive && ServerMeleeWindowStepIndex == StepIndex)
		{
			return;
		}

		bServerMeleeWindowActive = true;
		ServerMeleeWindowStepIndex = StepIndex;
		OnMeleeHitWindow.Broadcast(true, StepIndex);
		return;
	}

	// Duplicate/stale End notifications are ignored.
	if (!bServerMeleeWindowActive || ServerMeleeWindowStepIndex != StepIndex)
	{
		return;
	}

	bServerMeleeWindowActive = false;
	OnMeleeHitWindow.Broadcast(false, StepIndex);
}

void AAscenduntWeapon::Multicast_PlayWeaponMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	// 오너(로컬 컨트롤)는 예측으로 이미 재생했을 확률이 높으니 스킵
	if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		if (OwningPawn->IsLocallyControlled())
		{
			return;
		}
	}

	if (!Montage)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = GetWeaponMesh(); // 너 프로젝트의 무기 메쉬 getter로 교체
	if (!Mesh)
	{
		return;
	}

	if (UAnimInstance* Anim = Mesh->GetAnimInstance())
	{
		Anim->Montage_Play(Montage, PlayRate);
	}
}

void AAscenduntWeapon::Multicast_StopWeaponMontage_Implementation(float BlendOutTime)
{
	// 오너는 로컬 예측에서 이미 0초 블렌드로 전환했을 수 있으므로 서버 multicast stop으로 새 몽타주를 끊지 않는다.
	if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		if (OwningPawn->IsLocallyControlled())
		{
			return;
		}
	}

	USkeletalMeshComponent* Mesh = GetWeaponMesh();
	if (!Mesh)
	{
		return;
	}

	if (UAnimInstance* Anim = Mesh->GetAnimInstance())
	{
		Anim->Montage_Stop(FMath::Max(0.f, BlendOutTime));
	}
}

void AAscenduntWeapon::Multicast_DrawAOEDebugSphere_Implementation(
	FVector_NetQuantize ImpactPoint, float Radius, float Duration)
{
#if ENABLE_DRAW_DEBUG
	if (GetNetMode() == NM_DedicatedServer || Radius <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(
			World,
			FVector(ImpactPoint),
			Radius,
			32,
			FColor::Cyan,
			false,
			FMath::Max(0.f, Duration),
			0,
			2.f);
	}
#endif
}

void AAscenduntWeapon::SetReloading_Server(bool bInReloading)
{
	check(HasAuthority());
	bReloading = bInReloading;
	// 서버는 OnRep가 자동 호출되지 않음(필요하면 여기서 직접 호출)
}

void AAscenduntWeapon::SetReloading_Predicted(bool bInReloading)
{
	bReloading = bInReloading;
	// 예측으로 미리 값 바꾸면 서버에서 값 변경 후 OnRep가 호출되지 않을 수 있어 직접 호출.
	OnRep_Reloading(); // 오너클라 UI/사운드 갱신용
}

void AAscenduntWeapon::SetAmmoInMag_Server(int32 NewAmmo)
{
	check(HasAuthority());
	if (UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC())
		ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetCurrentAmmoAttribute(), float(FMath::Clamp(NewAmmo, 0, GetMagSize())));
}




float AAscenduntWeapon::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	if (!WeaponData)
	{
		return 1.f;
	}

	const FAscenduntWeaponModeStats& S = GetCurrentModeStats();
	
	// 사거리 밖이면 0 처리
	if (S.MaxRange > 0.f && Distance > S.MaxRange)
	{
		return 0.f;
	}

	if (S.DistanceFalloffCurve)
	{
		// Curve는 보통 x=Distance, y=Multiplier (0~1 or 그 이상)
		return S.DistanceFalloffCurve->GetFloatValue(Distance);
	}

	return 1.f;
}

float AAscenduntWeapon::GetHeadshotDamageMultiplier(const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	if (!WeaponData)
	{
		return 1.0f;
	}

	AActor* OwnerActor = GetInstigator();
	UAscenduntAbilitySystemComponent* AscASC =
		Cast<UAscenduntAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor));
	if (!AscASC) return 1.0;

	return FMath::Max(1.0f, AscASC->GetNumericAttribute(UAscenduntWeaponSet::GetHeadshotDamageMultiplierAttribute()));
}

const FAscenduntWeaponModeStats& AAscenduntWeapon::GetCurrentModeStats() const
{
	if (WeaponData && WeaponData->Modes.IsValidIndex(CurrentModeIndex))
	{
		return WeaponData->Modes[CurrentModeIndex].Stats;
	}
	
	// 엔진 크래시 방지
	static FAscenduntWeaponModeStats Dummy;
	return Dummy;
}

bool AAscenduntWeapon::IsInfiniteMag() const
{
	const UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC();
	return GetCurrentModeStats().bInfiniteMag || (ASC && ASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_InfiniteMag));
}

void AAscenduntWeapon::ConsumeAmmo_Server(int32 AmmoCost)
{
	AdjustAmmo_Server(-FMath::Max(0, AmmoCost));
}

UAscenduntAbilitySystemComponent* AAscenduntWeapon::GetCachedAscASC() const
{
	const AActor* OwnerActor = GetOwner();
	if (const UWeaponEquipmentComponent* Equipment = OwnerActor ? OwnerActor->FindComponentByClass<UWeaponEquipmentComponent>() : nullptr)
		if (Equipment->GetCurrentWeapon() != this) return nullptr;
	return CachedAscASC ? CachedAscASC.Get() : Cast<UAscenduntAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor));
}

void AAscenduntWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAscenduntWeapon, CurrentModeIndex);
	DOREPLIFETIME(AAscenduntWeapon, bReloading);
}

void AAscenduntWeapon::Server_NotifyMeleeHitWindow_Implementation(bool bBegin, int32 StepIndex)
{
	HandleMeleeHitWindow_Server(bBegin, StepIndex);
}

float AAscenduntWeapon::GetMaxRange() const
{
	return GetCurrentModeStats().MaxRange;
}

bool AAscenduntWeapon::SetWeaponModeByIndex(const int32 NewModeIndex)
{
	// CurrentModeIndex 는 복제 프로퍼티다. 서버만 바꾼다.
	if (!HasAuthority() || !WeaponData || !WeaponData->Modes.IsValidIndex(NewModeIndex))
	{
		return false;
	}

	if (NewModeIndex == CurrentModeIndex)
	{
		return true;
	}

	// 재장전 중 전환은 탄약과 재장전 타이머를 어긋나게 만든다.
	// 전환이 필요하면 호출부가 먼저 재장전을 끝내거나 취소해야 한다.
	if (bReloading)
	{
		return false;
	}

	const int32 PreviousAmmo = GetCurrentAmmoInMag();
	if (UGA_FireControlBase* Control = GetActiveFireControl())
		if (UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC()) ASC->CancelAbilityHandle(Control->GetCurrentAbilitySpecHandle());
	ClearPredictedWeaponState();
	UAscenduntAbilitySystemComponent* AscASC = GetCachedAscASC();

	// 무기 스탯 GE 는 "현재 모드의 Stats" 로 스펙을 만든다.
	// 인덱스를 바꾸기 전에 걷어내야 새 모드 수치로 다시 만들어진다.
	if (AscASC)
	{
		RemoveWeaponStatsEffect(AscASC);
	}

	CurrentModeIndex = NewModeIndex;

	if (AscASC)
	{
		ApplyWeaponStatsEffect(AscASC);
		InitAmmoFromAttributes(AscASC);
	}
	else
	{
		// Unequipped mode configuration has no live ammo Attribute to change.
	}

	// InitAmmoFromAttributes 는 탄창을 가득 채운다.
	// 모드 전환이 재장전을 대신하면 안 되므로 이전 잔탄을 새 탄창 크기로 잘라 되돌린다.
	SetAmmoInMag_Server(FMath::Clamp(PreviousAmmo, 0, GetMagSize()));

	// 서버에서는 OnRep 이 호출되지 않는다. 같은 갱신을 직접 돌린다.
	HandleModeChanged();

	return true;
}

bool AAscenduntWeapon::SetWeaponModeByTag(const FGameplayTag ModeTag)
{
	if (!WeaponData || !ModeTag.IsValid())
	{
		return false;
	}

	for (int32 Index = 0; Index < WeaponData->Modes.Num(); ++Index)
	{
		if (WeaponData->Modes[Index].ModeTag.MatchesTagExact(ModeTag))
		{
			return SetWeaponModeByIndex(Index);
		}
	}

	return false;
}

FGameplayTag AAscenduntWeapon::GetCurrentModeTag() const
{
	if (WeaponData && WeaponData->Modes.IsValidIndex(CurrentModeIndex))
	{
		return WeaponData->Modes[CurrentModeIndex].ModeTag;
	}

	return FGameplayTag();
}

void AAscenduntWeapon::HandleModeChanged()
{
	// 모드마다 탄창 크기가 다르므로 HUD 를 새 값으로 맞춘다.
	OnAmmoChanged.Broadcast(GetCurrentAmmoInMag(), GetMagSize());

	// 이전 모드의 발사 간격으로 잡아둔 게이트를 새 모드가 물려받지 않게 한다.
	// 느린 모드에서 빠른 모드로 바꿨을 때 첫 발이 불필요하게 지연되는 것을 막는다.
	NextAllowedFireTimeServer = 0.0;
	NextAllowedFireTimePredicted = 0.0;
}

void AAscenduntWeapon::OnRep_CurrentModeIndex()
{
	if (UGA_FireControlBase* Control = GetActiveFireControl())
		if (UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC()) ASC->CancelAbilityHandle(Control->GetCurrentAbilitySpecHandle());
	ClearPredictedWeaponState();
	// 클라이언트에서도 HUD 를 서버와 같은 값으로 맞춘다.
	HandleModeChanged();
}





void AAscenduntWeapon::OnRep_Reloading()
{
	// UI(재장전 아이콘), 사운드, 애니 트리거 등
}

void AAscenduntWeapon::ApplyWeaponStatsEffect(UAscenduntAbilitySystemComponent* AscenduntASC)
{
	if (!WeaponData || !WeaponData->WeaponStatsEffect)
	{
		return;
	}

	const FAscenduntWeaponModeStats& S = GetCurrentModeStats();

	FGameplayEffectContextHandle Ctx = AscenduntASC->MakeEffectContext();
	Ctx.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = AscenduntASC->MakeOutgoingSpec(WeaponData->WeaponStatsEffect, /*Level*/1.f, Ctx);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	check(Spec);

	// --- SetByCaller inject ---
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_RateOfFire, S.RateOfFire);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_ShotsPerTrigger, (float)S.ShotsPerTrigger);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_AmmoCostPerShot, (float)S.AmmoCostPerShot);

	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_MagSize, (float)S.MagSize);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_ReloadTime, S.ReloadTime);

	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_BaseSpreadDeg, S.SpreadRecoil.BaseSpreadDeg);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_SpreadIncPerShot, S.SpreadRecoil.SpreadIncPerShot);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_SpreadRecoverPerSec, S.SpreadRecoil.SpreadRecoverPerSec);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_RecoilPitch, S.SpreadRecoil.RecoilPitch);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_RecoilYaw, S.SpreadRecoil.RecoilYaw);

	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_HeadshotDamageMultiplier, S.HeadshotDamageMultiplier);

	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_MoveSpeedScalarWhileFiring, S.MoveSpeedScalarWhileFiring);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_MoveSpeedScalarWhileADS, S.MoveSpeedScalarWhileADS);

	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_ProjectileSpeed, S.ProjectileSpeed);
	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_ProjectileScale, S.ProjectileScale);

	Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_MeleePlayRate, S.MeleePlayRate);
	// 보류. Spec->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_MeleeDamageCoef, S.MeleeSteps[0].DamageCoef);

	// --- WeaponData bool -> Granted Tag 초기 태그 부여
	if (S.bInfiniteMag)
		Spec->DynamicGrantedTags.AddTag(AscenduntGameplayTags::Data_Weapon_InfiniteMag);

	if (S.bAllowHeadshot)
		Spec->DynamicGrantedTags.AddTag(AscenduntGameplayTags::Data_Weapon_AllowHeadshot);

	if(S.bProjectileCanBeNullified)
		Spec->DynamicGrantedTags.AddTag(AscenduntGameplayTags::Data_Weapon_ProjectileCanBeNullified);

	WeaponStatsGEHandle = AscenduntASC->ApplyGameplayEffectSpecToSelf(*Spec);
}

void AAscenduntWeapon::RemoveWeaponStatsEffect(UAscenduntAbilitySystemComponent* AscenduntASC)
{
	if (WeaponStatsGEHandle.IsValid())
	{
		AscenduntASC->RemoveActiveGameplayEffect(WeaponStatsGEHandle);
		WeaponStatsGEHandle.Invalidate();
	}
}

void AAscenduntWeapon::InitAmmoFromAttributes(UAscenduntAbilitySystemComponent* AscenduntASC)
{
	check(HasAuthority());
	AscenduntASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetCurrentAmmoAttribute(), float(GetMagSize()));
}

void AAscenduntWeapon::SyncWeaponMesh1P()
{
	if (!IsValid(WeaponMesh1P) || !IsValid(WeaponMesh))
	{
		return;
	}

	// 1P 무기는 정의상 3P 와 같은 총이다. 애셋을 따로 지정하게 두면 둘이 갈라진다.
	WeaponMesh1P->SetSkeletalMesh(WeaponMesh->GetSkeletalMeshAsset());

	// 포즈는 3P 를 그대로 따라간다. 무기 몽타주는 1P/3P 가 같은 동작이므로
	// 1P 전용 재생 경로를 만들 이유가 없고, LeaderPose 면 어긋날 수도 없다.
	WeaponMesh1P->SetLeaderPoseComponent(WeaponMesh);
}

int32 AAscenduntWeapon::GetCurrentAmmoInMag() const
{
	const UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC();
	return ASC ? FMath::Max(0, FMath::RoundToInt(ASC->GetNumericAttribute(UAscenduntWeaponSet::GetCurrentAmmoAttribute()))) : 0;
}

int32 AAscenduntWeapon::GetMagSize() const
{
	const UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC();
	return ASC ? FMath::Max(0, FMath::RoundToInt(ASC->GetNumericAttribute(UAscenduntWeaponSet::GetMagSizeAttribute())))
	           : FMath::Max(0, GetCurrentModeStats().MagSize);
}

void AAscenduntWeapon::AdjustAmmo_Server(int32 Delta)
{
	check(HasAuthority());
	UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC();
	if (!ASC || IsInfiniteMag() || Delta == 0)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UAscWeaponAmmoAdjustment::StaticClass(), 1.f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Weapon_AmmoDelta, float(Delta));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void AAscenduntWeapon::BindAmmoAttributes()
{
	UnbindAmmoAttributes();
	UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC();
	if (!ASC)
	{
		return;
	}

	BoundAmmoASC = ASC;
	AmmoChangedHandle =
	    ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute()).AddUObject(this, &ThisClass::HandleAmmoAttributeChanged);
	MagChangedHandle =
	    ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetMagSizeAttribute()).AddUObject(this, &ThisClass::HandleAmmoAttributeChanged);
	OnAmmoChanged.Broadcast(GetCurrentAmmoInMag(), GetMagSize());
}

void AAscenduntWeapon::UnbindAmmoAttributes()
{
	if (UAscenduntAbilitySystemComponent* ASC = BoundAmmoASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute()).Remove(AmmoChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetMagSizeAttribute()).Remove(MagChangedHandle);
	}

	BoundAmmoASC.Reset();
	AmmoChangedHandle.Reset();
	MagChangedHandle.Reset();
}

void AAscenduntWeapon::HandleAmmoAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnAmmoChanged.Broadcast(GetCurrentAmmoInMag(), GetMagSize());
}

void AAscenduntWeapon::SetActiveFireControl(UGA_FireControlBase* Control)
{
	ActiveFireControl = Control;
}

UGA_FireControlBase* AAscenduntWeapon::GetActiveFireControl() const
{
	return ActiveFireControl.Get();
}

void AAscenduntWeapon::TrackPredictedAmmoCost(FActiveGameplayEffectHandle Handle)
{
	if (!HasAuthority() && Handle.IsValid())
	{
		PredictedAmmoCosts.Add(Handle);
	}
}

void AAscenduntWeapon::ClearPredictedWeaponState(UAscenduntAbilitySystemComponent* PreviousASC)
{
	if (!HasAuthority())
	{
		UAscenduntAbilitySystemComponent* ASC = PreviousASC ? PreviousASC : BoundAmmoASC.IsValid() ? BoundAmmoASC.Get() : CachedAscASC.Get();
		if (!ASC)
		{
			ASC = Cast<UAscenduntAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
		}

		if (ASC)
		{
			for (FActiveGameplayEffectHandle Handle : PredictedAmmoCosts)
			{
				ASC->RemovePredictedWeaponAmmoCost(Handle, this);
			}
		}
	}

	PredictedAmmoCosts.Reset();
	LastPredictedShot = {};
	StopFirePresentation(FirePresentationSerial);
	LastPredictedShotKey = 0;
	PredictedReloadKey = 0;
	bAwaitingReloadConfirmation = false;
	NextAllowedFireTimePredicted = 0.;
}

void AAscenduntWeapon::BeginPredictedReload(FPredictionKey Key)
{
	PredictedReloadKey = Key.Current;
	bAwaitingReloadConfirmation = !HasAuthority();
	SetReloading_Predicted(true);
	if (Key.IsLocalClientKey())
	{
		Key.NewRejectedDelegate().BindUObject(this, &ThisClass::FinishPredictedReload, Key.Current);
	}
}

void AAscenduntWeapon::FinishPredictedReload(int16 Key)
{
	if (!bAwaitingReloadConfirmation || PredictedReloadKey != Key)
	{
		return;
	}

	bAwaitingReloadConfirmation = false;
	PredictedReloadKey = 0;
	SetReloading_Predicted(false);
}

void AAscenduntWeapon::RegisterPredictedShot(FPredictionKey Key, double PreviousDeadline)
{
	if (HasAuthority() || !Key.IsLocalClientKey())
	{
		return;
	}

	LastPredictedShot.PresentationSerial = FirePresentationSerial;
	LastPredictedShot.PreviousDeadline = PreviousDeadline;
	LastPredictedShotKey = Key.Current;
	Key.NewRejectedDelegate().BindUObject(this, &ThisClass::RejectPredictedShot, Key.Current);
	Key.NewCaughtUpDelegate().BindUObject(this, &ThisClass::ConfirmPredictedShot, Key.Current);
}

void AAscenduntWeapon::RejectPredictedShot(int16 Key)
{
	UE_LOG(LogTemp, Verbose, TEXT("[WeaponShotRejected] Weapon=%s Key=%d Ammo=%d"), *GetName(), Key, GetCurrentAmmoInMag());
	if (LastPredictedShotKey != Key)
	{
		return;
	}

	StopFirePresentation(LastPredictedShot.PresentationSerial);
	NextAllowedFireTimePredicted = LastPredictedShot.PreviousDeadline;
	LastPredictedShotKey = 0;
	LastPredictedShot = {};
}

void AAscenduntWeapon::ConfirmPredictedShot(int16 Key)
{
	if (LastPredictedShotKey == Key)
	{
		LastPredictedShotKey = 0;
		LastPredictedShot = {};
	}

	const UAscenduntAbilitySystemComponent* ASC = GetCachedAscASC();
	PredictedAmmoCosts.RemoveAll([ASC](FActiveGameplayEffectHandle Handle) { return !ASC || !ASC->GetActiveGameplayEffect(Handle); });
}

void AAscenduntWeapon::PlayFirePresentation(float Interval)
{
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	++FirePresentationSerial;
	FireMontages.Reset();
	const FAscenduntWeaponModeStats& Stats = GetCurrentModeStats();
	const auto Play = [this, &Stats, Interval](USkeletalMeshComponent* Mesh, UAnimMontage* Montage)
	{
		UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!Anim || !Montage)
		{
			return;
		}

		const float Rate = Stats.FireMontagePlayRateOverride > 0.f ? Stats.FireMontagePlayRateOverride : Montage->GetPlayLength() / FMath::Max(0.01f, Interval);
		Anim->Montage_Play(Montage, Rate);
		if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage))
		{
			FAscWeaponFireMontage Playback;
			Playback.Anim = Anim;
			Playback.Montage = Montage;
			Playback.InstanceID = Instance->GetInstanceID();
			FireMontages.Add(Playback);
		}
	};
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Play(Character->GetMesh(), Stats.HeroFireMontage);
	}

	Play(WeaponMesh, Stats.WeaponFireMontage);
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner()); Hero && Hero->IsLocallyControlled())
	{
		if (UHeroFirstPersonComponent* FirstPerson = Hero->GetFirstPersonComponent())
		{
			if (FirstPerson->IsFirstPersonActive())
			{
				Play(Hero->GetMesh1P(), Stats.HeroFireMontage1P);
			}

			FirstPerson->NotifyFired1P();
		}
	}
}

void AAscenduntWeapon::StopFirePresentation(uint32 Serial)
{
	if (Serial != FirePresentationSerial)
	{
		return;
	}

	for (const FAscWeaponFireMontage& Playback : FireMontages)
	{
		if (UAnimInstance* Anim = Playback.Anim.Get())
		{
			FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Playback.Montage.Get());
			if (Instance && Instance->GetInstanceID() == Playback.InstanceID)
			{
				Anim->Montage_Stop(0.1f, Playback.Montage.Get());
			}
		}
	}

	FireMontages.Reset();
}

void AAscenduntWeapon::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearPredictedWeaponState();
	UnbindAmmoAttributes();
	Super::EndPlay(Reason);
}
