#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "AscTimerConditionComponent.generated.h"

UCLASS(Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscTimerConditionComponent : public UAscObjectiveConditionComponent
{
    GENERATED_BODY()

public:
    virtual float GetCurrentProgressValue_Implementation() const override;
    virtual float GetTargetProgressValue_Implementation() const override;

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual void OnReset() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Timer", meta = (ClampMin = "0.0"))
    float RequiredDuration = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Timer", meta = (ClampMin = "0.05"))
    float ProgressUpdateInterval = 0.25f;

private:
    void HandleProgressTimer();
    float GetElapsedTime() const;

    double StartTimeSeconds = 0.0;
    bool bTimerStarted = false;
    FTimerHandle ProgressTimerHandle;
};
