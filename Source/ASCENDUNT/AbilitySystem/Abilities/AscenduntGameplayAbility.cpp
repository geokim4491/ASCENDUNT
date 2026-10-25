// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntGameplayAbility.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCameraComponent.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode.h"
#include "AbilitySystemLog.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"


#define ENSURE_ABILITY_IS_INSTANTIATED_OR_RETURN(FunctionName, ReturnValue) \
{ \
	if (!ensure(IsInstantiated())) \
	{ \
		UE_LOG(LogAbilitySystem, Error, TEXT("%s: %s cannot be called on non-instanced ability."), *GetPathName(), TEXT(#FunctionName)); \
		return ReturnValue; \
	} \
}

UAscenduntGameplayAbility::UAscenduntGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

	ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
}

UAscenduntAbilitySystemComponent* UAscenduntGameplayAbility::GetAscenduntASCFromActorInfo() const
{
	return CurrentActorInfo ? Cast<UAscenduntAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

APlayerController* UAscenduntGameplayAbility::GetPlayerControllerFromActorInfo() const
{
	return CurrentActorInfo ? CurrentActorInfo->PlayerController.Get() : nullptr;
}

AController* UAscenduntGameplayAbility::GetControllerFromActorInfo() const
{
	if (!CurrentActorInfo) return nullptr;

	if (AController* PC = CurrentActorInfo->PlayerController.Get())
	{
		return PC;
	}

	// Owner 체인에서 Controller/Pawn 찾기
	AActor* TestActor = CurrentActorInfo->OwnerActor.Get();
	while (TestActor)
	{
		if (AController* C = Cast<AController>(TestActor))
		{
			return C;
		}
		if (APawn* Pawn = Cast<APawn>(TestActor))
		{
			return Pawn->GetController();
		}
		TestActor = TestActor->GetOwner();
	}

	return nullptr;
}

APawn* UAscenduntGameplayAbility::GetPawnFromActorInfo() const
{
	return CurrentActorInfo ? Cast<APawn>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

ACharacter* UAscenduntGameplayAbility::GetCharacterFromActorInfo() const
{
	return CurrentActorInfo ? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

UHeroCameraComponent* UAscenduntGameplayAbility::GetHeroCameraComponentFromActorInfo() const
{
	return CurrentActorInfo ? UHeroCameraComponent::FindHeroCameraComponent(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

bool UAscenduntGameplayAbility::DoesAbilitySatisfyTagRequirements(
	const UAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::DoesAbilitySatisfyTagRequirements(
		AbilitySystemComponent,
		SourceTags,
		TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	const UAscenduntAbilitySystemComponent* AscASC =
		Cast<UAscenduntAbilitySystemComponent>(&AbilitySystemComponent);
	if (!AscASC)
	{
		return true;
	}

	FGameplayTagContainer AdditionalRequiredTags;
	FGameplayTagContainer AdditionalBlockedTags;
	AscASC->GetAdditionalActivationTagRequirements(
		GetAssetTags(),
		AdditionalRequiredTags,
		AdditionalBlockedTags);

	if (AdditionalRequiredTags.IsEmpty() && AdditionalBlockedTags.IsEmpty())
	{
		return true;
	}

	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent.GetOwnedGameplayTags(OwnedTags);

	const bool bBlocked = OwnedTags.HasAny(AdditionalBlockedTags);
	const bool bMissingRequired = !OwnedTags.HasAll(AdditionalRequiredTags);

	if (OptionalRelevantTags)
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

		if (bBlocked)
		{
			OptionalRelevantTags->AddTag(AbilitySystemGlobals.ActivateFailTagsBlockedTag);
			OptionalRelevantTags->AppendMatchingTags(OwnedTags, AdditionalBlockedTags);
		}

		if (bMissingRequired)
		{
			OptionalRelevantTags->AddTag(AbilitySystemGlobals.ActivateFailTagsMissingTag);

			FGameplayTagContainer MissingTags = AdditionalRequiredTags;
			MissingTags.RemoveTags(OwnedTags.GetGameplayTagParents());
			OptionalRelevantTags->AppendTags(MissingTags);
		}
	}

	return !bBlocked && !bMissingRequired;
}

bool UAscenduntGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// PvE Downed heroes may still crawl/look through native input, but no GameplayAbility
	// is allowed to activate until they are revived. This also blocks programmatic
	// TryActivateAbility calls, not only player input.
	if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(AscenduntGameplayTags::Status_PvE_Downed))
	{
		return false;
	}

	// ActivationGroup 블로킹 체크
	if (const UAscenduntAbilitySystemComponent* AscASC = Cast<UAscenduntAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
	{
		if (AscASC->IsActivationGroupBlocked(ActivationGroup))
		{
			// OptionalRelevantTags에 Fail Tag를 넣고 싶으면 여기서 추가(프로젝트 태그 정해지면)
			return false;
		}
	}

	return true;
}

void UAscenduntGameplayAbility::SetCanBeCanceled(bool bCanBeCanceled)
{
	// Replaceable 그룹은 cancel을 막으면 규칙이 깨짐
	if (!bCanBeCanceled && (ActivationGroup == EAscenduntAbilityActivationGroup::Exclusive_Replaceable))
	{
		UE_LOG(LogAbilitySystem, Error, TEXT("SetCanBeCanceled: Replaceable ability [%s] cannot block canceling."), *GetName());
		return;
	}
	Super::SetCanBeCanceled(bCanBeCanceled);
}

void UAscenduntGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	K2_OnAbilityAdded();
	TryActivateAbilityOnSpawn(ActorInfo, Spec);
}

void UAscenduntGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	K2_OnAbilityRemoved();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UAscenduntGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 그룹 등록(클라/서버 둘 다 동일하게 유지)
	if (UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo())
	{
		AscASC->AddAbilityToActivationGroup(ActivationGroup, this);
	}
}

void UAscenduntGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo())
	{
		AscASC->RemoveAbilityFromActivationGroup(ActivationGroup, this);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAscenduntGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	const bool bIsPredicting = (Spec.ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Predicting);

	if (ActorInfo && !Spec.IsActive() && !bIsPredicting && (ActivationPolicy == EAscenduntAbilityActivationPolicy::OnSpawn))
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		const AActor* AvatarActor = ActorInfo->AvatarActor.Get();

		if (ASC && AvatarActor && !AvatarActor->GetTearOff() && (AvatarActor->GetLifeSpan() <= 0.0f))
		{
			const bool bIsLocalExecution =
				(NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted) ||
				(NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly);

			const bool bIsServerExecution =
				(NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly) ||
				(NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated);

			const bool bClientShouldActivate = ActorInfo->IsLocallyControlled() && bIsLocalExecution;
			const bool bServerShouldActivate = ActorInfo->IsNetAuthority() && bIsServerExecution;

			if (bClientShouldActivate || bServerShouldActivate)
			{
				ASC->TryActivateAbility(Spec.Handle);
			}
		}
	}
}

bool UAscenduntGameplayAbility::CanChangeActivationGroup(EAscenduntAbilityActivationGroup NewGroup) const
{
	if (!IsInstantiated() || !IsActive())
	{
		return false;
	}
	if (ActivationGroup == NewGroup)
	{
		return true;
	}

	UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo();
	if (!AscASC)
	{
		return false;
	}

	if ((ActivationGroup != EAscenduntAbilityActivationGroup::Exclusive_Blocking) && AscASC->IsActivationGroupBlocked(NewGroup))
	{
		return false;
	}

	if ((NewGroup == EAscenduntAbilityActivationGroup::Exclusive_Replaceable) && !CanBeCanceled())
	{
		return false;
	}

	return true;
}

bool UAscenduntGameplayAbility::ChangeActivationGroup(EAscenduntAbilityActivationGroup NewGroup)
{
	ENSURE_ABILITY_IS_INSTANTIATED_OR_RETURN(ChangeActivationGroup, false);

	if (!CanChangeActivationGroup(NewGroup))
	{
		return false;
	}

	if (ActivationGroup != NewGroup)
	{
		UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo();
		if (!AscASC)
		{
			return false;
		}

		AscASC->RemoveAbilityFromActivationGroup(ActivationGroup, this);
		AscASC->AddAbilityToActivationGroup(NewGroup, this);

		ActivationGroup = NewGroup;
	}

	return true;
}

void UAscenduntGameplayAbility::SetCameraMode(TSubclassOf<UAscenduntCameraMode> CameraMode)
{
	ENSURE_ABILITY_IS_INSTANTIATED_OR_RETURN(SetCameraMode, );

	if (UHeroCameraComponent* CameraComponent = GetHeroCameraComponentFromActorInfo())
	{
		CameraComponent->SetAbilityCameraMode(CameraMode, CurrentSpecHandle);
		ActiveCameraMode = CameraMode;
	}
}

void UAscenduntGameplayAbility::ClearCameraMode()
{
	ENSURE_ABILITY_IS_INSTANTIATED_OR_RETURN(ClearCameraMode, );

	if (ActiveCameraMode)
	{
		if (UHeroCameraComponent* CameraComponent = GetHeroCameraComponentFromActorInfo())
		{
			CameraComponent->ClearAbilityCameraMode(CurrentSpecHandle);
		}

		ActiveCameraMode = nullptr;
	}
}

void UAscenduntGameplayAbility::GetAbilitySource(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	float& OutSourceLevel, const IAscenduntAbilitySourceInterface*& OutAbilitySource, AActor*& OutEffectCauser) const
{
	OutSourceLevel = 0.f;
	OutAbilitySource = nullptr;
	OutEffectCauser = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	UObject* SourceObject = GetSourceObject(Handle, ActorInfo);

	// EffectCauser는 Weapon(소스오브젝트)이 액터면 그걸로
	if (AActor* AsActor = Cast<AActor>(SourceObject))
	{
		OutEffectCauser = AsActor;
	}

	OutAbilitySource = Cast<IAscenduntAbilitySourceInterface>(SourceObject);
	OutSourceLevel = GetAbilityLevel(Handle, ActorInfo); // 최소한 레벨 스케일링은 가능하게
}

FGameplayEffectContextHandle UAscenduntGameplayAbility::MakeEffectContext(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	FGameplayEffectContextHandle Ctx = Super::MakeEffectContext(Handle, ActorInfo);

	if (!ActorInfo)
	{
		return Ctx;
	}

	float SourceLevel = 0.f;
	const IAscenduntAbilitySourceInterface* AbilitySource = nullptr;
	AActor* EffectCauser = nullptr;
	GetAbilitySource(Handle, ActorInfo, SourceLevel, AbilitySource, EffectCauser);

	UObject* SourceObject = GetSourceObject(Handle, ActorInfo);
	AActor* Instigator = ActorInfo->OwnerActor.Get();

	Ctx.AddInstigator(Instigator, EffectCauser);
	Ctx.AddSourceObject(SourceObject);

	return Ctx;
}
