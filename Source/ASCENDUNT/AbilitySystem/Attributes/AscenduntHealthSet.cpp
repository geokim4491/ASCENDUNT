// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntHealthSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameplayTagsManager.h"
#include "AbilitySystemGlobals.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Character/BaseCharacter.h"
#include "ASCENDUNT/Character/HitCollision/AscHitCollisionComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/AbilitySystem/Components/AscCombatContributionComponent.h"
#include "ASCENDUNT/AbilitySystem/Utility/AscTargetUtil.h"
#include "ASCENDUNT/Player/AscTargetAwarenessComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
	AAscenduntPlayerController* ResolveDamageInstigatorPlayerController(const FGameplayEffectContextHandle& Context)
	{
		AActor* InstigatorActor = Context.GetOriginalInstigator();
		if (!InstigatorActor)
		{
			return nullptr;
		}

		if (AAscenduntPlayerController* PlayerController = Cast<AAscenduntPlayerController>(InstigatorActor))
		{
			return PlayerController;
		}

		if (const APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
		{
			return Cast<AAscenduntPlayerController>(InstigatorPawn->GetController());
		}

		if (UAbilitySystemComponent* InstigatorASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor))
		{
			if (const APawn* AvatarPawn = Cast<APawn>(InstigatorASC->GetAvatarActor()))
			{
				return Cast<AAscenduntPlayerController>(AvatarPawn->GetController());
			}
		}

		return nullptr;
	}

	ABaseCharacter* ResolveDamageInstigatorCharacter(const FGameplayEffectContextHandle& Context)
	{
		AActor* InstigatorActor = Context.GetOriginalInstigator();
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(InstigatorActor))
		{
			return Character;
		}

		if (InstigatorActor)
		{
			if (UAbilitySystemComponent* InstigatorASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor))
			{
				return Cast<ABaseCharacter>(InstigatorASC->GetAvatarActor());
			}
		}

		return nullptr;
	}

	UAscCombatContributionComponent* ResolveInstigatorCombatContribution(const FGameplayEffectContextHandle& Context)
	{
		AActor* InstigatorActor = Context.GetOriginalInstigator();
		if (!InstigatorActor)
		{
			return nullptr;
		}

		AAscenduntPlayerState* PlayerState = Cast<AAscenduntPlayerState>(InstigatorActor);
		if (!PlayerState)
		{
			if (const APawn* Pawn = Cast<APawn>(InstigatorActor))
			{
				PlayerState = Pawn->GetPlayerState<AAscenduntPlayerState>();
			}
		}

		if (!PlayerState)
		{
			if (const AController* Controller = Cast<AController>(InstigatorActor))
			{
				PlayerState = Controller->GetPlayerState<AAscenduntPlayerState>();
			}
		}

		if (!PlayerState)
		{
			if (UAbilitySystemComponent* InstigatorASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor))
			{
				PlayerState = Cast<AAscenduntPlayerState>(InstigatorASC->GetOwnerActor());
			}
		}

		return PlayerState ? PlayerState->GetCombatContributionComponent() : nullptr;
	}

	bool ShouldRecordContribution(const FGameplayEffectSpec& EffectSpec)
	{
		const FGameplayTagContainer& DynamicTags = EffectSpec.GetDynamicAssetTags();
		return !DynamicTags.HasTagExact(AscenduntGameplayTags::Damage_Source_ThornReflect);
	}

	bool IsConfirmedHeadshotForFeedback(const FGameplayEffectSpec& EffectSpec)
	{
		const FGameplayEffectContextHandle& Context = EffectSpec.GetContext();
		const FHitResult* HitResult = Context.GetHitResult();
		if (!HitResult)
		{
			return false;
		}

		const UAscHitCollisionComponent* HitCollision = Cast<UAscHitCollisionComponent>(HitResult->GetComponent());
		if (!HitCollision || !HitCollision->IsHeadZone())
		{
			return false;
		}

		// Keep the UI definition identical to AscenduntDamageExecution: touching the Head hit zone
		// is a headshot only when the damage causer currently allows weapon headshots.
		AActor* EffectCauser = Context.GetEffectCauser();
		UAbilitySystemComponent* CauserASC = EffectCauser
			? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(EffectCauser)
			: nullptr;

		return CauserASC &&
			CauserASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_AllowHeadshot);
	}

	void NotifyInstigatorDamageFeedback(
		UAbilitySystemComponent* TargetASC,
		const FGameplayEffectSpec& EffectSpec,
		float ActualDamage,
		bool bKilled)
	{
		if (!TargetASC || ActualDamage <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FGameplayTagContainer& DynamicTags = EffectSpec.GetDynamicAssetTags();
		if (DynamicTags.HasTagExact(AscenduntGameplayTags::Damage_Source_ThornReflect))
		{
			// Reflected damage is not a direct player hit and should not create weapon hit feedback.
			return;
		}

		const FGameplayEffectContextHandle& Context = EffectSpec.GetContext();
		AAscenduntPlayerController* InstigatorPC = ResolveDamageInstigatorPlayerController(Context);
		ABaseCharacter* InstigatorCharacter = ResolveDamageInstigatorCharacter(Context);
		ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(TargetASC->GetAvatarActor());

		if (!InstigatorPC || !InstigatorCharacter || !TargetCharacter || InstigatorCharacter == TargetCharacter)
		{
			return;
		}

		const bool bHeadshot = IsConfirmedHeadshotForFeedback(EffectSpec);
		if (bKilled)
		{
			// One reliable RPC starts both visual layers together on the owning client.
			InstigatorPC->Client_ShowHitAndKillMarker(bHeadshot);
		}
		else
		{
			InstigatorPC->Client_ShowHitMarker(bHeadshot);
		}
	}

	void GrantInstigatorTargetReveal(
		UAbilitySystemComponent* TargetASC,
		const FGameplayEffectSpec& EffectSpec,
		float ActualDamage)
	{
		if (!TargetASC || ActualDamage <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FGameplayTagContainer& Tags = EffectSpec.GetDynamicAssetTags();
		if (Tags.HasTagExact(AscenduntGameplayTags::Damage_Source_ThornReflect))
		{
			return;
		}

		ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(TargetASC->GetAvatarActor());
		const FGameplayEffectContextHandle& Context = EffectSpec.GetContext();
		ABaseCharacter* InstigatorCharacter = ResolveDamageInstigatorCharacter(Context);
		AAscenduntPlayerController* InstigatorPC = ResolveDamageInstigatorPlayerController(Context);

		if (!TargetCharacter || !InstigatorCharacter || !InstigatorPC || TargetCharacter == InstigatorCharacter)
		{
			return;
		}

		// Current vision policy treats every non-player character as hostile.
		// For player targets, preserve normal team validation.
		const bool bTargetIsPlayer = TargetCharacter->GetPlayerState() != nullptr;
		if (bTargetIsPlayer &&
			TargetCharacter->GetGenericTeamId() == InstigatorCharacter->GetGenericTeamId())
		{
			return;
		}

		if (UAscTargetAwarenessComponent* Awareness = InstigatorPC->GetTargetAwarenessComponent())
		{
			Awareness->GrantTargetReveal(TargetCharacter);
		}
	}
}

UAscenduntHealthSet::UAscenduntHealthSet()
{
	// 안전 기본값(실제 영웅별 값은 DefaultAttributes GE로 설정)
	InitArmorFlatReductionPerInstance(7.f);
	InitArmorMaxReductionPct(0.5f);
	InitThornReflectPct(0.4f);
}

void UAscenduntHealthSet::OnRep_BasicHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntHealthSet, BasicHealth, OldValue);
	BroadcastHealthChangedFromRep(GetBasicHealth(), OldValue.GetCurrentValue());
}

