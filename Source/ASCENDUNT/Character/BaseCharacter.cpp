// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCharacter.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ASCENDUNT/System/AscCollisionChannels.h"
#include "ASCENDUNT/Character/AscenduntCharacterMovementComponent.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/AscVisionMarkerComponent.h"
#include "ASCENDUNT/Character/AscTargetStatusDisplayComponent.h"
#include "ASCENDUNT/UI/WorldStatus/AscWorldStatusLayoutData.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

#include "Net/UnrealNetwork.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

namespace AscBodyAimDebug
{
	static int32 GEnabled = 0;
	static FAutoConsoleVariableRef CVarEnabled(
		TEXT("asc.DebugBodyAim"),
		GEnabled,
		TEXT("3인칭 몸 방향 정렬 진단 화살표. 1=켬. 초록=액터 forward(리플리케이트되는 유일한 방향 채널), 빨강=root 본 forward(RotateRootBone 적용된 실제 렌더 방향), 자홍=로컬 플레이어 폰을 향하는 기준선."),
		ECVF_Cheat);

	/**
	 * 본이 레퍼런스 포즈 대비 얼마나 회전했는지를, 액터 forward 를 그만큼 돌린 방향으로 돌려준다.
	 *
	 * 본의 로컬 축(X 가 뼈 방향 등)은 본마다 제각각이라 GetForwardVector() 를 그대로 쓰면
	 * 의미 없는 값이 나온다. 대신 "레퍼런스 포즈에서 현재 포즈까지의 회전 델타" 만 뽑아
	 * 액터 forward 에 적용한다. 레퍼런스 포즈면 액터 forward 와 정확히 일치하므로,
	 * 결과 각도가 곧 "애니메이션이 이 본을 액터 기준으로 몇 도 돌렸는가" 가 된다.
	 *
	 * @return 유효한 값을 구했으면 true
	 */
	static bool GetBoneFacingFromRefPose(const USkeletalMeshComponent* Mesh, FName BoneName,
		const FVector& ActorForward, FVector& OutFacing)
	{
		const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!MeshAsset)
		{
			return false;
		}

		const FReferenceSkeleton& RefSkel = MeshAsset->GetRefSkeleton();
		const int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return false;
		}

		// 레퍼런스 포즈를 부모까지 누적해 컴포넌트 공간 회전을 만든다.
		const TArray<FTransform>& RefPose = RefSkel.GetRefBonePose();
		FTransform RefCS = FTransform::Identity;
		for (int32 Idx = BoneIndex; Idx != INDEX_NONE; Idx = RefSkel.GetParentIndex(Idx))
		{
			RefCS = RefCS * RefPose[Idx];
		}

		const FTransform ComponentToWorld = Mesh->GetComponentTransform();
		const FQuat NowCS = ComponentToWorld.InverseTransformRotation(
			Mesh->GetBoneTransform(BoneIndex).GetRotation());

		const FQuat DeltaCS = NowCS * RefCS.GetRotation().Inverse();

		FVector Local = ComponentToWorld.InverseTransformVectorNoScale(ActorForward);
		Local = DeltaCS.RotateVector(Local);

		OutFacing = ComponentToWorld.TransformVectorNoScale(Local) * FVector(1.f, 1.f, 0.f);
		OutFacing = OutFacing.GetSafeNormal();
		return !OutFacing.IsNearlyZero();
	}
}
#endif

ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAscenduntCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	// ----- Capsule: movement/physics -----
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionProfileName(TEXT("AscPawnCapsule"));

	// ----- Mesh: presentation only -----
	// Gameplay hit detection is handled exclusively by editor-placeable
	// UAscHitCollisionComponent instances. Keep PhysicsAsset out of normal hit queries.
	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMesh->SetGenerateOverlapEvents(false);
	SkeletalMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	// XRayBodyMesh와 관련 머티리얼은 여기서 만들지 않는다.
	// X-Ray가 필요한 Hero만 InitializeVisionPresentationSupport()를 호출한다.
	// 따라서 대규모 AI에는 추가 SkeletalMeshComponent가 생성되지 않는다.

	VisionMarkerComponent = CreateDefaultSubobject<UAscVisionMarkerComponent>(TEXT("VisionMarkerComponent"));
	TargetStatusDisplayComponent = CreateDefaultSubobject<UAscTargetStatusDisplayComponent>(TEXT("TargetStatusDisplayComponent"));

	static ConstructorHelpers::FObjectFinder<UAscWorldStatusLayoutData> WorldStatusLayoutDataFinder(
		TEXT("/Game/UI/Data/DA_WorldStatusLayout_Character.DA_WorldStatusLayout_Character"));
	if (WorldStatusLayoutDataFinder.Succeeded())
	{
		TargetStatusDisplayComponent->SetConfig(WorldStatusLayoutDataFinder.Object);
	}
}

