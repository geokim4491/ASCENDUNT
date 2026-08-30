// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntHealthComponent.h"

#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

UAscenduntHealthComponent::UAscenduntHealthComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	AbilitySystemComponent = nullptr;
	HealthSet = nullptr;
	DeathState = EAscenduntDeathState::NotDead;
}

bool UAscenduntHealthComponent::InitializeWithAbilitySystem(UAscenduntAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (!InASC)
	{
		UE_LOG(LogTemp, Error, TEXT("AscenduntHealthComponent: Cannot initialize owner [%s] with a null ASC."), *GetNameSafe(Owner));
		return false;
	}

	if (AbilitySystemComponent && AbilitySystemComponent != InASC)
	{
		UninitializeFromAbilitySystem();
	}

	AbilitySystemComponent = InASC;
	if (!RefreshHealthSetFromAbilitySystem())
	{
		HealthSet = nullptr;

		// 클라이언트에서는 AttributeSet이 폰보다 늦게 복제될 수 있다.
		// 이때 ASC 참조까지 버리면, 나중에 HealthSet이 도착해도
		// RefreshHealthSetFromAbilitySystem()이 첫 줄에서 막혀 스스로 복구할 수 없다.
		// 그 함수가 담당하는 "지연 재연결"이 동작하려면 참조를 유지해야 한다.
		// 서버는 AbilitySet 부여 직후라 이런 순서 문제가 없으므로 기존대로 실패 처리한다.
		if (InASC->IsOwnerActorAuthoritative())
		{
			AbilitySystemComponent = nullptr;
		}
		return false;
	}

	if (AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		AbilitySystemComponent->SetNumericAttributeBase(UAscenduntHealthSet::GetBasicHealthAttribute(), HealthSet->GetMaxBasicHealth());
		AbilitySystemComponent->SetNumericAttributeBase(UAscenduntHealthSet::GetArmorAttribute(), HealthSet->GetMaxArmor());
		AbilitySystemComponent->SetNumericAttributeBase(UAscenduntHealthSet::GetThornAttribute(), HealthSet->GetMaxThorn());

		// ASC와 새로 연결한다는 것은 새 생명을 시작한다는 뜻이다.
		// 오브젝트 풀 재사용 시 이전 생애의 DeathState가 남아 있으면
		// StartDeath()/FinishDeath()의 가드에 걸려 두 번째 죽음이 아예 처리되지 않는다.
		DeathState = EAscenduntDeathState::NotDead;
		AbilitySystemComponent->SetNumericAttributeBase(UAscenduntHealthSet::GetOverHealthAttribute(), 0.0f);
		AbilitySystemComponent->SetNumericAttributeBase(UAscenduntHealthSet::GetDamageAttribute(), 0.0f);
		AbilitySystemComponent->SetNumericAttributeBase(UAscenduntHealthSet::GetHealingAttribute(), 0.0f);

		Owner->ForceNetUpdate();
	}

	ClearGameplayTags();

	FAscenduntHealthSnapshot Snap;
	Snap.SetFromHealthSet(HealthSet);
	OnHealthChanged.Broadcast(this, Owner, nullptr, false, 0.f, Snap);
	return true;
}

