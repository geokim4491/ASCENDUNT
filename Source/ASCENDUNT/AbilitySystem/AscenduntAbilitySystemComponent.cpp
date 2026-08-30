// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Common/GA_DataAbility.h"
#include "ASCENDUNT/AbilitySystem/Components/AscSkillStockComponent.h"
#include "ASCENDUNT/AbilitySystem/Data/AscAbilityData.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_AbilityInputBlocked, "Gameplay.AbilityInputBlocked");

UAscenduntAbilitySystemComponent::UAscenduntAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationGroupAbilities.SetNum((int32)EAscenduntAbilityActivationGroup::MAX);
}

void UAscenduntAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UAscenduntAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void UAscenduntAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag)))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			}
		}
	}
}

void UAscenduntAbilitySystemComponent::AbilityInputTagTriggered(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (AbilitySpec.Ability && AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			InputTriggeredSpecHandles.AddUnique(AbilitySpec.Handle);
		}
	}
}

void UAscenduntAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(TAG_Gameplay_AbilityInputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reset();

	//@TODO: See if we can use FScopedServerAbilityRPCBatcher ScopedRPCBatcher in some of these loops

	//
	// Process all abilities that activate when the input is held.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability && !AbilitySpec->IsActive())
			{
				const UAscenduntGameplayAbility* AscenduntAbilityCDO = Cast<UAscenduntGameplayAbility>(AbilitySpec->Ability);
				if (AscenduntAbilityCDO && AscenduntAbilityCDO->GetActivationPolicy() == EAscenduntAbilityActivationPolicy::WhileInputActive)
				{
					AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
				}
			}
		}
	}

	//
	// Process one-shot axis inputs. These do not set InputPressed and do not
	// enter the held-input list, so mouse-wheel abilities cannot remain stuck.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputTriggeredSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			const UAscenduntGameplayAbility* AscenduntAbilityCDO =
				AbilitySpec->Ability ? Cast<UAscenduntGameplayAbility>(AbilitySpec->Ability) : nullptr;

			if (AscenduntAbilityCDO &&
				AscenduntAbilityCDO->GetActivationPolicy() == EAscenduntAbilityActivationPolicy::OnInputTriggered)
			{
				AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
			}
		}
	}

	//
	// Process all abilities that had their input pressed this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = true;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputPressed(*AbilitySpec);
				}
				else
				{
					const UAscenduntGameplayAbility* AscenduntAbilityCDO = Cast<UAscenduntGameplayAbility>(AbilitySpec->Ability);

					if (AscenduntAbilityCDO && AscenduntAbilityCDO->GetActivationPolicy() == EAscenduntAbilityActivationPolicy::OnInputTriggered)
					{
						AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
					}
				}
			}
		}
	}

	// A Hold interaction (revive included) is cancelled when the player tries to
	// activate any other GameplayAbility. Damage does not touch this path, so taking
	// damage does not cancel the interaction. Movement is separately locked by GA_Interact.
	if (HasMatchingGameplayTag(AscenduntGameplayTags::Status_Interacting))
	{
		bool bRequestedOtherAbility = false;
		for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
		{
			const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilitySpecHandle);
			if (AbilitySpec && !AbilitySpec->DynamicAbilityTags.HasTagExact(AscenduntGameplayTags::InputTag_Interact))
			{
				bRequestedOtherAbility = true;
				break;
			}
		}

		if (bRequestedOtherAbility)
		{
			FGameplayTagContainer InteractAbilityTags;
			InteractAbilityTags.AddTag(AscenduntGameplayTags::Ability_Interact);
			CancelAbilities(&InteractAbilityTags);
		}
	}

	//
	// Try to activate all the abilities that are from presses and holds.
	// We do it all at once so that held inputs don't activate the ability
	// and then also send a input event to the ability because of the press.
	//
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	//
	// Process all abilities that had their input released this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = false;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputReleased(*AbilitySpec);
				}
			}
		}
	}

	//
	// Clear the cached ability handles.
	//
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputTriggeredSpecHandles.Reset();
}

void UAscenduntAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
	InputTriggeredSpecHandles.Reset();
}

void UAscenduntAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		const UAscenduntGameplayAbility* AscenduntAbility =
			Cast<UAscenduntGameplayAbility>(AbilitySpec.Ability);
		if (AscenduntAbility)
		{
			AscenduntAbility->TryActivateAbilityOnSpawn(ActorInfo, AbilitySpec);
		}
	}
}