void UAscenduntHealthSet::OnRep_MaxBasicHealth(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntHealthSet, Armor, OldValue);
	BroadcastHealthChangedFromRep(GetArmor(), OldValue.GetCurrentValue());
}

void UAscenduntHealthSet::OnRep_MaxArmor(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_Thorn(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntHealthSet, Thorn, OldValue);
	BroadcastHealthChangedFromRep(GetThorn(), OldValue.GetCurrentValue());
}

void UAscenduntHealthSet::OnRep_MaxThorn(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_OverHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAscenduntHealthSet, OverHealth, OldValue);
	BroadcastHealthChangedFromRep(GetOverHealth(), OldValue.GetCurrentValue());
}

void UAscenduntHealthSet::OnRep_MaxOverHealth(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_RegenRate(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_RegenDelay(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_ArmorFlatReduction(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_ArmorMaxReductionPct(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::OnRep_ThornReflectPct(const FGameplayAttributeData& OldValue)
{
}

void UAscenduntHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAscenduntHealthSet, SourcePawnData);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, BasicHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, MaxBasicHealth, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, MaxArmor, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, Thorn, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, MaxThorn, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, OverHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, MaxOverHealth, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, RegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, RegenDelay, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, ArmorFlatReductionPerInstance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, ArmorMaxReductionPct, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAscenduntHealthSet, ThornReflectPct, COND_None, REPNOTIFY_Always);

	// Meta(Damage/Healing)는 보통 Replicate 안 함 (서버에서 처리 후 결과 Attribute만 Rep)
}

bool UAscenduntHealthSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	return true;
}

void UAscenduntHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	// Damage meta 소비 → 분배(당신 기존 로직 그대로)
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const bool bWasDead = IsDead();

		const float RawDamage = GetDamage();
		SetDamage(0.f);

		float ActualDamage = 0.0f;

		if (RawDamage > 0.f)
		{
			// 분배 로직 호출(이미 가지고 계신 OverHealth→Armor→Health→Thorn)
			const FDamageSplit Split = ApplyDamage_Distribute(Data, RawDamage);
			ActualDamage = Split.UseOver + Split.UseArmor + Split.UseHealth + Split.UseThorn;

			GCCharacterHit(Data, Split, RawDamage, 0.f);
			GrantInstigatorTargetReveal(GetOwningAbilitySystemComponent(), Data.EffectSpec, ActualDamage);

			// “데미지 받음” 이벤트 브로드캐스트
			const FGameplayEffectContextHandle& Ctx = Data.EffectSpec.GetContext();
			AActor* Instigator = Ctx.GetOriginalInstigator();
			SetHealthSnapshot();
			OnHealthChanged.Broadcast(GetOwningActor(), Instigator, true,
				&Data.EffectSpec, RawDamage, LastHealthSnapshot);

			// 반사는 Thorn이 깎일 때만
			if (Split.UseThorn > 0.f)
			{
				TryApplyThornReflect_WhenThornDamaged(Data, RawDamage);
			}
		}

		ClampAll();

		/*
		*  Alive -> Die로 변경된 경우에만 사망 이벤트 발생
		*/
		const bool bIsDeadNow = IsDead();
		const bool bKilledByThisDamage = !bWasDead && bIsDeadNow;

		NotifyInstigatorDamageFeedback(
			GetOwningAbilitySystemComponent(),
			Data.EffectSpec,
			ActualDamage,
			bKilledByThisDamage);

		if (ActualDamage > KINDA_SMALL_NUMBER && ShouldRecordContribution(Data.EffectSpec))
		{
			const FGameplayEffectContextHandle& ContributionContext = Data.EffectSpec.GetContext();
			ABaseCharacter* InstigatorCharacter = ResolveDamageInstigatorCharacter(ContributionContext);
			ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(GetOwningAbilitySystemComponent()
				? GetOwningAbilitySystemComponent()->GetAvatarActor()
				: nullptr);

			if (InstigatorCharacter && TargetCharacter && InstigatorCharacter != TargetCharacter &&
				UAscTargetUtil::IsEnemyActor(InstigatorCharacter, TargetCharacter))
			{
				if (UAscCombatContributionComponent* Contribution = ResolveInstigatorCombatContribution(ContributionContext))
				{
					Contribution->RecordDamageDealt(ActualDamage, bKilledByThisDamage);
				}
			}
		}

		if (bKilledByThisDamage)
		{
			const FGameplayEffectContextHandle& Ctx = Data.EffectSpec.GetContext();
			AActor* Instigator = Ctx.GetOriginalInstigator();

			OnOutOfHealth.Broadcast(
				GetOwningActor(),
				Instigator,
				&Data.EffectSpec);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float RawHeal = GetHealing();
		SetHealing(0.f);

		UAbilitySystemComponent* OwningASC = GetOwningAbilitySystemComponent();
		const bool bIsPvEDowned = OwningASC &&
			OwningASC->HasMatchingGameplayTag(AscenduntGameplayTags::Status_PvE_Downed);

		FGameplayTagContainer EffectAssetTags;
		Data.EffectSpec.GetAllAssetTags(EffectAssetTags);
		const bool bIsReviveEffect = EffectAssetTags.HasTagExact(AscenduntGameplayTags::Effect_Revive);

		// Normal healing/regeneration must not pull a Downed hero back above zero.
		// Step 4's revive GE will carry Effect.Revive and is the only exception.
		if (RawHeal > 0.f && (!bIsPvEDowned || bIsReviveEffect))
		{
			const float TotalHealthBefore = GetTotalHealth();
			ApplyHealing_Distribute(Data, RawHeal);
			const float ActualHeal = FMath::Max(0.f, GetTotalHealth() - TotalHealthBefore);

			// 풀피 등으로 실제 회복량이 없으면 힐 GC와 체력 변경 이벤트를 실행하지 않는다.
			if (ActualHeal > KINDA_SMALL_NUMBER)
			{
				const FDamageSplit Split;
				GCCharacterHit(Data, Split, 0.f, ActualHeal);

				const FGameplayEffectContextHandle& Ctx = Data.EffectSpec.GetContext();
				AActor* Instigator = Ctx.GetOriginalInstigator();

				ABaseCharacter* HealInstigatorCharacter = ResolveDamageInstigatorCharacter(Ctx);
				ABaseCharacter* HealTargetCharacter = Cast<ABaseCharacter>(OwningASC ? OwningASC->GetAvatarActor() : nullptr);
				const bool bSelfHeal = HealInstigatorCharacter && HealTargetCharacter && HealInstigatorCharacter == HealTargetCharacter;
				const bool bFriendlyHeal = HealInstigatorCharacter && HealTargetCharacter &&
					UAscTargetUtil::IsFriendlyActor(HealInstigatorCharacter, HealTargetCharacter);
				if (bSelfHeal || bFriendlyHeal)
				{
					if (UAscCombatContributionComponent* Contribution = ResolveInstigatorCombatContribution(Ctx))
					{
						Contribution->RecordHealingDone(ActualHeal);
					}
				}

				// Scoreboard "healing received" is target-side and counts final applied healing only.
				// Self-heal and revive healing are intentionally included because both actually restore this player.
				if (OwningASC)
				{
					if (AAscenduntPlayerState* TargetPS = Cast<AAscenduntPlayerState>(OwningASC->GetOwnerActor()))
					{
						if (UAscCombatContributionComponent* TargetContribution = TargetPS->GetCombatContributionComponent())
						{
							TargetContribution->RecordHealingReceived(ActualHeal);
						}
					}
				}

				SetHealthSnapshot();
				OnHealthChanged.Broadcast(GetOwningActor(), Instigator, false,
					&Data.EffectSpec, ActualHeal, LastHealthSnapshot);
			}
		}

		ClampAll();
	}
	else
	{
		ClampAll();
	}
}

void UAscenduntHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Max 기반 클램프
	if (Attribute == GetBasicHealthAttribute())        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxBasicHealth());
	if (Attribute == GetArmorAttribute())         NewValue = FMath::Clamp(NewValue, 0.f, GetMaxArmor());
	if (Attribute == GetOverHealthAttribute())    NewValue = FMath::Clamp(NewValue, 0.f, GetMaxOverHealth());
	if (Attribute == GetThornAttribute())         NewValue = FMath::Clamp(NewValue, 0.f, GetMaxThorn());
}

void UAscenduntHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	// 기본 클램프
	if (Attribute == GetBasicHealthAttribute())        NewValue = FMath::Max(0.f, NewValue);
	if (Attribute == GetArmorAttribute())         NewValue = FMath::Max(0.f, NewValue);
	if (Attribute == GetOverHealthAttribute())    NewValue = FMath::Max(0.f, NewValue);
	if (Attribute == GetThornAttribute())         NewValue = FMath::Max(0.f, NewValue);
}

void UAscenduntHealthSet::ClampAll()
{
	SetBasicHealth(FMath::Clamp(GetBasicHealth(), 0.f, GetMaxBasicHealth()));
	SetArmor(FMath::Clamp(GetArmor(), 0.f, GetMaxArmor()));
	SetOverHealth(FMath::Clamp(GetOverHealth(), 0.f, GetMaxOverHealth()));
	SetThorn(FMath::Clamp(GetThorn(), 0.f, GetMaxThorn()));
}