FGenericTeamId ABaseCharacter::GetGenericTeamId() const
{
	// Players: authoritative on PlayerState
	if (const APlayerState* PS = GetPlayerState())
	{
		if (const IGenericTeamAgentInterface* PSTeam = Cast<IGenericTeamAgentInterface>(PS))
		{
			return PSTeam->GetGenericTeamId();
		}
	}

	// AI / fallback: replicated on pawn
	return FGenericTeamId(TeamId);
}

void ABaseCharacter::SetGenericTeamId(const FGenericTeamId& NewID)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 NewTeamId = NewID.GetId();
	if (TeamId != NewTeamId)
	{
		TeamId = NewTeamId;
		ForceNetUpdate();
	}

	// Keep PlayerState in sync (players)
	if (APlayerState* PS = GetPlayerState())
	{
		if (IGenericTeamAgentInterface* PSTeam = Cast<IGenericTeamAgentInterface>(PS))
		{
			if (PSTeam->GetGenericTeamId() != NewID)
			{
				PSTeam->SetGenericTeamId(NewID);
			}
		}
	}

	// Keep Controller in sync (AI)
	if (AController* C = GetController())
	{
		if (IGenericTeamAgentInterface* CTeam = Cast<IGenericTeamAgentInterface>(C))
		{
			if (CTeam->GetGenericTeamId() != NewID)
			{
				CTeam->SetGenericTeamId(NewID);
			}
		}
	}
}

UAscenduntHealthComponent* ABaseCharacter::GetAscenduntHealthComponent() const
{
	return AscenduntHealthComponent;
}

void ABaseCharacter::InitializeVisionPresentationSupport()
{
	// 반드시 파생 클래스 생성자에서만 호출한다. 같은 이름의 Native Subobject를 중복 생성하지 않는다.
	if (!XRayBodyMesh)
	{
		XRayBodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("XRayBodyMesh"));
		XRayBodyMesh->SetupAttachment(GetMesh());
		XRayBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		XRayBodyMesh->SetGenerateOverlapEvents(false);
		XRayBodyMesh->SetCastShadow(false);
		XRayBodyMesh->SetRenderCustomDepth(false);
		XRayBodyMesh->SetRenderInMainPass(true);
		XRayBodyMesh->bReceivesDecals = false;
		XRayBodyMesh->SetVisibility(false, true);
		XRayBodyMesh->SetHiddenInGame(true, true);
		XRayBodyMesh->SetIsReplicated(false);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		OverlayMaterialAsset(TEXT("/Game/Materials/M_EnemyOverlay.M_EnemyOverlay"));
	if (OverlayMaterialAsset.Succeeded())
	{
		EnemyOverlayMaterial = OverlayMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		XRayMaterialAsset(TEXT("/Game/Materials/M_XRayMesh_Ally.M_XRayMesh_Ally"));
	if (XRayMaterialAsset.Succeeded())
	{
		XRayMeshMaterial = XRayMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		XRayHiddenMaterialAsset(TEXT("/Game/Materials/M_XRayHidden.M_XRayHidden"));
	if (XRayHiddenMaterialAsset.Succeeded())
	{
		XRayHiddenMaterial = XRayHiddenMaterialAsset.Object;
	}
}

void ABaseCharacter::SetEnemyOverlayEnabled(bool bEnabled)
{
	if (!SupportsEnemyOverlay())
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	GatherVisionMeshes(Meshes);

	for (UMeshComponent* MeshComponent : Meshes)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		MeshComponent->SetOverlayMaterial(bEnabled ? EnemyOverlayMaterial : nullptr);
		MeshComponent->MarkRenderStateDirty();
	}
}

void ABaseCharacter::GatherVisionMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	OutMeshes.Reset();

	if (USkeletalMeshComponent* BaseMesh = GetMesh())
	{
		OutMeshes.Add(BaseMesh);
	}
}

