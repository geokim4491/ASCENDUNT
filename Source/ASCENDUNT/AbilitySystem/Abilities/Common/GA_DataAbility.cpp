#include "GA_DataAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include "ASCENDUNT/AbilitySystem/AbilityTask/AT_PlayWeaponMontageAndWait.h"
#include "ASCENDUNT/AbilitySystem/AscenduntGameplayEffectContext.h"
#include "ASCENDUNT/AbilitySystem/Data/AscAbilityData.h"
#include "ASCENDUNT/AbilitySystem/Effects/AscSkillCooldownGameplayEffect.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntCharacterStatSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

UGA_DataAbility::UGA_DataAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

const FGameplayTagContainer* UGA_DataAbility::GetCooldownTags() const
{
	RuntimeCooldownTags.Reset();

	const UAscAbilityData* AbilityData = GetAbilityData();
	if (!AbilityData || AbilityData->CooldownDuration <= 0.0f)
	{
		return &RuntimeCooldownTags;
	}

	// Stock/recharge abilities use their stock recovery timer as the cooldown presentation.
	// Do not additionally block the whole ability through GAS while another charge is available.
	FGameplayTag StockTag;
	int32 MaxStock = 0;
	float RecoverDuration = 0.0f;
	if (GetSkillStockDefinition(StockTag, MaxStock, RecoverDuration))
	{
		return &RuntimeCooldownTags;
	}

	if (AbilityData->CooldownTag.IsValid())
	{
		RuntimeCooldownTags.AddTag(AbilityData->CooldownTag);
		return &RuntimeCooldownTags;
	}

	// Backward-compatible generic fallback for existing DataAssets that predate CooldownTag.
	// Any Ability.Hero.Skill tag maps to Cooldown.Hero.Skill without character-specific code.
	TArray<FGameplayTag> AssetTagArray;
	GetAssetTags().GetGameplayTagArray(AssetTagArray);
	for (const FGameplayTag& AbilityAssetTag : AssetTagArray)
	{
		FString AbilityName = AbilityAssetTag.ToString();
		if (!AbilityName.RemoveFromStart(TEXT("Ability.")))
		{
			continue;
		}

		const FGameplayTag DerivedCooldownTag = FGameplayTag::RequestGameplayTag(
			FName(*FString::Printf(TEXT("Cooldown.%s"), *AbilityName)),
			false);
		if (DerivedCooldownTag.IsValid())
		{
			RuntimeCooldownTags.AddTag(DerivedCooldownTag);
			break;
		}
	}

	return &RuntimeCooldownTags;
}

void UGA_DataAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UAscAbilityData* AbilityData = GetAbilityData();
	if (!AbilityData || AbilityData->CooldownDuration <= 0.0f || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	FGameplayTag StockTag;
	int32 MaxStock = 0;
	float RecoverDuration = 0.0f;
	if (GetSkillStockDefinition(StockTag, MaxStock, RecoverDuration))
	{
		return;
	}

	float CooldownDuration = AbilityData->CooldownDuration;
	if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		const FGameplayAttribute CooldownScalarAttribute = UAscenduntCharacterStatSet::GetCooldownDurationScalarAttribute();
		if (ASC->HasAttributeSetForAttribute(CooldownScalarAttribute))
		{
			const float CooldownScalar = ASC->GetNumericAttribute(CooldownScalarAttribute);
			CooldownDuration *= FMath::Max(0.0f, CooldownScalar);
		}
	}

	if (CooldownDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	if (!CooldownTags || CooldownTags->IsEmpty())
	{
		return;
	}

	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		UAscSkillCooldownGameplayEffect::StaticClass(),
		GetAbilityLevel(Handle, ActorInfo));

	if (!CooldownSpec.IsValid())
	{
		return;
	}

	CooldownSpec.Data->SetDuration(CooldownDuration, true);
	CooldownSpec.Data->DynamicGrantedTags.AppendTags(*CooldownTags);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec);
}