void UAscenduntHealthSet::SetHealthSnapshot()
{
	LastHealthSnapshot.SetFromHealthSet(this);
}

UAscenduntHealthSet::FDamageSplit UAscenduntHealthSet::ApplyDamage_Distribute(const FGameplayEffectModCallbackData& Data, float RawDamage)
{
	FDamageSplit Out;
	float Remaining = RawDamage;

	const FGameplayTagContainer& Tags = Data.EffectSpec.GetDynamicAssetTags();

	const bool bBypassArmor = Tags.HasTagExact(AscenduntGameplayTags::Damage_Policy_IgnoreArmor);
	const bool bBypassOverHealth = Tags.HasTagExact(AscenduntGameplayTags::Damage_Policy_BypassOverHealth);

	const float Instances = Data.EffectSpec.GetSetByCallerMagnitude(AscenduntGameplayTags::Data_Instances, false, 1.f);

	// 1) OverHealth
	if (!bBypassOverHealth && GetOverHealth() > 0.f && Remaining > 0.f)
	{
		Out.UseOver = FMath::Min(GetOverHealth(), Remaining);
		SetOverHealth(GetOverHealth() - Out.UseOver);
		Remaining -= Out.UseOver;
	}

	// 2) Armor (인스턴스 감쇠)
	if (GetArmor() > 0.f && Remaining > 0.f)
	{
		const float DamageToArmor = !bBypassArmor ? ApplyArmorMitigation_PerInstance(
			Remaining, Instances,
			GetArmorFlatReductionPerInstance(),
			GetArmorMaxReductionPct()
		) : Remaining;

		const float ArmorBefore = GetArmor();

		// Armor는 감쇠된 데미지로 깎인다
		Out.UseArmor = FMath::Min(ArmorBefore, DamageToArmor);
		SetArmor(ArmorBefore - Out.UseArmor);

		// Armor가 충분하면 여기서 데미지 처리가 끝나야 한다
		if (ArmorBefore >= DamageToArmor)
		{
			Remaining = 0.f;
		}
		else
		{
			// Armor가 깨졌으면 남은(감쇠된) 데미지만큼만 Health로 전달
			Remaining = DamageToArmor - ArmorBefore;
		}
	}

	// 3) Health
	if (GetBasicHealth() > 0.f && Remaining > 0.f)
	{
		Out.UseHealth = FMath::Min(GetBasicHealth(), Remaining);
		SetBasicHealth(GetBasicHealth() - Out.UseHealth);
		Remaining -= Out.UseHealth;
	}

	// 4) Thorn (Death pool)
	if (GetThorn() > 0.f && Remaining > 0.f)
	{

		Out.UseThorn = FMath::Min(GetThorn(), Remaining);
		SetThorn(GetThorn() - Out.UseThorn);
		Remaining -= Out.UseThorn;
	}

	return Out;
}