void ABaseCharacter::GatherXRayMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	OutMeshes.Reset();

	if (!SupportsXRay())
	{
		return;
	}

	if (IsValid(XRayBodyMesh))
	{
		OutMeshes.Add(XRayBodyMesh);
	}
}

void ABaseCharacter::SyncXRayBodyMesh()
{
	if (!IsValid(XRayBodyMesh) || !IsValid(GetMesh()))
	{
		return;
	}

	XRayBodyMesh->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
	XRayBodyMesh->SetLeaderPoseComponent(GetMesh());
	XRayBodyMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	XRayBodyMesh->SetRelativeTransform(FTransform::Identity);
	XRayBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	XRayBodyMesh->SetGenerateOverlapEvents(false);
	XRayBodyMesh->SetCastShadow(false);
	XRayBodyMesh->SetRenderCustomDepth(false);
	XRayBodyMesh->SetRenderInMainPass(true);
	XRayBodyMesh->bReceivesDecals = false;
	XRayBodyMesh->SetVisibility(false, true);
	XRayBodyMesh->SetHiddenInGame(true, true);

	ApplyXRayMaterialToMesh(XRayBodyMesh);
	XRayBodyMesh->MarkRenderStateDirty();
}

void ABaseCharacter::ApplyXRayMaterialToMesh(UMeshComponent* MeshComponent) const
{
	if (!IsValid(MeshComponent) || !XRayMeshMaterial)
	{
		return;
	}

	const int32 MaterialCount = FMath::Max(1, MeshComponent->GetNumMaterials());
	const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent);
	const USkeletalMesh* SkeletalMeshAsset =
		SkeletalMeshComponent ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr;

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		UMaterialInterface* MaterialToApply = XRayMeshMaterial;

		if (SkeletalMeshAsset)
		{
			const TArray<FSkeletalMaterial>& SkeletalMaterials = SkeletalMeshAsset->GetMaterials();

			if (SkeletalMaterials.IsValidIndex(MaterialIndex))
			{
				const FSkeletalMaterial& SkeletalMaterial = SkeletalMaterials[MaterialIndex];

				const FName SlotName = SkeletalMaterial.MaterialSlotName;
				const UMaterialInterface* SourceMaterial = SkeletalMaterial.MaterialInterface;

				if (XRayHiddenMaterial && ShouldHideXRayMaterialSlot(SlotName, SourceMaterial))
				{
					MaterialToApply = XRayHiddenMaterial;
				}
			}
		}

		MeshComponent->SetMaterial(MaterialIndex, MaterialToApply);
	}
}

bool ABaseCharacter::HasToken(const FString& UpperName, const FString& Token) const
{
	TArray<FString> Tokens;
	FString NormalizedName = UpperName;

	NormalizedName.ReplaceInline(TEXT("-"), TEXT("_"));
	NormalizedName.ReplaceInline(TEXT("."), TEXT("_"));
	NormalizedName.ReplaceInline(TEXT(" "), TEXT("_"));

	NormalizedName.ParseIntoArray(Tokens, TEXT("_"), true);

	for (const FString& CurrentToken : Tokens)
	{
		if (CurrentToken == Token)
		{
			return true;
		}
	}

	return false;
}

