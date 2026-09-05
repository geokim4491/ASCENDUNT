// Fill out your copyright notice in the Description page of Project Settings.

#include "BaseAICharacter.h"
#include "ASCENDUNT/Character/HitCollision/AscHitCollisionComponent.h"

#include "Net/UnrealNetwork.h"
#include "AIInitComponent.h"
#include "ASCENDUNT/AI/BaseAIController.h"
#include "AIPawnData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySet.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntCharacterStatSet.h"
#include "GameplayEffectTypes.h"
#include "Components/CapsuleComponent.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/System/AIPoolSubsystem.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

ABaseAICharacter::ABaseAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;

	// AI는 Tick을 쓰지 않는다. ABaseCharacter가 켜둔 것을 여기서 끈다.
	// (CharacterMovement / SkeletalMesh 는 각자 컴포넌트 틱으로 돌므로 영향 없다)
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = ABaseAIController::StaticClass();

	// 스폰된 AI는 스포너가 SetPawnData() 이후에 명시적으로 컨트롤러를 붙인다.
	// PlacedInWorldOrSpawned로 두면 SpawnActor 도중에 PossessedBy가 터져서
	// PawnData가 주입되기 전에 초기화가 GameplayReady까지 진행되어 버린다.
	// 레벨에 직접 배치한 AI는 자기 BP 기본 PawnData를 쓰므로 자동 빙의를 유지한다.
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;

	AIInitComponent = CreateDefaultSubobject<UAIInitComponent>(TEXT("AIInitComponent"));

	ASC = CreateDefaultSubobject<UAscenduntAbilitySystemComponent>(TEXT("ASC"));
	ASC->SetIsReplicated(true);
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthComponent = CreateDefaultSubobject<UAscenduntHealthComponent>(TEXT("HealthComponent"));
	WeaponEquipmentComponent = CreateDefaultSubobject<UWeaponEquipmentComponent>(TEXT("WeaponEquipmentComponent"));

	SetGenericTeamId(FGenericTeamId(99));
}

UAbilitySystemComponent* ABaseAICharacter::GetAbilitySystemComponent() const
{
	return ASC;
}

void ABaseAICharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseAICharacter, bPooledActive);
}

void ABaseAICharacter::ApplyPooledActiveState(bool bActive)
{
	// AActor::bHidden은 복제되지만 bActorEnableCollision은 복제되지 않는다(Actor.h).
	// 그래서 이 처리는 서버와 클라이언트가 각자 수행해야 한다.
	// 숨기는 것을 먼저 한다. 몽타주를 먼저 끊으면 그 프레임에 idle 포즈가 노출될 수 있다.
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	SetActorTickEnabled(bActive);

	if (!bActive)
	{
		// 사망 몽타주 상태 정리.
		// ResetForPool()은 서버에서만 실행되므로 여기서 해야 클라이언트에도 적용된다.
		// 액터가 파괴되지 않아 AnimInstance가 유지되기 때문에, 정리하지 않으면
		// 다음 생애에 사망 포즈로 되살아난다.
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
			{
				Anim->StopAllMontages(0.f);
			}
		}
	}

	// 사망 중에 꺼둔 상호작용 충돌을 여기서 되돌린다.
	SetInteractionCollisionEnabled(bActive);

	// 무기는 폰과 별개 액터라 폰의 틱/복제 설정이 전파되지 않는다.
	// 숨김은 ServerReleaseWeaponsForPool / ServerEnsureDefaultWeapons 가 담당하고,
	// 여기서는 틱과 복제만 다룬다.
	if (WeaponEquipmentComponent)
	{
		for (AAscenduntWeapon* Weapon : WeaponEquipmentComponent->GetWeaponInventory())
		{
			if (!IsValid(Weapon))
			{
				continue;
			}

			Weapon->SetActorTickEnabled(bActive);

			if (HasAuthority())
			{
				if (bActive)
				{
					Weapon->SetNetDormancy(DORM_Awake);
					Weapon->FlushNetDormancy();
				}
				else
				{
					Weapon->SetNetDormancy(DORM_DormantAll);
				}
			}
		}
	}

	OnPooledActiveStateChanged.Broadcast(this, bActive);
}