void UAscenduntHealthSet::ApplyHealing_Distribute(const FGameplayEffectModCallbackData& Data, float RawHeal)
{
	float Remaining = RawHeal;

	// 1) Thorn
	if (GetThorn() < GetMaxThorn() && Remaining > 0.f)
	{
		const float Add = FMath::Min(GetMaxThorn() - GetThorn(), Remaining);
		SetThorn(GetThorn() + Add);
		Remaining -= Add;
	}

	// 2) Health
	if (GetBasicHealth() < GetMaxBasicHealth() && Remaining > 0.f)
	{
		const float Add = FMath::Min(GetMaxBasicHealth() - GetBasicHealth(), Remaining);
		SetBasicHealth(GetBasicHealth() + Add);
		Remaining -= Add;
	}

	// 3) Armor
	if (GetArmor() < GetMaxArmor() && Remaining > 0.f)
	{
		const float Add = FMath::Min(GetMaxArmor() - GetArmor(), Remaining);
		SetArmor(GetArmor() + Add);
		Remaining -= Add;
	}

	// OverHealth는 치유 불가
}

float UAscenduntHealthSet::ApplyArmorMitigation_PerInstance(float TotalDamage, float Instances, float FlatReduce, float MaxReducePct)
{
	Instances = FMath::Max(1.f, Instances);
	const float Per = TotalDamage / Instances;

	float Sum = 0.f;
	const int32 Count = (int32)Instances;

	for (int32 i = 0; i < Count; ++i)
	{
		const float Reduced = FMath::Max(1.f, Per - FlatReduce); // 최소 데미지 1
		const float MinAllowed = Per * (1.f - MaxReducePct);
		Sum += FMath::Max(Reduced, MinAllowed);
	}
	return Sum;
}