bool UAscenduntAbilitySystemComponent::GetAbilityCooldownByAbilityTag(FGameplayTag AbilityTag, float& TimeRemaining, float& CooldownDuration) const
{
	TimeRemaining = 0.0f;
	CooldownDuration = 0.0f;

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!AbilityTag.IsValid() || !ActorInfo)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			continue;
		}

		AbilitySpec.Ability->GetCooldownTimeRemainingAndDuration(
			AbilitySpec.Handle, ActorInfo, TimeRemaining, CooldownDuration);
		return TimeRemaining > KINDA_SMALL_NUMBER;
	}

	return false;
}

bool UAscenduntAbilitySystemComponent::GetAbilityCooldownByInputTag(FGameplayTag InputTag, float& TimeRemaining, float& CooldownDuration) const
{
	TimeRemaining = 0.0f;
	CooldownDuration = 0.0f;

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!InputTag.IsValid() || !ActorInfo)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			continue;
		}

		AbilitySpec.Ability->GetCooldownTimeRemainingAndDuration(
			AbilitySpec.Handle, ActorInfo, TimeRemaining, CooldownDuration);
		return TimeRemaining > KINDA_SMALL_NUMBER;
	}

	return false;
}

bool UAscenduntAbilitySystemComponent::GetAbilityUIStateByInputTag(FGameplayTag InputTag, FAscAbilityUIState& OutState) const
{
	OutState = FAscAbilityUIState();

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!InputTag.IsValid() || !ActorInfo)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.DynamicAbilityTags.HasTagExact(InputTag))
		{
			continue;
		}

		if (const UGA_DataAbility* DataAbility = Cast<UGA_DataAbility>(AbilitySpec.Ability))
		{
			if (const UAscAbilityData* AbilityData = DataAbility->GetAbilityDataForUI())
			{
				OutState.AbilityIcon = AbilityData->AbilityIcon;
			}

			FGameplayTag StockTag;
			int32 ConfiguredMaxStock = 0;
			float ConfiguredRecoverDuration = 0.0f;
			if (DataAbility->GetSkillStockDefinition(StockTag, ConfiguredMaxStock, ConfiguredRecoverDuration))
			{
				OutState.bUsesStock = true;
				OutState.MaxStock = ConfiguredMaxStock;
				OutState.CurrentStock = ConfiguredMaxStock;
				OutState.Duration = ConfiguredRecoverDuration;

				AActor* AvaActor = ActorInfo->AvatarActor.Get();
				const UAscSkillStockComponent* StockComp = AvaActor
					? AvaActor->FindComponentByClass<UAscSkillStockComponent>()
					: nullptr;

				if (StockComp)
				{
					const int32 ReplicatedMaxStock = StockComp->GetMaxStock(StockTag);
					if (ReplicatedMaxStock > 0)
					{
						OutState.MaxStock = ReplicatedMaxStock;
						OutState.CurrentStock = StockComp->GetCurrentStock(StockTag);
					}

					float RechargeRemaining = 0.0f;
					float RechargeDuration = 0.0f;
					if (StockComp->GetRecoverState(StockTag, RechargeRemaining, RechargeDuration))
					{
						OutState.bShowCooldownOverlay = true;
						OutState.TimeRemaining = RechargeRemaining;
						OutState.Duration = RechargeDuration;
						OutState.OverlayPercent = RechargeDuration > KINDA_SMALL_NUMBER
							? FMath::Clamp(RechargeRemaining / RechargeDuration, 0.0f, 1.0f)
							: 0.0f;
					}
				}

				return true;
			}
		}

		AbilitySpec.Ability->GetCooldownTimeRemainingAndDuration(
			AbilitySpec.Handle, ActorInfo, OutState.TimeRemaining, OutState.Duration);
		OutState.bShowCooldownOverlay = OutState.TimeRemaining > KINDA_SMALL_NUMBER;
		OutState.OverlayPercent = OutState.Duration > KINDA_SMALL_NUMBER
			? FMath::Clamp(OutState.TimeRemaining / OutState.Duration, 0.0f, 1.0f)
			: 0.0f;
		return true;
	}

	return false;
}

bool UAscenduntAbilitySystemComponent::IsActivationGroupBlocked(EAscenduntAbilityActivationGroup Group) const
{
	if (Group == EAscenduntAbilityActivationGroup::Independent)
	{
		return false;
	}

	// Blocking이 하나라도 있으면 다른 Exclusive는 막힘
	const bool bHasBlocking =
		ActivationGroupAbilities[(int32)EAscenduntAbilityActivationGroup::Exclusive_Blocking].Num() > 0;

	return bHasBlocking;
}

void UAscenduntAbilitySystemComponent::AddAbilityToActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability)
{
	if (!Ability) return;

	auto& List = ActivationGroupAbilities[(int32)Group];
	for (const auto& W : List)
	{
		if (W.Get() == Ability) return; // 중복 방지
	}

	// Blocking이 들어오면 Replaceable을 밀어내기
	if (Group == EAscenduntAbilityActivationGroup::Exclusive_Blocking)
	{
		CancelActivationGroupAbilities(EAscenduntAbilityActivationGroup::Exclusive_Replaceable, Ability);
	}

	List.Add(Ability);
}