bool UGA_DataAbility::PlayAbilityMontage(const UAscAbilityData* AbilityData)
{
	if (!AbilityData || !CurrentActorInfo)
	{
		return false;
	}

	const FAscMontageData& MontageData = AbilityData->Montage;
	const float SafePlayRate = FMath::Max(0.01f, MontageData.PlayRate);
	bool bPlayedAny = false;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	ABaseAICharacter* AIAvatar = Cast<ABaseAICharacter>(Avatar);
	AHeroCharacter* HeroAvatar = Cast<AHeroCharacter>(Avatar);
	const bool bAuthority = CurrentActorInfo->IsNetAuthority();
	const bool bLocallyControlled = CurrentActorInfo->IsLocallyControlled();
	const bool bServerAI = AIAvatar && bAuthority;
	const bool bServerAuthoritativePresentation = HeroAvatar && bAuthority &&
		(NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly ||
		 NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated);

	// ServerOnly/ServerInitiated player abilities do not execute on the owning client.
	// Therefore a normal PlayMontageAndWait task on the Dedicated Server cannot drive the local 1P arms.
	// Explicitly multicast presentation: every client gets 3P, while HeroCharacter lets only the owner play 1P.
	if ((MontageData.HeroMontage || MontageData.HeroMontage1P) && bServerAuthoritativePresentation)
	{
		HeroAvatar->Multicast_PlayAbilityPresentationMontage(
			MontageData.HeroMontage,
			MontageData.HeroMontage1P,
			SafePlayRate,
			MontageData.StartSection);
		bPlayedAny = true;
	}
	else
	{
		if (MontageData.HeroMontage && bServerAI)
		{
			AIAvatar->Multicast_PlayPresentationMontage(
				MontageData.HeroMontage,
				SafePlayRate,
				MontageData.StartSection);
			bPlayedAny = true;
		}
		else if (MontageData.HeroMontage && (bLocallyControlled || bAuthority))
		{
			if (HeroMontageTask)
			{
				HeroMontageTask->EndTask();
				HeroMontageTask = nullptr;
			}

			HeroMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				NAME_None,
				MontageData.HeroMontage,
				SafePlayRate,
				MontageData.StartSection,
				/*bStopWhenAbilityEnds*/ false);

			if (HeroMontageTask)
			{
				HeroMontageTask->ReadyForActivation();
				bPlayedAny = true;
			}
		}

		// Predicted/local abilities already execute on the owner, so their 1P presentation can stay local.
		if (MontageData.HeroMontage1P && bLocallyControlled && HeroAvatar)
		{
			if (UHeroFirstPersonComponent* FirstPerson = HeroAvatar->GetFirstPersonComponent())
			{
				bPlayedAny |= FirstPerson->PlayMontage1P(
					MontageData.HeroMontage1P,
					SafePlayRate,
					MontageData.StartSection) > 0.0f;
			}
		}
	}

	if (MontageData.WeaponMontage && bServerAI)
	{
		AAscenduntWeapon* Weapon = GetCurrentWeaponFromActorInfo();
		if (Weapon)
		{
			Weapon->Multicast_PlayWeaponMontage(MontageData.WeaponMontage, SafePlayRate);
			bPlayedAny = true;
		}
	}
	else if (MontageData.WeaponMontage && (bLocallyControlled || bAuthority))
	{
		AAscenduntWeapon* Weapon = GetCurrentWeaponFromActorInfo();
		if (Weapon)
		{
			if (WeaponMontageTask)
			{
				WeaponMontageTask->EndTask();
				WeaponMontageTask = nullptr;
			}

			const bool bReplicateToOthers = bAuthority;
			WeaponMontageTask = UAT_PlayWeaponMontageAndWait::PlayWeaponMontageAndWait(
				this,
				NAME_None,
				Weapon,
				MontageData.WeaponMontage,
				SafePlayRate,
				bReplicateToOthers,
				/*bStopWhenAbilityEnds*/ false);

			if (WeaponMontageTask)
			{
				WeaponMontageTask->ReadyForActivation();
				bPlayedAny = true;
			}
		}
	}

	return bPlayedAny;
}

