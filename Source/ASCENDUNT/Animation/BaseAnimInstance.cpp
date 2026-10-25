#include "BaseAnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

#include "ASCENDUNT/Character/AscenduntCharacterMovementComponent.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BaseAnimInstance)

UBaseAnimInstance::UBaseAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBaseAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	// 태그 프로퍼티 맵을 시스템에 등록하여 자동 변수 업데이트 활성화
	GameplayTagPropertyMap.Initialize(this, ASC);
}

#if WITH_EDITOR
EDataValidationResult UBaseAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	// 에디터 툴 단에서 잘못된 태그가 매핑되어 있지 않은지 유효성 검사
	GameplayTagPropertyMap.IsDataValid(this, Context);

	return ((Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // WITH_EDITOR
																													
void UBaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 캐릭터가 생성될 때 액터로부터 어빌리티 시스템 컴포넌트(ASC)를 자동으로 찾아와 연동합니다.
	if (AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
			bIsTagMapInitialized = true;
		}
	}
}

void UBaseAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// PlayerState에 있는 ASC는 초기화 타이밍이 늦을 수 있으므로 Update에서 폴링으로 연결을 시도합니다.
	if (!bIsTagMapInitialized)
	{
		if (AActor* OwningActor = GetOwningActor())
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
			{
				InitializeWithAbilitySystem(ASC);
				bIsTagMapInitialized = true;
			}
		}
	}

	// 양손 파지 IK 데이터 갱신.
	// 소켓 조회가 스레드 세이프하지 않으므로 반드시 이 게임 스레드 경로에서만 수행한다.
	UpdateWeaponGripData();

	const ACharacter* Character = Cast<ACharacter>(GetOwningActor());
	if (!Character)
	{
		return;
	}

	UpdateGroundDistance(Character);
}