void UAscenduntHealthSet::TryApplyThornReflect_WhenThornDamaged(const FGameplayEffectModCallbackData& Data, float RawDamage)
{
	// 반사 조건:
	// - Thorn이 깎였다는 사실은 호출 측에서 보장(Split.UseThorn > 0)
	// - 반사 금지 공격이면 반사 X
	// - 반사로 들어온 공격은 반사 X (무한루프 방지)
	const FGameplayTagContainer& Tags = Data.EffectSpec.GetDynamicAssetTags();
	if (Tags.HasTagExact(AscenduntGameplayTags::Damage_Policy_NoThornReflect) ||
		Tags.HasTagExact(AscenduntGameplayTags::Damage_Source_ThornReflect))
	{
		return;
	}

	const float Pct = GetThornReflectPct();
	const float ReflectDamage = RawDamage * Pct;
	if (ReflectDamage < 1.f)
	{
		return;
	}

	// 인스티게이터 ASC 찾기
	const FGameplayEffectContextHandle& Ctx = Data.EffectSpec.GetContext();
	AActor* InstigatorActor = Ctx.GetOriginalInstigator();
	if (!InstigatorActor)
	{
		return;
	}

	UAbilitySystemComponent* InstigatorASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor);
	UAbilitySystemComponent* ThornOwnerASC = GetOwningAbilitySystemComponent();
	if (!InstigatorASC || !ThornOwnerASC)
	{
		return;
	}

	// “들어온 Damage GE” 재사용: DamageExecution이 SetByCaller(Data.Damage)를 우선 사용하도록 만들어놨음
	const UGameplayEffect* IncomingDef = Data.EffectSpec.Def;
	if (!IncomingDef)
	{
		return;
	}

	FGameplayEffectContextHandle NewCtx = ThornOwnerASC->MakeEffectContext();
	NewCtx.AddSourceObject(GetOwningActor());

	FGameplayEffectSpecHandle NewSpec = ThornOwnerASC->MakeOutgoingSpec(IncomingDef->GetClass(), 1.f, NewCtx);
	if (!NewSpec.IsValid())
	{
		return;
	}

	// SetByCaller로 반사 데미지 주입
	NewSpec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageOverride, ReflectDamage);
	NewSpec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Instances, 1.f);

	// 반사 출처 태그(루프 방지)
	NewSpec.Data->DynamicAssetTags.AddTag(AscenduntGameplayTags::Damage_Source_ThornReflect);

	ThornOwnerASC->ApplyGameplayEffectSpecToTarget(*NewSpec.Data.Get(), InstigatorASC);
}

void UAscenduntHealthSet::BroadcastHealthChangedFromRep(float NewValue, float OldValue)
{
	const float Delta = NewValue - OldValue;
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}

	SetHealthSnapshot();

	// 클라에선 EffectSpec/Instigator를 모르니 nullptr로 => Instigator 몰라도 서버에서 관리해주면 되니까.
	OnHealthChanged.Broadcast(
		GetOwningActor(),
		nullptr,
		Delta < 0.f,          // 감소면 damaging
		nullptr,              // EffectSpec 없음
		FMath::Abs(Delta),    // RawValue는 delta로 대체
		LastHealthSnapshot
	);
}

