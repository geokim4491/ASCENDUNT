// Fill out your copyright notice in the Description page of Project Settings.


#include "HeroFirstPersonComponent.h"

#include "ASCENDUNT/Animation/FirstPerson/AscFPAnimInstance.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode_FirstPerson.h"
#include "ASCENDUNT/Character/Hero/HeroCameraComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

#if !UE_BUILD_SHIPPING
// Mesh1P 위치 실시간 조정용. BP 의 Relative Location 에 더해진다.
// 팔과 총을 뜯어보고 싶을 때 X 를 크게 줘서 리그 전체를 카메라 앞으로 밀어낼 수 있다.
//   asc.FP.Mesh.X 60      전방(+) / 후방(-)
//   asc.FP.Mesh.Z 10      상단(+) / 하단(-)
//   asc.FP.Mesh.Dump 1    현재 최종 Relative Location 을 로그로 출력
static float GAscFPMeshX = 0.f;
static float GAscFPMeshY = 0.f;
static float GAscFPMeshZ = 0.f;
static int32 GAscFPMeshDump = 0;

static FAutoConsoleVariableRef CVarAscFPMeshX(TEXT("asc.FP.Mesh.X"), GAscFPMeshX, TEXT("Mesh1P 전방 오프셋(cm)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPMeshY(TEXT("asc.FP.Mesh.Y"), GAscFPMeshY, TEXT("Mesh1P 우측 오프셋(cm)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPMeshZ(TEXT("asc.FP.Mesh.Z"), GAscFPMeshZ, TEXT("Mesh1P 상단 오프셋(cm)."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPMeshDump(TEXT("asc.FP.Mesh.Dump"), GAscFPMeshDump, TEXT("1이면 Mesh1P 최종 Relative Location 을 로그로 출력."), ECVF_Cheat);
#endif

UHeroFirstPersonComponent::UHeroFirstPersonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UHeroFirstPersonComponent::BeginPlay()
{
	Super::BeginPlay();

	// 데디케이티드 서버는 렌더링이 없으므로 조정할 대상 자체가 없다.
	//
	// 반대로 클라이언트에서는 원격 폰이라도 틱을 끄지 않는다. 로컬 조종 여부가 확정되는
	// 시점(PossessedBy / OnRep_Controller / OnRep_PlayerState)은 경로마다 다르고, 그중
	// 하나를 훅으로 잡지 못하면 1인칭이 영영 켜지지 않는다. 그건 이 컴포넌트가 없애려는
	// 실패 방식 그 자체이므로, 판정은 EvaluateDesiredFirstPerson 안에서만 한다.
	// 원격 폰의 비용은 조기 반환 몇 줄이다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
	}
}

void UHeroFirstPersonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AHeroCharacter* Hero = GetHeroCharacter();
	if (!Hero)
	{
		return;
	}

	const bool bFirstPerson = EvaluateDesiredFirstPerson();

	// 부착은 매 프레임, 렌더 상태는 값이 바뀔 때만.
	//
	// 부착 성공 조건(Mesh1P 메시 / 무기 액터 / 소켓)이 갖춰지는 시점은 모드 전환 시점과
	// 무관하다. 변경 감지 안에 두면 전환 순간 한 번 시도하고 실패하면 영영 재시도하지 않는다.
	ReconcileWeaponAttachment(Hero, bFirstPerson);
	ApplyViewMode(Hero, bFirstPerson);

#if !UE_BUILD_SHIPPING
	ApplyDebugMeshOffset(Hero);
#endif
}

AHeroCharacter* UHeroFirstPersonComponent::GetHeroCharacter() const
{
	return Cast<AHeroCharacter>(GetOwner());
}

AAscenduntWeapon* UHeroFirstPersonComponent::GetCurrentWeapon(const AHeroCharacter* Hero)
{
	const UWeaponEquipmentComponent* Equipment = Hero ? Hero->GetWeaponEquipmentComponent() : nullptr;
	return Equipment ? Equipment->GetCurrentWeapon() : nullptr;
}

bool UHeroFirstPersonComponent::EvaluateDesiredFirstPerson() const
{
	const AHeroCharacter* Hero = GetHeroCharacter();
	if (!Hero || !Hero->IsGameplayReady() || !Hero->IsLocallyControlled())
	{
		return false;
	}

	// 래그돌로 넘어간 뒤에는 카메라가 3인칭으로 빠지기 전이라도 1인칭을 유지할 이유가 없다.
	if (Hero->IsDeathRagdollActive())
	{
		return false;
	}

	// 1P 애셋을 아직 지정하지 않은 히어로는 조용히 3인칭으로 동작한다.
	// GetAnimClass() 가 non-const 라 const 포인터로 받을 수 없다.
	USkeletalMeshComponent* Mesh1P = Hero->GetMesh1P();
	if (!Mesh1P || !Mesh1P->GetSkeletalMeshAsset() || !Mesh1P->GetAnimClass())
	{
		return false;
	}

	const UHeroCameraComponent* HeroCamera = Hero->GetHeroCameraComponent();
	if (!HeroCamera)
	{
		return false;
	}

	// DetermineCameraMode 가 Downed > Ability > PawnData 우선순위를 이미 계산한다.
	// 여기서 Downed 나 스킬 상태를 다시 검사하면 판정이 두 군데로 갈라진다.
	const TSubclassOf<UAscenduntCameraMode> CameraMode = HeroCamera->DetermineCameraMode();
	return CameraMode && CameraMode->IsChildOf(UAscenduntCameraMode_FirstPerson::StaticClass());
}

void UHeroFirstPersonComponent::ApplyViewMode(AHeroCharacter* Hero, bool bFirstPerson)
{
	AAscenduntWeapon* CurrentWeapon = GetCurrentWeapon(Hero);

	// 무기도 캐시 키에 넣는다. 새 무기는 렌더 타입이 None 인 채로 스폰되므로,
	// 모드가 그대로여도 다시 적용해야 1인칭에서 총이 두 개 보이지 않는다.
	if (bHasAppliedOnce && bFirstPerson == bAppliedFirstPerson && CurrentWeapon == AppliedWeapon.Get())
	{
		return;
	}

	AAscenduntWeapon* PreviousWeapon = AppliedWeapon.Get();

	bHasAppliedOnce = true;
	bAppliedFirstPerson = bFirstPerson;
	AppliedWeapon = CurrentWeapon;

	ApplyMesh1PVisibility(Hero, bFirstPerson);

	// 3P 몸통. WorldSpaceRepresentation 이면 씬 프록시가 bOwnerNoSee 와 bCastHiddenShadow 를
	// 강제하므로(PrimitiveSceneProxy) 그 둘을 여기서 직접 만지지 않는다.
	// 직접 만지면 None 으로 되돌릴 때 상태가 샌다.
	//
	// 이 메시는 1인칭에서도 계속 그림자를 만든다. 로컬 플레이어에게는 이 그림자가 자기 몸의
	// 유일한 시각적 표현이므로, 3P 몽타주 재생과 포즈 틱을 줄이면 안 된다.
	if (USkeletalMeshComponent* Mesh3P = Hero->GetMesh())
	{
		Mesh3P->SetFirstPersonPrimitiveType(bFirstPerson
			? EFirstPersonPrimitiveType::WorldSpaceRepresentation
			: EFirstPersonPrimitiveType::None);
	}

	// 몸통 X-Ray. LeaderPoseComponent 로 포즈만 따라갈 뿐 렌더 플래그는 상속하지 않으므로,
	// 처리하지 않으면 1인칭 화면 한가운데에 자기 몸 실루엣이 남는다.
	//
	// WorldSpaceRepresentation 대신 bOwnerNoSee 를 쓴다. X-Ray 메시는 1인칭 메시의 월드
	// 표현이 아니라 오버레이라서 의미상 "오너에게만 숨긴다" 가 맞다.
	// 프로젝트 어디에서도 bOwnerNoSee 를 쓰지 않으므로 false 가 원래 값이다.
	//
	// 무기 쪽 X-Ray 는 SetWeaponFirstPersonState 가 담당한다. 여기서 Hero->GatherXRayMeshes()
	// 를 쓰면 현재 무기 것이 중복으로 잡힌다.
	if (UMeshComponent* XRayBody = Hero->GetXRayBodyMesh())
	{
		XRayBody->SetOwnerNoSee(bFirstPerson);
	}

	// 이전 무기는 항상 원복한다. 교체 직후에는 숨겨져 있지만 다시 꺼낼 때를 대비해야 한다.
	if (PreviousWeapon && PreviousWeapon != CurrentWeapon)
	{
		SetWeaponFirstPersonState(PreviousWeapon, false);
	}
	SetWeaponFirstPersonState(CurrentWeapon, bFirstPerson);
}

void UHeroFirstPersonComponent::ApplyMesh1PVisibility(AHeroCharacter* Hero, bool bFirstPerson)
{
	USkeletalMeshComponent* Mesh1P = Hero ? Hero->GetMesh1P() : nullptr;
	if (!Mesh1P)
	{
		return;
	}

	Mesh1P->SetVisibility(bFirstPerson, /*bPropagateToChildren*/ true);

	// 1P 팔은 X-Ray 표시 대상이 아니라, 화면 앞의 X-Ray를 가리는 로컬 Blocker다.
	// 1P 가시성에 맞춰 CustomDepth도 이 컴포넌트에서 관리한다.
	Mesh1P->SetRenderCustomDepth(bFirstPerson);

	if (!Mesh1P->GetSkeletalMeshAsset())
	{
		return;
	}

	// 본 숨김: 스켈레톤 기준. 머리 내부가 화면을 가리고 다리가 카메라 피치를 따라 도는 걸 막는다.
	// 스켈레탈 메시가 바뀌면 숨김이 초기화되므로 진입할 때마다 다시 적용한다.
	for (const FName& BoneName : HiddenBones1P)
	{
		if (BoneName == NAME_None || Mesh1P->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			continue;
		}

		if (bFirstPerson)
		{
			Mesh1P->HideBoneByName(BoneName, PBO_None);
		}
		else
		{
			Mesh1P->UnHideBoneByName(BoneName);
		}
	}
}

void UHeroFirstPersonComponent::SetWeaponFirstPersonState(AAscenduntWeapon* Weapon, bool bFirstPerson)
{
	if (!IsValid(Weapon))
	{
		return;
	}

	const EFirstPersonPrimitiveType BodyType = bFirstPerson
		? EFirstPersonPrimitiveType::WorldSpaceRepresentation
		: EFirstPersonPrimitiveType::None;

	// 3P 무기 메시를 빠뜨리면 몸통만 사라지고 총은 3인칭 위치에 남아, 1P 팔에 든 총과 합쳐져
	// 총이 두 개 보인다. 스켈레탈과 스태틱 둘 다 존재하므로 양쪽을 모두 처리한다.
	if (USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMesh())
	{
		WeaponMesh->SetFirstPersonPrimitiveType(BodyType);
	}

	if (UStaticMeshComponent* StaticWeaponMesh = Weapon->GetStaticWeaponMesh())
	{
		StaticWeaponMesh->SetFirstPersonPrimitiveType(BodyType);
	}

	// 1P 무기도 팔과 동일하게 X-Ray Blocker로만 CustomDepth에 기록한다.
	// 실제 가시성/부착은 ReconcileWeaponAttachment가 별도로 관리한다.
	if (USkeletalMeshComponent* WeaponMesh1P = Weapon->GetWeaponMesh1P())
	{
		WeaponMesh1P->SetRenderCustomDepth(bFirstPerson);
	}

	TArray<UMeshComponent*> WeaponXRayMeshes;
	Weapon->GatherXRayMeshes(WeaponXRayMeshes);
	for (UMeshComponent* XRayMesh : WeaponXRayMeshes)
	{
		if (IsValid(XRayMesh))
		{
			XRayMesh->SetOwnerNoSee(bFirstPerson);
		}
	}
}

void UHeroFirstPersonComponent::ReconcileWeaponAttachment(AHeroCharacter* Hero, bool bFirstPerson)
{
	AAscenduntWeapon* Weapon = GetCurrentWeapon(Hero);
	if (!Hero || !IsValid(Weapon))
	{
		return;
	}

	USkeletalMeshComponent* WeaponMesh1P = Weapon->GetWeaponMesh1P();
	if (!WeaponMesh1P)
	{
		return;
	}

	USkeletalMeshComponent* Mesh1P = Hero->GetMesh1P();
	const UWeaponEquipmentComponent* Equipment = Hero->GetWeaponEquipmentComponent();
	const FAscenduntWeaponSlot* Slot = Equipment ? Equipment->GetSlotForWeapon(Weapon) : nullptr;

	// EquippedSocket1P 가 NAME_None 이면 "무기가 1P 메시에 이미 포함되어 있다" 는 뜻이다.
	const bool bCanAttach =
		bFirstPerson &&
		Mesh1P &&
		Slot &&
		Slot->EquippedSocket1P != NAME_None &&
		Mesh1P->DoesSocketExist(Slot->EquippedSocket1P);

	if (!bCanAttach)
	{
		WeaponMesh1P->SetRenderCustomDepth(false);

		// 소켓 이름이 틀렸을 때만 알린다. 3인칭이거나 의도적으로 끈 경우는 정상 경로다.
		if (bFirstPerson && Mesh1P && Slot && Slot->EquippedSocket1P != NAME_None)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[FirstPerson] Mesh1P has no socket %s. 1P weapon not attached. Hero=%s Weapon=%s"),
				*Slot->EquippedSocket1P.ToString(), *GetNameSafe(Hero), *GetNameSafe(Weapon));
		}

		WeaponMesh1P->SetVisibility(false);
		return;
	}

	// 부모와 소켓을 비교해 어긋났을 때만 재부착한다. 서버 복제가 무기 액터 루트를 다시
	// 붙여도 다음 프레임에 여기서 복구되므로 복제와 경합하지 않는다.
	if (WeaponMesh1P->GetAttachParent() != Mesh1P ||
		WeaponMesh1P->GetAttachSocketName() != Slot->EquippedSocket1P)
	{
		WeaponMesh1P->AttachToComponent(
			Mesh1P,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Slot->EquippedSocket1P);
	}

	WeaponMesh1P->SetRelativeLocation(Slot->WeaponBoneOffset1P);
	WeaponMesh1P->SetVisibility(true);
	WeaponMesh1P->SetRenderCustomDepth(true);
}

