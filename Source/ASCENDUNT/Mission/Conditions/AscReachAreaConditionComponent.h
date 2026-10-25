#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "AscReachAreaConditionComponent.generated.h"

class AAscObjectiveArea;
class APawn;

UCLASS(Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscReachAreaConditionComponent : public UAscObjectiveConditionComponent
{
    GENERATED_BODY()

public:
    virtual float GetCurrentProgressValue_Implementation() const override;
    virtual float GetTargetProgressValue_Implementation() const override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual void OnReset() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Reach Area")
    TObjectPtr<AAscObjectiveArea> TargetArea;

    /** If true, required count follows the currently connected player count. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Reach Area")
    bool bRequireAllConnectedPlayers = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Reach Area", meta = (EditCondition = "!bRequireAllConnectedPlayers", ClampMin = "1"))
    int32 RequiredPlayerCount = 1;

private:
    UFUNCTION()
    void HandlePlayerEntered(AAscObjectiveArea* Area, APawn* PlayerPawn);

    UFUNCTION()
    void HandlePlayerExited(AAscObjectiveArea* Area, APawn* PlayerPawn);

    void EvaluateArea();
    int32 ResolveRequiredPlayerCount() const;
};