void ABaseAICharacter::SetInteractionCollisionEnabled(bool bEnabled)
{
	// 캡슐(AscPawnCapsule): QueryAndPhysics, ObjectType=Pawn.
	// 이걸 꺼야 플레이어가 시체를 통과할 수 있다.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(
			bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	// Gameplay hit detection no longer uses SkeletalMesh/PhysicsAsset.
	// Toggle only the editor-authored hit collision primitives.
	UAscHitCollisionComponent::SetHitCollisionsEnabled(this, bEnabled);
}

void ABaseAICharacter::OnDeathStarted(AActor* OwningActor)
{
	// 서버/클라 양쪽에서 호출된다(서버: StartDeath, 클라: OnRep_DeathState).

	// 사망 몽타주가 재생되는 동안 시체가 길을 막거나 총알을 먹으면 안 된다.
	SetInteractionCollisionEnabled(false);

	// 죽은 폰이 관성으로 미끄러지지 않도록. 클라이언트 시뮬레이션도 함께 멈춘다.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	// AI 로직 정지. 내부에 서버 가드가 있다.
	if (ABaseAIController* AIC = Cast<ABaseAIController>(GetController()))
	{
		AIC->SuspendLogic();
	}

	// 여기부터는 사망 완료 시점을 결정하는 부분이라 서버만 수행한다.
	if (!HasAuthority())
	{
		return;
	}

	// 사망 연출을 맡을 어빌리티에게 기회를 준다.
	// HandleGameplayEvent는 동기 호출이고 "실제로 트리거된 어빌리티 개수"를 반환하므로,
	// 아무도 맡지 않았다는 사실을 그 자리에서 확정할 수 있다.
	int32 HandledCount = 0;
	if (ASC)
	{
		FGameplayEventData EventData;
		EventData.EventTag = AscenduntGameplayTags::Event_Death;
		EventData.Instigator = this;
		EventData.Target = this;

		HandledCount = ASC->HandleGameplayEvent(AscenduntGameplayTags::Event_Death, &EventData);
	}

	if (HandledCount <= 0)
	{
		// 사망 연출 소유자가 없다. 연출 없이 즉시 완료한다.
		// 여기서 멈추면 폰이 풀로 돌아오지 못하고 스포너의 리스폰도 정지한다.
		UE_LOG(LogTemp, Warning,
			TEXT("[AI] %s: 사망 어빌리티가 트리거되지 않아 연출 없이 사망을 완료합니다. "
				 "AbilitySet에 사망 어빌리티가 등록되어 있는지, "
				 "그리고 BP에서 Event.Death를 중복 발송하고 있지 않은지 확인하세요."),
			*GetName());

		FinishDeathDeferred();
	}
}

void ABaseAICharacter::FinishDeathDeferred()
{
	if (!HasAuthority())
	{
		return;
	}

	// 이 함수는 애니메이션 업데이트 콜스택 안에서 불릴 수 있다.
	// 거기서 FinishDeath까지 이어지면 ResetForPool이 같은 AnimInstance의 몽타주를
	// 정지시키고 메시 틱까지 꺼서 애님 시스템에 재진입하게 된다.
	// 다음 틱으로 미뤄 콜스택을 빠져나간 뒤 처리한다.
	GetWorldTimerManager().SetTimerForNextTick(this, &ABaseAICharacter::FinishDeathNow);
}

void ABaseAICharacter::FinishDeathNow()
{
	if (HasAuthority() && HealthComponent)
	{
		// 중복 호출은 FinishDeath() 내부 가드가 막는다.
		HealthComponent->FinishDeath();
	}
}

void ABaseAICharacter::OnRep_PooledActive()
{
	ApplyPooledActiveState(bPooledActive);
}

void ABaseAICharacter::SetPooledActive(bool bActive)
{
	if (HasAuthority())
	{
		bPooledActive = bActive;
	}

	ApplyPooledActiveState(bActive);
}