bool UGA_DataAbility::StartAbilityMontageLifecycle(const UAscAbilityData* AbilityData)
{
	bFinishingDataAbility = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MontageFallbackTimer);
	}

	if (!AbilityData || !CurrentActorInfo)
	{
		return false;
	}

	const FAscMontageData& MontageData = AbilityData->Montage;
	UAnimMontage* PrimaryMontage = MontageData.HeroMontage
		? MontageData.HeroMontage
		: (MontageData.HeroMontage1P ? MontageData.HeroMontage1P : MontageData.WeaponMontage);
	const bool bPlayedMontage = PlayAbilityMontage(AbilityData);

	if (!PrimaryMontage || !MontageData.bEndAbilityWhenMontageEnds)
	{
		return false;
	}

	if (MontageData.HeroMontage && HeroMontageTask)
	{
		HeroMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleHeroMontageCompleted);
		HeroMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleHeroMontageInterrupted);
		HeroMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleHeroMontageInterrupted);
	}
	else if (!MontageData.HeroMontage && MontageData.WeaponMontage && WeaponMontageTask)
	{
		WeaponMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleWeaponMontageCompleted);
		WeaponMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleWeaponMontageInterrupted);
	}

	// Fallback for cases where a montage asset exists but an anim task cannot complete normally
	// in the current execution context. The timer is cleared if a montage delegate ends first.
	const float Duration = PrimaryMontage->GetPlayLength() / FMath::Max(0.01f, MontageData.PlayRate);
	if (Duration > KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MontageFallbackTimer,
				this,
				&ThisClass::HandleMontageFallbackCompleted,
				Duration,
				false);
		}
	}

	return bPlayedMontage || Duration > KINDA_SMALL_NUMBER;
}

void UGA_DataAbility::HandleHeroMontageCompleted()
{
	FinishDataAbility(false);
}

void UGA_DataAbility::HandleHeroMontageInterrupted()
{
	FinishDataAbility(true);
}

void UGA_DataAbility::HandleWeaponMontageCompleted()
{
	FinishDataAbility(false);
}

void UGA_DataAbility::HandleWeaponMontageInterrupted()
{
	FinishDataAbility(true);
}

void UGA_DataAbility::HandleMontageFallbackCompleted()
{
	FinishDataAbility(false);
}

void UGA_DataAbility::FinishDataAbility(bool bWasCancelled)
{
	if (bFinishingDataAbility)
	{
		return;
	}

	bFinishingDataAbility = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MontageFallbackTimer);
	}

	OnDataAbilityMontageFinished(bWasCancelled);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UGA_DataAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MontageFallbackTimer);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// InstancedPerActor abilities reuse the same GA instance across activations.
	// FinishDataAbility() raises this guard while ending; clear it only after EndAbility
	// has completed so the next activation can finish normally as well.
	bFinishingDataAbility = false;
}

AAscenduntWeapon* UGA_DataAbility::GetCurrentWeaponFromActorInfo() const
{
	if (const AHeroCharacter* Hero = Cast<AHeroCharacter>(GetCharacterFromActorInfo()))
	{
		const UWeaponEquipmentComponent* WeaponComp = Hero->GetWeaponEquipmentComponent();
		return WeaponComp ? WeaponComp->GetCurrentWeapon() : nullptr;
	}

	if (const ABaseAICharacter* AI = Cast<ABaseAICharacter>(GetCharacterFromActorInfo()))
	{
		const UWeaponEquipmentComponent* WeaponComp = AI->GetWeaponEquipmentComponent();
		return WeaponComp ? WeaponComp->GetCurrentWeapon() : nullptr;
	}

	return nullptr;
}

bool UGA_DataAbility::ApplyDamageEffectToActor(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass,
	const FHitResult& Hit, float DamageOverride, float DamageCoef) const
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !TargetActor || !DamageEffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	AActor* AvatarActor = CurrentActorInfo->AvatarActor.Get();
	if (!SourceASC || !TargetASC || !AvatarActor)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(const_cast<UGA_DataAbility*>(this));
	EffectContext.AddInstigator(Cast<APawn>(AvatarActor), AvatarActor);
	EffectContext.AddOrigin(AvatarActor->GetActorLocation());
	EffectContext.AddHitResult(Hit, true);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	if (DamageOverride >= 0.0f)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageOverride, DamageOverride);
	}
	else
	{
		SpecHandle.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageCoef, FMath::Max(0.0f, DamageCoef));
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

bool UGA_DataAbility::ApplyGameplayEffectToOwner(TSubclassOf<UGameplayEffect> EffectClass, float Level) const
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !EffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!SourceASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	SourceASC->ApplyGameplayEffectToSelf(EffectClass->GetDefaultObject<UGameplayEffect>(), Level, EffectContext);
	return true;
}