void UAscenduntAbilitySystemComponent::RemoveAbilityFromActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability)
{
	auto& List = ActivationGroupAbilities[(int32)Group];
	List.RemoveAll([&](const TWeakObjectPtr<UAscenduntGameplayAbility>& W)
		{
			return !W.IsValid() || (W.Get() == Ability);
		});
}

void UAscenduntAbilitySystemComponent::CancelActivationGroupAbilities(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* IgnoreAbility)
{
	auto& List = ActivationGroupAbilities[(int32)Group];

	for (const TWeakObjectPtr<UAscenduntGameplayAbility>& W : List)
	{
		UAscenduntGameplayAbility* Ability = W.Get();
		if (!Ability || Ability == IgnoreAbility) continue;

		// UAbilitySystemComponent의 CancelAbility(UGameplayAbility*) 사용 가능
		CancelAbility(Ability);
	}
}


// =================== GameplayCue ========================

void UAscenduntAbilitySystemComponent::ExecuteGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	if (!GameplayCueTag.IsValid())
	{
		return;
	}

	AActor* TargetActor = GetAvatarActor();
	if (!TargetActor)
	{
		return;
	}

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(
		TargetActor, GameplayCueTag, EGameplayCueEvent::Executed, GameplayCueParameters);
}

void UAscenduntAbilitySystemComponent::AddGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	if (!GameplayCueTag.IsValid()) return;

	AActor* TargetActor = GetAvatarActor();
	if (!TargetActor) return;

	auto* GCM = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	GCM->HandleGameplayCue(TargetActor, GameplayCueTag, EGameplayCueEvent::OnActive, GameplayCueParameters);
	GCM->HandleGameplayCue(TargetActor, GameplayCueTag, EGameplayCueEvent::WhileActive, GameplayCueParameters);
}

void UAscenduntAbilitySystemComponent::RemoveGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	if (!GameplayCueTag.IsValid()) return;

	AActor* TargetActor = GetAvatarActor();
	if (!TargetActor) return;

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(
		TargetActor, GameplayCueTag, EGameplayCueEvent::Removed, GameplayCueParameters);
}


// ==================== RPC ===========================

void UAscenduntAbilitySystemComponent::Client_ExecuteCueLocal_Implementation(FGameplayTag CueTag, FGameplayCueParameters Params)
{
	ExecuteGameplayCueLocal(CueTag, Params);
}


void UAscenduntAbilitySystemComponent::Client_ExecuteCueLocalOnActor_Implementation(AActor* CueTarget, FGameplayTag CueTag, FGameplayCueParameters Params)
{
	if (!CueTarget || !CueTag.IsValid())
		return;

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(
		CueTarget, CueTag, EGameplayCueEvent::Executed, Params);
}