void ABaseAICharacter::InitializeFromPawnData()
{
	if (!PawnData) return;

	// 외형만 담당한다. 같은 값으로 여러 번 호출되어도 무해하다.
	// GAS 부여는 UAIInitComponent가 소유한다(GrantPawnDataAbilities 참고).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSkeletalMesh(PawnData->EnemyMesh);

		if (GetNetMode() == NM_DedicatedServer)
		{
			// 서버 AI의 HitCollision은 Root 기준이므로 Bone/Pose가 Gameplay에 필요하지 않다.
			// Mesh asset 자체는 weapon_r 같은 기존 socket attachment 호환을 위해 유지하되,
			// AnimInstance와 Component Tick을 제거해 TickAnimation / RefreshBoneTransforms를 막는다.
			MeshComp->SetAnimInstanceClass(nullptr);
			MeshComp->SetComponentTickEnabled(false);
		}
		else
		{
			MeshComp->SetAnimInstanceClass(PawnData->AnimClass);
			MeshComp->SetComponentTickEnabled(true);
		}
	}

	DisableDedicatedServerAnimation();
}

void ABaseAICharacter::DisableDedicatedServerAnimation()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	// AI Actor에 붙은 SkeletalMesh를 함께 끈다.
	// AI에는 XRayBodyMesh를 생성하지 않으며, 남은 SkeletalMesh도 서버 Gameplay Collision/Hit Detection에 사용되지 않는다.
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
}

void ABaseAICharacter::PlayPresentationMontageLocal(UAnimMontage* Montage, float PlayRate, FName StartSection)
{
	if (GetNetMode() == NM_DedicatedServer || !Montage)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}

	Anim->Montage_Play(Montage, FMath::Max(0.01f, PlayRate));
	if (!StartSection.IsNone())
	{
		Anim->Montage_JumpToSection(StartSection, Montage);
	}
}

void ABaseAICharacter::Multicast_PlayPresentationMontage_Implementation(UAnimMontage* Montage, float PlayRate, FName StartSection)
{
	PlayPresentationMontageLocal(Montage, PlayRate, StartSection);
}

void ABaseAICharacter::Multicast_PlayDeathPresentationMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	PlayPresentationMontageLocal(Montage, PlayRate, NAME_None);
}

void ABaseAICharacter::GrantPawnDataAbilities()
{
	if (!HasAuthority() || !ASC || !PawnData || !PawnData->AbilitySet)
	{
		return;
	}

	// AttributeSet 등록, GA 부여, 초기화 GE 적용을 한 번에 처리한다.
	PawnData->AbilitySet->GiveToAbilitySystem(ASC, &AbilitySetHandles, this);
}

void ABaseAICharacter::RevokePawnDataAbilities()
{
	if (!HasAuthority() || !ASC)
	{
		return;
	}

	AbilitySetHandles.TakeFromAbilitySystem(ASC);
}

void ABaseAICharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 1. AIController가 빙의(Possess)할 때 데이터 에셋을 읽어 캐릭터 세팅
	InitializeFromPawnData();

	// 2. 서버: InitComponent로 위임
	// BT 시작은 여기서 하지 않는다. UAIInitComponent가 GameplayReady에 도달했을 때
	// TryStartBTFromReady()로 시작하는 것이 유일한 경로다.
	// 여기서 먼저 켜면 무기 스폰 전에 공격 Task가 실행될 수 있다.
	if (AIInitComponent)
	{
		AIInitComponent->NotifyPossessed();
	}
}

void ABaseAICharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// 클라: 복제 타이밍 흡수
	if (AIInitComponent)
	{
		AIInitComponent->NotifyControllerRep();
	}
}

