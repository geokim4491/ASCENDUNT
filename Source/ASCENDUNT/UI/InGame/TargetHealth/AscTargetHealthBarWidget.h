#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/UI/InGame/HealthBar/AscSegmentedHealthBarWidget.h"
#include "AscTargetHealthBarWidget.generated.h"

UENUM(BlueprintType)
enum class EAscTargetHealthBarRelation : uint8
{
    Ally,
    Enemy
};

/** World-space segmented health bar for allies, enemies, and AI targets. */
UCLASS(Blueprintable)
class ASCENDUNT_API UAscTargetHealthBarWidget : public UAscSegmentedHealthBarWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Target Health")
    void SetHealthSnapshot(const FAscenduntHealthSnapshot& InSnapshot);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Target Health")
    void SetRelation(EAscTargetHealthBarRelation InRelation);

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeOnInitialized() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Target Health",
        meta = (DisplayName = "On Health Snapshot Changed"))
    void BP_OnHealthSnapshotChanged(
        const FAscenduntHealthSnapshot& InSnapshot,
        float HealthPercent);

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Target Health",
        meta = (DisplayName = "On Relation Changed"))
    void BP_OnRelationChanged(EAscTargetHealthBarRelation InRelation);

    /** Assign the same centralized DA used by the player health panel. Null uses tuned C++ defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Target Health|Style")
    TObjectPtr<UAscHealthBarStyleSetData> HealthBarStyleSet;

private:
    void ApplyRelationStyle();

    EAscTargetHealthBarRelation CurrentRelation = EAscTargetHealthBarRelation::Enemy;
};