float UHeroFirstPersonComponent::PlayMontage1P(UAnimMontage* Montage, float PlayRate, FName StartSection)
{
	// 1인칭이 아니면 Mesh1P 가 숨겨져 있어 재생해도 보이지 않는다.
	// 스킬이 잠깐 3인칭으로 전환한 사이의 사격까지 큐에 쌓아둘 이유가 없다.
	if (!Montage || !bAppliedFirstPerson)
	{
		return 0.0f;
	}

	const AHeroCharacter* Hero = GetHeroCharacter();
	USkeletalMeshComponent* Mesh1P = Hero ? Hero->GetMesh1P() : nullptr;
	UAnimInstance* AnimInstance = Mesh1P ? Mesh1P->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return 0.0f;
	}

	const float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
	if (Duration > 0.0f && StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}
	return Duration;
}

#if !UE_BUILD_SHIPPING
void UHeroFirstPersonComponent::ApplyDebugMeshOffset(AHeroCharacter* Hero)
{
	USkeletalMeshComponent* Mesh1P = Hero ? Hero->GetMesh1P() : nullptr;
	if (!Mesh1P)
	{
		return;
	}

	// BP 값을 기준으로 삼아야 콘솔 오프셋이 누적되지 않는다.
	if (!bCachedBaseMeshLocation)
	{
		BaseMesh1PRelativeLocation = Mesh1P->GetRelativeLocation();
		bCachedBaseMeshLocation = true;
	}

	const FVector Desired = BaseMesh1PRelativeLocation + FVector(GAscFPMeshX, GAscFPMeshY, GAscFPMeshZ);
	if (!Mesh1P->GetRelativeLocation().Equals(Desired))
	{
		Mesh1P->SetRelativeLocation(Desired);
	}

	if (GAscFPMeshDump != 0)
	{
		GAscFPMeshDump = 0;
		UE_LOG(LogTemp, Warning,
			TEXT("[FP-MESH] Mesh1P Relative Location = (%.1f, %.1f, %.1f)"),
			Desired.X, Desired.Y, Desired.Z);
	}
}
#endif

void UHeroFirstPersonComponent::NotifyFired1P()
{
	if (!bAppliedFirstPerson)
	{
		return;
	}

	const AHeroCharacter* Hero = GetHeroCharacter();
	USkeletalMeshComponent* Mesh1P = Hero ? Hero->GetMesh1P() : nullptr;
	UAnimInstance* AnimInstance = Mesh1P ? Mesh1P->GetAnimInstance() : nullptr;

	if (UAscFPAnimInstance* FPAnim = Cast<UAscFPAnimInstance>(AnimInstance))
	{
		FPAnim->NotifyFired();
	}
}