void UAscenduntHealthComponent::UninitializeFromAbilitySystem()
{
	// The ASC can outlive this Pawn (PlayerState-owned hero ASC). Death tags are
	// gameplay lifecycle state, not binding state, so unbinding must not clear them.
	if (HealthSet)
	{
		HealthSet->OnHealthChanged.RemoveAll(this);
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	HealthSet = nullptr;
	AbilitySystemComponent = nullptr;
}

float UAscenduntHealthComponent::GetHealth() const
{
	// Keep the delegate cache synchronized with the persistent ASC-owned HealthSet.
	const_cast<UAscenduntHealthComponent*>(this)->RefreshHealthSetFromAbilitySystem();
	return (HealthSet ? HealthSet->GetTotalHealth() : 0.0f);
}

float UAscenduntHealthComponent::GetMaxHealth() const
{
	const_cast<UAscenduntHealthComponent*>(this)->RefreshHealthSetFromAbilitySystem();
	return (HealthSet ? HealthSet->GetMaxTotalHealth() : 0.0f);
}

float UAscenduntHealthComponent::GetHealthNormalized() const
{
	const_cast<UAscenduntHealthComponent*>(this)->RefreshHealthSetFromAbilitySystem();

	if (HealthSet)
	{
		const float Health = HealthSet->GetTotalHealth();
		const float MaxHealth = HealthSet->GetMaxTotalHealth();

		return ((MaxHealth > 0.0f) ? (Health / MaxHealth) : 0.0f);
	}

	return 0.0f;
}

bool UAscenduntHealthComponent::GetHealthSnapshot(FAscenduntHealthSnapshot& OutSnapshot) const
{
	const_cast<UAscenduntHealthComponent*>(this)->RefreshHealthSetFromAbilitySystem();
	if (!HealthSet)
	{
		OutSnapshot = FAscenduntHealthSnapshot();
		return false;
	}

	OutSnapshot.SetFromHealthSet(HealthSet);
	return true;
}

bool UAscenduntHealthComponent::HasValidHealthSet() const
{
	const_cast<UAscenduntHealthComponent*>(this)->RefreshHealthSetFromAbilitySystem();
	return IsValid(HealthSet);
}

bool UAscenduntHealthComponent::RefreshHealthSetFromAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const UAscenduntHealthSet* CurrentHealthSet = AbilitySystemComponent->GetSet<UAscenduntHealthSet>();
	if (!CurrentHealthSet)
	{
		if (HealthSet)
		{
			HealthSet->OnHealthChanged.RemoveAll(this);
			HealthSet->OnOutOfHealth.RemoveAll(this);
			HealthSet = nullptr;
		}
		return false;
	}

	if (CurrentHealthSet == HealthSet)
	{
		return IsValid(HealthSet);
	}

	// Defensive rebind for ASC replacement or unusual initialization order.
	if (HealthSet)
	{
		HealthSet->OnHealthChanged.RemoveAll(this);
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	HealthSet = CurrentHealthSet;

	if (HealthSet)
	{
		HealthSet->OnHealthChanged.AddUObject(this, &UAscenduntHealthComponent::HandleHealthChanged);
		HealthSet->OnOutOfHealth.AddUObject(this, &UAscenduntHealthComponent::HandleOutOfHealth);
	}

	return IsValid(HealthSet);
}

void UAscenduntHealthComponent::StartDeath()
{
	if (DeathState != EAscenduntDeathState::NotDead)
	{
		return;
	}

	DeathState = EAscenduntDeathState::DeathStarted;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dead, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dying, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathStarted.Broadcast(Owner);

	Owner->ForceNetUpdate();
}

void UAscenduntHealthComponent::FinishDeath()
{
	if (DeathState != EAscenduntDeathState::DeathStarted)
	{
		return;
	}

	DeathState = EAscenduntDeathState::DeathFinished;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dying, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dead, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathFinished.Broadcast(Owner);

	Owner->ForceNetUpdate();
}

bool UAscenduntHealthComponent::ReviveFromDeathStarted()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || DeathState != EAscenduntDeathState::DeathStarted)
	{
		return false;
	}

	// The revive healing must have already restored a valid living pool before the
	// lifecycle state is returned to NotDead.
	if (GetHealth() <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	DeathState = EAscenduntDeathState::NotDead;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dying, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dead, 0);
	}

	Owner->ForceNetUpdate();
	return true;
}

void UAscenduntHealthComponent::DamageSelfDestruct(bool bFellOutOfWorld)
{
}

void UAscenduntHealthComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	UActorComponent::OnUnregister();
}

void UAscenduntHealthComponent::ClearGameplayTags()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dying, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(AscenduntGameplayTags::Status_Death_Dead, 0);
	}
}

void UAscenduntHealthComponent::HandleHealthChanged(AActor* Target, AActor* DamageInstigator, bool bWasDamaing, const FGameplayEffectSpec* DamageEffectSpec, float DamageOrHeal, const FAscenduntHealthSnapshot& HealthSnapshot)
{
	OnHealthChanged.Broadcast(this, Target, DamageInstigator, bWasDamaing, DamageOrHeal, HealthSnapshot);
}

void UAscenduntHealthComponent::HandleOutOfHealth(AActor* Target, AActor* DamageInstigator, const FGameplayEffectSpec* DamageEffectSpec)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 사망 시작만 알린다.
	// FinishDeath()는 사망 연출을 소유한 쪽이 연출을 끝낸 시점에 호출한다.
	StartDeath();
}


void UAscenduntHealthComponent::OnRep_DeathState(EAscenduntDeathState OldDeathState)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (DeathState == EAscenduntDeathState::DeathStarted)
	{
		if (OldDeathState == EAscenduntDeathState::NotDead)
		{
			OnDeathStarted.Broadcast(Owner);
		}
	}
	else if (DeathState == EAscenduntDeathState::DeathFinished)
	{
		if (OldDeathState == EAscenduntDeathState::NotDead)
		{
			OnDeathStarted.Broadcast(Owner);
		}

		if (OldDeathState != EAscenduntDeathState::DeathFinished)
		{
			OnDeathFinished.Broadcast(Owner);
		}
	}
}

void UAscenduntHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAscenduntHealthComponent, DeathState);
}