bool ABaseCharacter::ShouldHideXRayMaterialSlot(const FName SlotName, const UMaterialInterface* SourceMaterial) const
{
	FString NameToCheck = SlotName.ToString();

	// 슬롯명만으로 구분이 어려운 경우를 대비해 실제 머티리얼 이름까지 함께 검사한다.
	if (SourceMaterial)
	{
		NameToCheck += TEXT("_");
		NameToCheck += SourceMaterial->GetName();
	}

	NameToCheck.ToUpperInline();

	// Contains("EYE")로 단순 처리하지 않은 이유는, 나중에 EYEBROW 같은 슬롯이 생기면 눈썹까지 숨겨질 수 있기 때문

	
	return
		HasToken(NameToCheck, TEXT("EYE")) ||
		HasToken(NameToCheck, TEXT("EYES")) ||
		// HasToken(NameToCheck, TEXT("CARUNCLE")) || // 눈 꺼풀
		HasToken(NameToCheck, TEXT("LASH")) ||     // 속 눈썹
		HasToken(NameToCheck, TEXT("LASHES")) ||
		HasToken(NameToCheck, TEXT("TEETH")) ||
		HasToken(NameToCheck, TEXT("TOOTH")) ||
		HasToken(NameToCheck, TEXT("TONGUE")) ||
		HasToken(NameToCheck, TEXT("MOUTH")) ||
		HasToken(NameToCheck, TEXT("GUM")) ||         // 잇몸
		HasToken(NameToCheck, TEXT("GUMS"));
	
}

void ABaseCharacter::SetCriticalWarningScale(float InScale)
{
	if (!VisionMarkerComponent || !VisionMarkerComponent->GetCriticalWidgetComponent()) return;

	const float ClampedScale = FMath::Clamp(InScale, 0.2f, 1.0f);

	if (UUserWidget* Widget = VisionMarkerComponent->GetCriticalWidgetComponent()->GetUserWidgetObject())
	{
		Widget->SetRenderScale(FVector2D(ClampedScale, ClampedScale));
	}
}

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, TeamId);

	// Anti-ESP Test 용
	DOREPLIFETIME(ThisClass, DebugReplicationPulse);

	// 가속도는 소유자가 이미 알고 있으므로 시뮬레이티드 프록시에만 보낸다.
	DOREPLIFETIME_CONDITION(ThisClass, ReplicatedAcceleration, COND_SimulatedOnly);
}

void ABaseCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// 서버에서 복제 직전에 현재 가속도를 담아둔다.
	if (const UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		ReplicatedAcceleration = MoveComp->GetCurrentAcceleration();
	}
}

void ABaseCharacter::OnRep_ReplicatedAcceleration()
{
	// 시뮬레이티드 프록시에서만 호출된다. 무브먼트가 SimulateMovement 에서 이 값을 보존한다.
	if (UAscenduntCharacterMovementComponent* MoveComp = Cast<UAscenduntCharacterMovementComponent>(GetCharacterMovement()))
	{
		MoveComp->SetReplicatedAcceleration(ReplicatedAcceleration);
	}
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server: when possessed, mirror PlayerState team into pawn TeamId (useful for AI perception + replicated fallback)
	if (!HasAuthority())
	{
		return;
	}

	if (const APlayerState* PS = GetPlayerState())
	{
		if (const IGenericTeamAgentInterface* PSTeam = Cast<IGenericTeamAgentInterface>(PS))
		{
			SetGenericTeamId(PSTeam->GetGenericTeamId());
		}
	}
}

void ABaseCharacter::OnRep_DebugReplicationPulse()
{
	if (!HasAuthority() && !IsLocallyControlled())
	{
		if (!GetName().Contains(TEXT("BP_Astel_C_0")))
		{
			return;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[RemoteActor] %s Loc=%s"),
			*GetName(),
			*GetActorLocation().ToCompactString());
	}
}