void UBaseAnimInstance::UpdateGroundDistance(const ACharacter* Character)
{
	// 값을 못 구하는 경우에도 음수를 남기면 안 된다.
	// "GroundDistance < 200" 형태의 전이 조건이 항상 참이 되어 낙하 루프가 즉시 끊긴다.
	GroundDistance = GroundDistanceFallback;

	if (!Character)
	{
		return;
	}

	// 이 값의 소비자는 애니메이션뿐이므로 데디케이티드 서버에서는 트레이스를 아낀다.
	if (bSkipGroundTraceOnDedicatedServer)
	{
		const UWorld* World = GetWorld();
		if (World && World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
	}

	// 계산은 무브먼트 컴포넌트가 한다. 여기서는 값을 옮기기만 한다.
	// 라이라도 ULyraAnimInstance가 GetGroundInfo()의 결과를 받아 적기만 한다.
	//
	// CastChecked를 쓰지 않는 이유: 이 애님 인스턴스가 ABaseCharacter 계열이 아닌 폰
	// (툴, 프리뷰, 외부에서 가져온 테스트 폰)에 붙어도 크래시하면 안 된다.
	if (UAscenduntCharacterMovementComponent* MoveComp = Cast<UAscenduntCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		GroundDistance = MoveComp->GetGroundInfo().GroundDistance;
	}
}

void UBaseAnimInstance::UpdateWeaponGripData()
{
	// 이번 프레임에 값을 못 구하면 반드시 false로 남아야 한다.
	// 그렇지 않으면 무기를 놓거나 죽은 뒤에도 왼손이 마지막 좌표를 계속 붙잡는다.
	bHasLeftHandGripTarget = false;

	if (bSkipGripUpdateOnDedicatedServer)
	{
		const UWorld* World = GetWorld();
		if (World && World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
	}

	USkeletalMeshComponent* OwnerMesh = GetOwningComponent();
	APawn* OwnerPawn = TryGetPawnOwner();

	// 애님 BP 프리뷰 뷰포트에는 폰이 없다. 정상 경로이므로 경고하지 않는다.
	if (!OwnerMesh || !OwnerPawn)
	{
		return;
	}

	// 폰이 바뀌면 캐시를 버린다. 캐릭터 교체나 리스폰 시 옛 컴포넌트를 붙들지 않게 한다.
	if (CachedGripPawn.Get() != OwnerPawn)
	{
		CachedGripPawn = OwnerPawn;
		CachedEquipment.Reset();
		bEquipmentSearchDone = false;
		bLoggedMissingReferenceBone = false;
	}

	if (!bEquipmentSearchDone)
	{
		bEquipmentSearchDone = true;
		CachedEquipment = OwnerPawn->FindComponentByClass<UWeaponEquipmentComponent>();
	}

	UWeaponEquipmentComponent* Equipment = CachedEquipment.Get();
	if (!Equipment)
	{
		// 무기 시스템이 없는 폰. 정상 상태다.
		return;
	}

	AAscenduntWeapon* Weapon = Equipment->GetCurrentWeapon();
	if (!Weapon)
	{
		// 무기 미장착. 리스폰 직후나 캐릭터 선택 중에는 정상 상태다.
		CachedGripWeapon.Reset();
		CachedGripMesh.Reset();
		bGripMeshResolved = false;
		return;
	}

	// 파지 메시 탐색은 무기가 바뀔 때만 수행한다.
	// 소켓 조회와 컴포넌트 순회를 매 프레임 반복하지 않기 위함이다.
	if (!bGripMeshResolved || CachedGripWeapon.Get() != Weapon)
	{
		CachedGripWeapon = Weapon;
		CachedGripMesh = ResolveGripMesh(Weapon);
		bGripMeshResolved = true;
	}

	USceneComponent* GripMesh = CachedGripMesh.Get();
	if (!GripMesh)
	{
		// 파지 소켓이 없는 무기. 양손 파지가 아니거나 메시가 없는 무기에서는 정상이다.
		return;
	}

	FVector Target = FVector::ZeroVector;
	if (ComputeLeftHandGripTarget(OwnerMesh, GripMesh, Target))
	{
		LeftHandGripTarget = Target;
		bHasLeftHandGripTarget = true;
	}
}

USceneComponent* UBaseAnimInstance::ResolveGripMesh(AAscenduntWeapon* Weapon) const
{
	if (!Weapon)
	{
		return nullptr;
	}

	// Active Weapon Mesh는 Skeletal/Static 중 실제 에셋이 지정된 컴포넌트다.
	// DoesSocketExist는 USceneComponent 레벨에서 처리되므로 양쪽 모두 동일하게 사용할 수 있다.
	USceneComponent* GripMesh = Weapon->GetActiveWeaponMesh();
	if (GripMesh && GripMesh->DoesSocketExist(GripSocketName))
	{
		return GripMesh;
	}

	return nullptr;
}

bool UBaseAnimInstance::ComputeLeftHandGripTarget(USkeletalMeshComponent* OwnerMesh, const USceneComponent* GripMesh, FVector& OutTarget)
{
	if (!OwnerMesh || !GripMesh)
	{
		return false;
	}

	// 기준 본이 없으면 GetSocketTransform이 조용히 항등 트랜스폼을 반환해
	// 완전히 엉뚱한 좌표가 나온다. 반드시 먼저 검사한다.
	if (!OwnerMesh->DoesSocketExist(ReferenceBoneName))
	{
		if (!bLoggedMissingReferenceBone)
		{
			bLoggedMissingReferenceBone = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[AnimGrip] ReferenceBone '%s' not found. Mesh=%s"),
				*ReferenceBoneName.ToString(),
				*GetNameSafe(OwnerMesh->GetSkeletalMeshAsset()));
		}
		return false;
	}

	const FVector GripWorld = GripMesh->GetSocketLocation(GripSocketName);
	const FTransform RefBoneWorld = OwnerMesh->GetSocketTransform(ReferenceBoneName, RTS_World);

	OutTarget = RefBoneWorld.InverseTransformPosition(GripWorld);
	return true;
}