void ABaseAICharacter::PrepareForPoolReuse(const FTransform& SpawnTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	// 풀에 있는 동안의 이동 상태/MovementBase를 다음 생애로 가져가지 않는다.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->ClearAccumulatedForces();
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}
	SetBase(nullptr);

	FVector SafeLocation = SpawnTransform.GetLocation();
	FRotator SafeRotation = SpawnTransform.Rotator();
	SafeRotation.Pitch = 0.f;
	SafeRotation.Roll = 0.f;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (Capsule && World)
	{
		// SpawnPoint가 지면 자체에 있든 기존 Capsule 중심 높이에 있든 상관없이,
		// 요청 XY 아래의 실제 월드를 찾은 뒤 현재 Capsule 크기로 중심 Z를 재계산한다.
		// ObjectType 기반으로 조회해 Pawn 타입인 다른 AI/HitCollision은 바닥으로 취급하지 않는다.
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		constexpr float GroundClearance = 2.0f;
		constexpr float ExtraTraceDistance = 1000.0f;

		const FVector TraceStart = SafeLocation + FVector::UpVector * FMath::Max(HalfHeight, 100.0f);
		const FVector TraceEnd = SafeLocation - FVector::UpVector * (HalfHeight + ExtraTraceDistance);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AIPoolRespawnGround), false, this);
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FHitResult FloorHit;
		if (World->LineTraceSingleByObjectType(FloorHit, TraceStart, TraceEnd, ObjectParams, QueryParams))
		{
			SafeLocation.Z = FloorHit.ImpactPoint.Z + HalfHeight + GroundClearance;
		}

		// 바닥 높이를 먼저 맞춘 뒤 SpawnActor와 동일하게 주변의 비침투 위치를 찾는다.
		// 풀 상태에서는 Actor collision이 꺼져 있으므로 Capsule만 잠시 Query 대상으로 살린다.
		SetActorEnableCollision(true);
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		FVector AdjustedLocation = SafeLocation;
		if (World->FindTeleportSpot(this, AdjustedLocation, SafeRotation))
		{
			SafeLocation = AdjustedLocation;
		}

		// SetPawnData()->SetPooledActive(true) 전까지는 다시 완전히 비활성 상태로 유지한다.
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetActorEnableCollision(false);
	}

	// SpawnPoint의 Scale은 AI Actor에 덮어쓰지 않는다. AI별 Capsule/Actor 크기는 BP 설정을 유지한다.
	SetActorLocationAndRotation(
		SafeLocation,
		SafeRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ABaseAICharacter::SetPawnData(UAIPawnData* InPawnData)
{
	// 다른 PawnData로 교체되는 경우 InitState를 처음부터 다시 돌려야 한다.
	// Evaluate()는 bGameplayReady면 즉시 return하므로, 이 가드가 없으면
	// 새 AbilitySet이 조용히 부여되지 않는다.
	// 풀 반납 경로에서는 ResetForPool()이 이미 Shutdown()을 불렀으므로 중복 호출은 무해하다.
	if (PawnData != InPawnData && AIInitComponent)
	{
		AIInitComponent->Shutdown();
	}

	PawnData = InPawnData;
	InitializeFromPawnData();

	// 새 Spawn과 Pool Respawn 모두 Character는 Yaw만 사용한다.
	// 풀 재사용은 PrepareForPoolReuse()에서 이미 정리되지만 최초 Spawn도 동일 정책을 유지한다.
	FRotator UprightRotation = GetActorRotation();
	UprightRotation.Pitch = 0.f;
	UprightRotation.Roll = 0.f;
	SetActorRotation(UprightRotation, ETeleportType::TeleportPhysics);

	// 풀 재사용 AI는 AcquireAI()의 PrepareForPoolReuse()에서 Capsule 크기를 반영한
	// 안전 위치를 이미 확정했다. 위치가 결정된 뒤에만 Collision을 다시 켠다.
	SetPooledActive(true);

	// 이동 복원. ResetForPool()의 DisableMovement()를 정확히 되돌린다.
	// 안전 위치 확정 뒤에 해야 FindFloor가 올바른 바닥을 잡는다.
	// MOVE_Walking으로 되돌리지 않으면 BT는 도는데 폰만 제자리에 서 있게 된다.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetComponentTickEnabled(true);
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Walking);
	}

	// 컨트롤러는 풀에 있는 동안에도 possess를 유지한다. 생각만 다시 켜준다.
	if (ABaseAIController* AIC = Cast<ABaseAIController>(GetController()))
	{
		AIC->ResumeLogic();
	}

	// InitState 재평가. 컨트롤러가 살아있으므로 여기서 GameplayReady까지 도달하고
	// 무기 스폰과 BT 시작(TryStartBTFromReady)도 그 안에서 처리된다.
	if (AIInitComponent)
	{
		AIInitComponent->NotifyPossessed();
	}
}