void UAscenduntHealthSet::GCCharacterHit(const FGameplayEffectModCallbackData& Data, const FDamageSplit& Split, const float RawDamage, const float RawHeal)
{
	// (권한 검사 AscRouteCue에서 함.)
	if (AActor* OwningActor = GetOwningActor())
	{
		UAscenduntAbilitySystemComponent* TargetASC = Cast<UAscenduntAbilitySystemComponent>(GetOwningAbilitySystemComponent());

		if (TargetASC)
		{
			const float ShieldUsed = Split.UseArmor + Split.UseOver;
			const float HealthUsed = Split.UseHealth + Split.UseThorn;
			const float ThornUsed = Split.UseThorn;

			// ===== GC 파라미터 =====
			const FGameplayEffectContextHandle& Ctx = Data.EffectSpec.GetContext();
			const FHitResult* HR = Ctx.GetHitResult();

			const bool bHeadshot = IsConfirmedHeadshotForFeedback(Data.EffectSpec);


			FGameplayCueParameters P;
			P.EffectContext = Ctx;
			P.AggregatedSourceTags.AppendTags(Data.EffectSpec.GetDynamicAssetTags());
			P.Instigator = Ctx.GetOriginalInstigator();
			P.EffectCauser = Ctx.GetEffectCauser();
			// Projectile effect causers can be destroyed before an owner-only GameplayCue reaches the client.
			// UI cues such as Scroll Damage/Heal require a stable actor reference, so fall back to the instigator.
			if (!P.EffectCauser.IsValid())
			{
				P.EffectCauser = P.Instigator;
			}
			P.NormalizedMagnitude = bHeadshot ? 1.0f : 0.0f;

			if(RawDamage > 0.f)
				P.RawMagnitude = ShieldUsed + HealthUsed;
			else
				P.RawMagnitude = RawHeal;

			if (HR)
			{
				P.Location = HR->ImpactPoint;
				P.Normal = HR->ImpactNormal;
				P.PhysicalMaterial = HR->PhysMaterial.Get();
			}
			else if (AActor* TargetAvatar = TargetASC->GetAvatarActor())
			{
				// 여우 Heal GE처럼 HitResult가 없는 회복
				P.Location = TargetAvatar->GetActorLocation();
			}

			UAscenduntAbilitySystemComponent* InstigatorASC = nullptr;
			if (AActor* InstigatorActor = Ctx.GetOriginalInstigator())
			{
				InstigatorASC = Cast<UAscenduntAbilitySystemComponent>(UAbilitySystemGlobals::
					GetAbilitySystemComponentFromActor(InstigatorActor));
			}

			// ===== Damage =====
			FGameplayTag CueTag;
			// 정책: Health가 실제로 깎였으면 Health VFX, 아니면 Shield VFX
			if (HealthUsed > 0.f)
			{
				CueTag = AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactCharacter_Health;
			}
			else if (ShieldUsed > 0.f)
			{
				CueTag = AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactCharacter_Shield;
			}

			if (CueTag.IsValid())
			{
				// 서버에서 ReplicatedToRelevant로 뿌리고, GCN(Audience=AllExceptTarget)에서 타깃 본인만 제외
				UAscenduntAbilitySystemComponent::
					AscRouteCue(InstigatorASC, TargetASC, CueTag, P, EAscenduntCueRoute::ReplicatedToRelevant);

				// Local로 실행되는 Effect 히트마커, Scroll Damage 등등
				UAscenduntAbilitySystemComponent::
					AscRouteCue(InstigatorASC, TargetASC, AscenduntGameplayTags::GameplayCue_Local_Weapon_ImpactCharacter,
						P, EAscenduntCueRoute::InstigatorOnly_CueTarget);
			}

			// ===== Heal =====
			else if (RawHeal > 0.f)
			{
				// 서버에서 ReplicatedToRelevant로 뿌리고, GCN(Audience=AllExceptTarget)에서 타깃 본인만 제외
				UAscenduntAbilitySystemComponent::
					AscRouteCue(InstigatorASC, TargetASC, AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactCharacter_Heal,
						P, EAscenduntCueRoute::ReplicatedToRelevant);

				// Local로 실행되는 Effect 히트마커, Scroll Heal 등등
				UAscenduntAbilitySystemComponent::
					AscRouteCue(InstigatorASC, TargetASC, AscenduntGameplayTags::GameplayCue_Local_Weapon_ImpactCharacter_Heal,
						P, EAscenduntCueRoute::InstigatorOnly_CueTarget);
			}
		}
	}
}

void FAscenduntHealthSnapshot::SetFromHealthSet(const UAscenduntHealthSet* HealthSet)
{
	if (HealthSet)
	{
		BasicHealth = HealthSet->GetBasicHealth();
		MaxBasicHealth = HealthSet->GetMaxBasicHealth();
		Armor = HealthSet->GetArmor();
		MaxArmor = HealthSet->GetMaxArmor();
		Thorn = HealthSet->GetThorn();
		MaxThorn = HealthSet->GetMaxThorn();
		OverHealth = HealthSet->GetOverHealth();
		MaxOverHealth = HealthSet->GetMaxOverHealth();
	}
}


float FAscenduntHealthSnapshot::GetCurrentTotal() const
{
	return
		FMath::Max(0.0f, Thorn) +
		FMath::Max(0.0f, BasicHealth) +
		FMath::Max(0.0f, Armor) +
		FMath::Max(0.0f, OverHealth);
}

float FAscenduntHealthSnapshot::GetDisplayCapacityTotal() const
{
	return
		FMath::Max(0.0f, MaxThorn) +
		FMath::Max(0.0f, MaxBasicHealth) +
		FMath::Max(0.0f, MaxArmor) +
		FMath::Max(0.0f, FMath::Max(MaxOverHealth, OverHealth));
}
