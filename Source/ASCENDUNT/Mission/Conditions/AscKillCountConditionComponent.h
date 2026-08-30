#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "AscKillCountConditionComponent.generated.h"

class AAISpawner;

UCLASS(Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscKillCountConditionComponent : public UAscObjectiveConditionComponent
{
    GENERATED_BODY()

public:
    virtual float GetCurrentProgressValue_Implementation() const override;
    virtual float GetTargetProgressValue_Implementation() const override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual void OnReset() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Kill", meta = (ClampMin = "1"))
    int32 TargetKillCount = 1;

    /** If empty, bObserveAllSpawnersIfNoneAssigned can bind every AAISpawner in the level. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Kill")
    TArray<TObjectPtr<AAISpawner>> ObservedSpawners;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Kill")
    bool bObserveAllSpawnersIfNoneAssigned = false;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Kill")
    int32 CurrentKillCount = 0;

private:
    void HandleMonsterDied(AActor* DeadActor);
    void BindSpawners();
    void UnbindSpawners();

    UPROPERTY(Transient)
    TArray<TObjectPtr<AAISpawner>> BoundSpawners;
};
