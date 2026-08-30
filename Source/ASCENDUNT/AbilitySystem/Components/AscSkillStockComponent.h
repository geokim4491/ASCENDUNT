#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AscSkillStockComponent.generated.h"

UENUM(BlueprintType)
enum class EAscStockRecoverMode : uint8
{
	RefillToMax,
	RecoverOne
};

USTRUCT(BlueprintType)
struct FAscSkillStockEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag StockTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaxStock = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CurrentStock = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float RecoverTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EAscStockRecoverMode RecoverMode = EAscStockRecoverMode::RecoverOne;

	// Absolute synchronized server time when the active recovery finishes.
	// Replicated so autonomous clients can animate the recharge UI without owning the server timer.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RecoverEndServerTime = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAscStockChanged, FGameplayTag, StockTag, int32, OldStock, int32, NewStock);

UCLASS(ClassGroup = (ASCENDUNT), meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UAscSkillStockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscSkillStockComponent();

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|SkillStock")
	void RegisterStock(FGameplayTag StockTag, int32 MaxStock, float RecoverTime, EAscStockRecoverMode RecoverMode, bool bFillCurrentStock = true);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|SkillStock")
	bool CanConsumeStock(FGameplayTag StockTag, int32 Amount = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|SkillStock")
	bool TryConsumeStock(FGameplayTag StockTag, int32 Amount = 1, bool bStartRecoverTimer = true);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|SkillStock")
	int32 GetCurrentStock(FGameplayTag StockTag) const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|SkillStock")
	int32 GetMaxStock(FGameplayTag StockTag) const;

	UFUNCTION(BlueprintPure, Category = "Ascendunt|SkillStock")
	bool HasRecoverTimer(FGameplayTag StockTag) const;

	/** Returns the replicated recharge state used by HUD cooldown overlays. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|SkillStock")
	bool GetRecoverState(FGameplayTag StockTag, float& TimeRemaining, float& RecoverDuration) const;

	UPROPERTY(BlueprintAssignable, Category = "Ascendunt|SkillStock")
	FAscStockChanged OnStockChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_StockEntries();

private:
	FAscSkillStockEntry* FindEntryMutable(FGameplayTag StockTag);
	const FAscSkillStockEntry* FindEntry(FGameplayTag StockTag) const;

	void SetCurrentStock(FAscSkillStockEntry& Entry, int32 NewStock);
	void StartRecoverTimer(FAscSkillStockEntry& Entry);
	void FinishRecover(FGameplayTag StockTag);
	float GetSynchronizedServerTimeSeconds() const;
	void MarkStockStateDirty();

private:
	UPROPERTY(ReplicatedUsing = OnRep_StockEntries)
	TArray<FAscSkillStockEntry> StockEntries;

	TMap<FGameplayTag, FTimerHandle> RecoverTimers;
};
