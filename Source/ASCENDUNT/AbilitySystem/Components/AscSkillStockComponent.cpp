#include "AscSkillStockComponent.h"

#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"

UAscSkillStockComponent::UAscSkillStockComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAscSkillStockComponent::RegisterStock(FGameplayTag StockTag, int32 MaxStock, float RecoverTime, EAscStockRecoverMode RecoverMode, bool bFillCurrentStock)
{
	if (!StockTag.IsValid() || MaxStock <= 0 || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (FAscSkillStockEntry* Entry = FindEntryMutable(StockTag))
	{
		Entry->MaxStock = MaxStock;
		Entry->RecoverTime = RecoverTime;
		Entry->RecoverMode = RecoverMode;
		SetCurrentStock(*Entry, FMath::Clamp(Entry->CurrentStock, 0, MaxStock));
		MarkStockStateDirty();
		return;
	}

	FAscSkillStockEntry NewEntry;
	NewEntry.StockTag = StockTag;
	NewEntry.MaxStock = MaxStock;
	NewEntry.CurrentStock = MaxStock;
	NewEntry.RecoverTime = RecoverTime;
	NewEntry.RecoverMode = RecoverMode;
	StockEntries.Add(NewEntry);
	MarkStockStateDirty();
}

bool UAscSkillStockComponent::CanConsumeStock(FGameplayTag StockTag, int32 Amount) const
{
	if (Amount <= 0)
	{
		return true;
	}

	const FAscSkillStockEntry* Entry = FindEntry(StockTag);
	return Entry && Entry->CurrentStock >= Amount;
}

bool UAscSkillStockComponent::TryConsumeStock(FGameplayTag StockTag, int32 Amount, bool bStartRecoverTimer)
{
	if (Amount <= 0)
	{
		return true;
	}
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return CanConsumeStock(StockTag, Amount);
	}

	FAscSkillStockEntry* Entry = FindEntryMutable(StockTag);
	if (!Entry || Entry->CurrentStock < Amount)
	{
		return false;
	}

	SetCurrentStock(*Entry, Entry->CurrentStock - Amount);

	if (bStartRecoverTimer && Entry->CurrentStock < Entry->MaxStock)
	{
		StartRecoverTimer(*Entry);
	}

	return true;
}

int32 UAscSkillStockComponent::GetCurrentStock(FGameplayTag StockTag) const
{
	const FAscSkillStockEntry* Entry = FindEntry(StockTag);
	return Entry ? Entry->CurrentStock : 0;
}

int32 UAscSkillStockComponent::GetMaxStock(FGameplayTag StockTag) const
{
	const FAscSkillStockEntry* Entry = FindEntry(StockTag);
	return Entry ? Entry->MaxStock : 0;
}

bool UAscSkillStockComponent::HasRecoverTimer(FGameplayTag StockTag) const
{
	const FAscSkillStockEntry* Entry = FindEntry(StockTag);
	if (Entry && Entry->RecoverEndServerTime > GetSynchronizedServerTimeSeconds() + KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FTimerHandle* Handle = RecoverTimers.Find(StockTag);
	return Handle && GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(*Handle);
}

bool UAscSkillStockComponent::GetRecoverState(FGameplayTag StockTag, float& TimeRemaining, float& RecoverDuration) const
{
	TimeRemaining = 0.0f;
	RecoverDuration = 0.0f;

	const FAscSkillStockEntry* Entry = FindEntry(StockTag);
	if (!Entry || Entry->RecoverTime <= 0.0f || Entry->RecoverEndServerTime <= 0.0f)
	{
		return false;
	}

	RecoverDuration = Entry->RecoverTime;
	TimeRemaining = FMath::Max(0.0f, Entry->RecoverEndServerTime - GetSynchronizedServerTimeSeconds());
	return TimeRemaining > KINDA_SMALL_NUMBER;
}

FAscSkillStockEntry* UAscSkillStockComponent::FindEntryMutable(FGameplayTag StockTag)
{
	return StockEntries.FindByPredicate([StockTag](const FAscSkillStockEntry& Entry)
	{
		return Entry.StockTag == StockTag;
	});
}

const FAscSkillStockEntry* UAscSkillStockComponent::FindEntry(FGameplayTag StockTag) const
{
	return StockEntries.FindByPredicate([StockTag](const FAscSkillStockEntry& Entry)
	{
		return Entry.StockTag == StockTag;
	});
}

void UAscSkillStockComponent::SetCurrentStock(FAscSkillStockEntry& Entry, int32 NewStock)
{
	const int32 OldStock = Entry.CurrentStock;
	Entry.CurrentStock = FMath::Clamp(NewStock, 0, Entry.MaxStock);

	if (OldStock != Entry.CurrentStock)
	{
		OnStockChanged.Broadcast(Entry.StockTag, OldStock, Entry.CurrentStock);
		MarkStockStateDirty();
	}
}

void UAscSkillStockComponent::StartRecoverTimer(FAscSkillStockEntry& Entry)
{
	if (!GetWorld() || Entry.RecoverTime <= 0.0f || HasRecoverTimer(Entry.StockTag))
	{
		return;
	}

	Entry.RecoverEndServerTime = GetSynchronizedServerTimeSeconds() + Entry.RecoverTime;
	MarkStockStateDirty();

	FTimerHandle& Handle = RecoverTimers.FindOrAdd(Entry.StockTag);
	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ThisClass::FinishRecover, Entry.StockTag);
	GetWorld()->GetTimerManager().SetTimer(Handle, Delegate, Entry.RecoverTime, false);
}

void UAscSkillStockComponent::FinishRecover(FGameplayTag StockTag)
{
	RecoverTimers.Remove(StockTag);

	FAscSkillStockEntry* Entry = FindEntryMutable(StockTag);
	if (!Entry)
	{
		return;
	}

	Entry->RecoverEndServerTime = 0.0f;

	if (Entry->RecoverMode == EAscStockRecoverMode::RefillToMax)
	{
		SetCurrentStock(*Entry, Entry->MaxStock);
		MarkStockStateDirty();
		return;
	}

	SetCurrentStock(*Entry, Entry->CurrentStock + 1);
	if (Entry->CurrentStock < Entry->MaxStock)
	{
		StartRecoverTimer(*Entry);
	}
	else
	{
		MarkStockStateDirty();
	}
}

float UAscSkillStockComponent::GetSynchronizedServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void UAscSkillStockComponent::MarkStockStateDirty()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UAscSkillStockComponent::OnRep_StockEntries()
{
	for (const FAscSkillStockEntry& Entry : StockEntries)
	{
		OnStockChanged.Broadcast(Entry.StockTag, INDEX_NONE, Entry.CurrentStock);
	}
}

void UAscSkillStockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAscSkillStockComponent, StockEntries);
}