/*
* =============================================
*  AscRouteCue : GameplayCue 라우팅(전송/실행 방식) 표준화
* =============================================
*
* [핵심 개념]
* - CueTargetASC : "GC를 어떤 액터(MyTarget)에 적용할지" (어디에 붙이냐/누구 몸이냐)
* - InstigatorASC: "누가 원인(가해자/시전자)이냐" (누가 봐야 하는지/누구에게 보낼지 제한할 때 필요)
*
* - Route는 "누가(어느 머신에서) GC를 실행/전달받느냐(전송/실행 방식)"를 결정한다.
*   (서버에서 복제, 내 로컬, 타깃 오너만, 가해자 오너만, 가해자 오너만+타깃은 피해자 등)
*
* [중요 규칙]
* - 서버 확정 결과(피격/데미지/버프/디버프 등)를 LocalOnly로 호출하면:
*   데디 서버에서는 아무도 못 보고, 리슨 서버에서는 호스트만 보는 "이상한 결과"가 된다.
*   => 서버에서 확정되는 사건은 ReplicatedToRelevant / TargetOwnerOnly / InstigatorOnly / InstigatorOnly_CueTarget 중 선택.
*
* - 클라에서 즉시 보여줘야 하는 것(예측 머즐/반동, 로컬 UI 등)은 LocalOnly로 실행한다.
*
* - 서버 권한(HasAuthority) 체크는 이 함수 내부에서 수행하므로,
*   호출부에서 매번 HasAuthority/IsLocallyControlled를 흩뿌릴 필요가 없도록 한다.
*
* =========================================================
*  Route 사용 상황 (전송/실행 방식) : "누가 받는가"를 크게 결정
* =========================================================
* - ReplicatedToRelevant:
*     서버 확정 사건을 relevancy 되는 모두에게 보여줄 때(피격 VFX, 버프/디버프, 월드 임팩트,  사운드 등).
* - LocalOnly:
*     내 머신에서만 즉시 보여줘야 할 때(예측 머즐/반동, 로컬 UI, 디버그/연습용 연출).
* - TargetOwnerOnly:
*     타깃 본인(그 ASC 오너)만 봐야 할 때(피격 화면 플래시/카메라 흔들림/개인 경고 UI 등).
* - InstigatorOnly:
*     공격자 본인만 봐야 하고 "타깃에 붙을 필요가 없는" 경우(히트마커, 딜 숫자 UI, 명중 사운드 등).
*     ※ 이 Route는 보통 Instigator(공격자) 아바타 기준으로 실행되며,
*        피해자 몸에 붙는 VFX를 만들 수는 없다.
* - InstigatorOnly_CueTarget:
*     공격자 본인만 봐야 하지만, 이펙트는 피해자(=CueTargetASC)의 몸(MyTarget)에 붙어야 하는 경우.
*     예) "B가 불탐/표식/디버프"를 A만 확인하는 특수 시야/스캔 연출.
*
* =========================================================
*  Audience(수신측 필터)와의 관계
* =========================================================
* - Route가 "누가 받는가"를 먼저 결정한다. (전송/실행 방식)
* - Audience는 "받은 뒤에 그 클라에서 재생할지 말지"를 결정한다. (GCN Base에서 필터)
*
* [권장 운영]
* - 기본적으로:
*     - ReplicatedToRelevant를 쓸 때만 Audience로 (타깃 제외/가해자 제외 등) 가시성을 세밀하게 조정한다.
*     - TargetOwnerOnly / InstigatorOnly / InstigatorOnly_CueTarget 처럼 "수신자가 이미 1명"으로 제한된 Route에서는
*       Audience는 가급적 AllRelevant(또는 무해한 값)로 둔다. (불필요한 필터로 재생이 막히는 사고 방지)
*
* [주의]
* - TargetOwnerOnly + Audience=AllExceptTarget 같이 "타깃 제외" 필터를 켜면,
*   타깃 오너 클라에서 스스로를 제외해서 아무 것도 안 나올 수 있다.
*/
void UAscenduntAbilitySystemComponent::AscRouteCue(
	/** 누가 원인(가해자/시전자)이냐 */
	UAscenduntAbilitySystemComponent* InstigatorASC,
	/** 누가 결과(피해자/수혜자)냐 + VFX를 어디 기준으로 붙일지 */
	UAscenduntAbilitySystemComponent* CueTargetASC,
	const FGameplayTag CueTag,
	const FGameplayCueParameters& Params,
	EAscenduntCueRoute Route)
{
	// 서버 여부는 "있는 쪽"으로 판정
	AActor* AnyAvatar = nullptr;
	if (CueTargetASC)     AnyAvatar = CueTargetASC->GetAvatarActor();
	if (!AnyAvatar && InstigatorASC) AnyAvatar = InstigatorASC->GetAvatarActor();
	const bool bIsServer = AnyAvatar && AnyAvatar->HasAuthority();

	switch (Route)
	{
	case EAscenduntCueRoute::ReplicatedToRelevant:
		// "누구 몸에 붙이냐"가 필요 -> CueTargetASC 필수
		if (bIsServer && CueTargetASC)
		{
			CueTargetASC->ExecuteGameplayCue(CueTag, Params);
		}
		break;

	case EAscenduntCueRoute::LocalOnly:
		// 로컬에서도 "타깃(누구 몸)"은 필요 -> CueTargetASC(또는 InstigatorASC를 넘겨서 자기 몸)
		if (CueTargetASC)
		{
			CueTargetASC->ExecuteGameplayCueLocal(CueTag, Params);
		}
		break;

	case EAscenduntCueRoute::TargetOwnerOnly:
		// Victim의 owning client에게만
		if (bIsServer && CueTargetASC)
		{
			CueTargetASC->Client_ExecuteCueLocal(CueTag, Params);
		}
		break;

	case EAscenduntCueRoute::InstigatorOnly:
		// Instigator의 owning client에게만 (CueTargetASC 없어도 됨)
		if (bIsServer && InstigatorASC)
		{
			InstigatorASC->Client_ExecuteCueLocal(CueTag, Params);
		}
		break;

	case EAscenduntCueRoute::InstigatorOnly_CueTarget:
		if (bIsServer && InstigatorASC && CueTargetASC)
		{
			AActor* CueTargetAvatar = CueTargetASC->GetAvatarActor();
			if (CueTargetAvatar)
			{
				InstigatorASC->Client_ExecuteCueLocalOnActor(CueTargetAvatar, CueTag, Params);
			}
		}
		break;
	}
}
