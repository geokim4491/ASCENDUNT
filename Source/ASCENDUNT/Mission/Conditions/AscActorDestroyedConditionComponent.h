#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "AscActorDestroyedConditionComponent.generated.h"

UCLASS(Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscActorDestroyedConditionComponent : public UAscObjectiveConditionComponent
{
    GENERATED_BODY()

protected:
    virtual void OnActivated() override;
    virtual void OnDeactivated() override;
    virtual void OnReset() override;

    /** Commonly used with ConditionRole=Failure: destruction satisfies the failure trigger. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Actor")
    TObjectPtr<AActor> TargetActor;

private:
    UFUNCTION()
    void HandleTargetDestroyed(AActor* DestroyedActor);
};
