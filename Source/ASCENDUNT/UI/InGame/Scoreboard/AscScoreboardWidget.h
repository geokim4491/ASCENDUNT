#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCENDUNT/UI/InGame/Scoreboard/AscScoreboardTypes.h"
#include "AscScoreboardWidget.generated.h"

class AAscenduntPlayerController;
class AAscenduntPlayerState;
class UAscScoreboardPlayerRowWidget;
class UPanelWidget;
class UTextBlock;

/** Local presentation widget for the hold-TAB PvE scoreboard. */
UCLASS()
class ASCENDUNT_API UAscScoreboardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Scoreboard")
    void InitializeScoreboard(AAscenduntPlayerController* InOwningPC);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Scoreboard")
    void SetScoreboardVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Scoreboard")
    void RefreshScoreboard();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Scoreboard")
    TArray<FAscScoreboardPlayerEntry> GetLastEntries() const { return LastEntries; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Scoreboard")
    FAscScoreboardLocalWeaponStats GetLocalWeaponStats() const { return LocalWeaponStats; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Called after C++ has refreshed both the row data and local right-side stats. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Scoreboard")
    void BP_OnScoreboardRefreshed();

    void BuildEntries(TArray<FAscScoreboardPlayerEntry>& OutEntries, FAscScoreboardLocalWeaponStats& OutLocalStats) const;
    void RefreshRows(const TArray<FAscScoreboardPlayerEntry>& Entries);
    FAscScoreboardPlayerEntry BuildEntry(AAscenduntPlayerState* PlayerState) const;

protected:
    /** VerticalBox / ScrollBox / other panel that receives one player-row widget per PlayerState. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    TObjectPtr<UPanelWidget> Panel_PlayerRows;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    TObjectPtr<UTextBlock> Txt_WeaponAccuracy;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    TObjectPtr<UTextBlock> Txt_CriticalHitAccuracy;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    TSubclassOf<UAscScoreboardPlayerRowWidget> PlayerRowWidgetClass;

    /** Only runs while TAB is held. 10 Hz is plenty for scoreboard values. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Scoreboard", meta = (ClampMin = "0.02"))
    float RefreshInterval = 0.10f;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    TArray<FAscScoreboardPlayerEntry> LastEntries;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    FAscScoreboardLocalWeaponStats LocalWeaponStats;

private:
    UPROPERTY(Transient)
    TObjectPtr<AAscenduntPlayerController> OwningAscPC = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UAscScoreboardPlayerRowWidget>> SpawnedRows;

    float RefreshAccumulator = 0.0f;
};