void ABaseCharacter::DebugIncreasePulse()
{
	++DebugReplicationPulse;
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Enforce presentation-only SkeletalMesh even when an existing Blueprint CDO
	// still carries the old AscPawnMesh collision settings. Gameplay hit queries
	// must go through UAscHitCollisionComponent only.
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkeletalMesh->SetGenerateOverlapEvents(false);
	}

	SyncXRayBodyMesh();
	
	if (false) // 복제 테스트 할 때, 키기
	{
		if (HasAuthority())
		{
			GetWorldTimerManager().SetTimer(
				DebugPulseTimerHandle,
				this,
				&ABaseCharacter::DebugIncreasePulse,
				1.f,
				true
			);
		}
	}
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
	DrawBodyAimDebug();
#endif
}

#if !UE_BUILD_SHIPPING
void ABaseCharacter::DrawBodyAimDebug() const
{
	if (AscBodyAimDebug::GEnabled == 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const FVector Origin = GetActorLocation() + FVector(0.f, 0.f, 20.f);
	const float ArrowLength = 250.f;

	// (1) 액터 forward.
	//
	// 원격 클라이언트가 이 캐릭터의 "방향"으로 받아볼 수 있는 값은 이것뿐이다.
	// 시뮬레이티드 프록시에는 컨트롤러가 없어서 APawn::GetBaseAimRotation() 도
	// 결국 액터 회전의 yaw 를 그대로 돌려준다. 별도의 조준 yaw 복제 채널은 존재하지 않는다.
	const FVector ActorFwd = GetActorForwardVector();

	// (2) root 본 forward.
	//
	// 반드시 컴포넌트 회전이 아니라 '본' 을 읽어야 한다. RotateRootBone 노드는 평가된 포즈
	// 안의 root 본만 회전시키고 컴포넌트 트랜스폼은 건드리지 않으므로,
	// MeshComp->GetComponentRotation() 으로는 RootYawOffset 이 절대 관측되지 않는다.
	FVector BodyFwd = ActorFwd;
	const int32 RootBoneIndex = MeshComp->GetBoneIndex(TEXT("root"));
	if (RootBoneIndex != INDEX_NONE)
	{
		// 메시는 액터 대비 yaw -90 으로 붙어 있다. 되돌려서 액터 공간 forward 와 비교 가능하게 만든다.
		FRotator RootRot = MeshComp->GetBoneTransform(RootBoneIndex).Rotator();
		RootRot.Yaw += 90.f;
		RootRot.Pitch = 0.f;
		RootRot.Roll = 0.f;
		BodyFwd = RootRot.Vector();
	}

	// (3) 로컬 플레이어 폰을 향하는 방향.
	//
	// 카메라 위치가 아니라 폰 위치를 쓴다. 3인칭 카메라가 캐릭터 뒤 220cm 에 있어서,
	// 근접 상황에서 카메라를 기준으로 잡으면 기준선 자체가 수십 도 틀어진다.
	FVector ToLocalPawn = FVector::ZeroVector;
	if (const APlayerController* LocalPC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr)
	{
		if (const APawn* LocalPawn = LocalPC->GetPawn())
		{
			if (LocalPawn != this)
			{
				ToLocalPawn = (LocalPawn->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
				ToLocalPawn = ToLocalPawn.GetSafeNormal();
			}
		}
	}

	DrawDebugDirectionalArrow(World, Origin, Origin + ActorFwd * ArrowLength,
		40.f, FColor::Green, false, -1.f, 0, 3.f);
	DrawDebugDirectionalArrow(World, Origin, Origin + BodyFwd * ArrowLength,
		40.f, FColor::Red, false, -1.f, 0, 3.f);

	// (4) 총구 forward.
	//
	// 초록->빨강 은 RotateRootBone 이 돌린 양(RootYawOffset), 빨강->노랑 은 그 위에 스파인이
	// 추가로 돌린 양(에임오프셋 + 리타깃된 포즈)이다. 라이라 설계대로면 이 둘은 부호가 반대여서
	// 서로 상쇄되어야 한다. 카메라를 돌렸을 때 두 값이 같은 방향으로 움직이면 상쇄가 아니라
	// 누적되고 있는 것이고, 그게 몸이 한쪽으로만 계속 틀어지는 원인이다.
	FVector MuzzleFwd = FVector::ZeroVector;
	if (const UWeaponEquipmentComponent* Equipment = FindComponentByClass<UWeaponEquipmentComponent>())
	{
		if (const AAscenduntWeapon* Weapon = Equipment->GetCurrentWeapon())
		{
			MuzzleFwd = Weapon->GetMuzzleTransform().GetRotation().GetForwardVector() * FVector(1.f, 1.f, 0.f);
			MuzzleFwd = MuzzleFwd.GetSafeNormal();
		}
	}

	float ActorVsLocalPawn = 0.f;
	if (!ToLocalPawn.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(World, Origin, Origin + ToLocalPawn * (ArrowLength * 0.7f),
			40.f, FColor::Magenta, false, -1.f, 0, 3.f);

		ActorVsLocalPawn = FMath::UnwindDegrees(ActorFwd.Rotation().Yaw - ToLocalPawn.Rotation().Yaw);
	}

	// 빨강이 초록에서 벌어진 각도가 곧 화면에 적용된 RootYawOffset 이다.
	const float BodyVsActor = FMath::UnwindDegrees(BodyFwd.Rotation().Yaw - ActorFwd.Rotation().Yaw);

	// 골반과 머리. 16도가 골반부터 들어가면 다리 포함 전신이 돌아간 것이고,
	// 골반은 0 인데 머리·총구만 돌아가 있으면 스파인 위쪽에서 들어간 것이다.
	// 수정 위치가 갈리는 지점이라 둘을 따로 잰다.
	FVector PelvisFwd = FVector::ZeroVector;
	FVector HeadFwd = FVector::ZeroVector;
	AscBodyAimDebug::GetBoneFacingFromRefPose(MeshComp, TEXT("pelvis"), ActorFwd, PelvisFwd);
	AscBodyAimDebug::GetBoneFacingFromRefPose(MeshComp, TEXT("head"), ActorFwd, HeadFwd);

	float PelvisVsActor = 0.f;
	if (!PelvisFwd.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(World, Origin + FVector(0, 0, 10.f),
			Origin + FVector(0, 0, 10.f) + PelvisFwd * ArrowLength,
			40.f, FColor::Cyan, false, -1.f, 0, 3.f);
		PelvisVsActor = FMath::UnwindDegrees(PelvisFwd.Rotation().Yaw - ActorFwd.Rotation().Yaw);
	}

	float HeadVsActor = 0.f;
	if (!HeadFwd.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(World, Origin + FVector(0, 0, 20.f),
			Origin + FVector(0, 0, 20.f) + HeadFwd * ArrowLength,
			40.f, FColor::White, false, -1.f, 0, 3.f);
		HeadVsActor = FMath::UnwindDegrees(HeadFwd.Rotation().Yaw - ActorFwd.Rotation().Yaw);
	}

	float MuzzleVsBody = 0.f;
	if (!MuzzleFwd.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(World, Origin, Origin + MuzzleFwd * ArrowLength,
			40.f, FColor::Yellow, false, -1.f, 0, 3.f);

		MuzzleVsBody = FMath::UnwindDegrees(MuzzleFwd.Rotation().Yaw - BodyFwd.Rotation().Yaw);
	}

	if (GEngine)
	{
		const TCHAR* RoleText = TEXT("?");
		switch (GetLocalRole())
		{
		case ROLE_Authority:        RoleText = TEXT("Authority");        break;
		case ROLE_AutonomousProxy:  RoleText = TEXT("AutonomousProxy");  break;
		case ROLE_SimulatedProxy:   RoleText = TEXT("SimulatedProxy");   break;
		default: break;
		}

		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()), 0.f, FColor::Yellow,
			FString::Printf(
				TEXT("[%s] %s  |  root-액터 = %6.1f  |  골반-액터 = %6.1f  |  머리-액터 = %6.1f  |  총구-액터 = %6.1f"),
				*GetName(), RoleText,
				BodyVsActor, PelvisVsActor, HeadVsActor, BodyVsActor + MuzzleVsBody));
	}
}
#endif

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}