void ABaseAICharacter::ResetForPool()
{
	// 1) 컨트롤러는 possess를 유지한 채 휴면시킨다.
	// UnPossess()는 컨트롤러를 파괴하지 않으므로, 재사용 시 SpawnDefaultController()가
	// 새로 하나를 더 만들어 사이클마다 컨트롤러가 누수된다.
	if (ABaseAIController* AIC = Cast<ABaseAIController>(GetController()))
	{
		AIC->SuspendLogic();
	}

	// 2) 이동 정지.
	// SetActorTickEnabled(false)는 액터 틱만 끈다. CharacterMovementComponent는
	// 독립적으로 틱하므로 풀에 있는 동안에도 계속 돈다. 충돌이 꺼져 있어 바닥을 찾지
	// 못하면 MOVE_Falling으로 전환되어 속도가 계속 누적되고, 재사용 시 그 속도가
	// 남아 있어 스폰 직후 위로 튀거나 위치가 어긋난다.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->ClearAccumulatedForces();
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}

	// 3) 무기 회수. 파괴하지 않고 ASC 연결만 끊고 숨긴다.
	// SetPooledActive(false)보다 먼저 해야 한다. 그 안에서 무기를 도먼시 상태로
	// 만들기 때문에, 순서가 뒤바뀌면 무기의 숨김 상태가 클라이언트로 전파되지 않는다.
	if (WeaponEquipmentComponent)
	{
		WeaponEquipmentComponent->ServerReleaseWeaponsForPool();
	}

	// 4) 렌더링/충돌/틱 끄기 + 몽타주 정리 (복제되어 클라이언트에도 적용된다)
	SetPooledActive(false);

	// 5) InitState 초기화. HealthComponent unbind와 AbilitySet 회수를 모두 소유한다.
	// AbilitySetHandles를 여기서 직접 건드리지 않는다. 회수 주체는 Shutdown() 하나뿐이다.
	if (AIInitComponent)
	{
		AIInitComponent->Shutdown();
	}

	if (ASC)
	{
		// 남아 있는 버프/디버프 제거
		ASC->RemoveActiveGameplayEffectBySourceEffect(UGameplayEffect::StaticClass(), nullptr);
	}
}

void ABaseAICharacter::BeginPlay()
{
	Super::BeginPlay();

	// BP 기본값에 AnimClass가 들어 있어도 Dedicated Server에서는 즉시 제거한다.
	// InitializeFromPawnData()가 이후 다시 호출되어도 같은 정책을 재적용한다.
	DisableDedicatedServerAnimation();

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ABaseAICharacter::OnDeathStarted);
		HealthComponent->OnDeathFinished.AddDynamic(this, &ABaseAICharacter::OnDeathFinished);
	}

	if (ASC)
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntCharacterStatSet::GetBaseMoveSpeedAttribute()).AddUObject(this, &ABaseAICharacter::OnBaseMoveSpeedChanged);
	}

	// 클라이언트에서 InitState 평가를 시작시킨다.
	// 서버는 PossessedBy / SetPawnData 가 NotifyPossessed()로 굴려주지만,
	// 클라이언트에는 그 계기가 없다. AIController는 복제되지 않아 OnRep_Controller도
	// 발화하지 않으므로, 여기서 직접 첫 평가를 돌려야 HealthComponent가 ASC에 연결된다.
	if (!HasAuthority() && AIInitComponent)
	{
		AIInitComponent->NotifyControllerRep();
	}
}

void ABaseAICharacter::OnBaseMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->MaxWalkSpeed = Data.NewValue;
	}
}

void ABaseAICharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASC_AI_Rotation);

	Super::FaceRotation(NewControlRotation, DeltaTime);
}

void ABaseAICharacter::OnDeathFinished(AActor* OwningActor)
{
	if (HasAuthority())
	{
		if (UAIPoolSubsystem* Pool = GetWorld()->GetSubsystem<UAIPoolSubsystem>())
		{
			Pool->ReleaseAI(this);
		}
	}
}
