#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCENDUNT/UI/InGame/Scoreboard/AscScoreboardTypes.h"
#include "AscScoreboardPlayerRowWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UWidget;

/**
 * Optional C++ row base for WBP_ScoreboardPlayerRow.
 * Every BindWidget is optional so the Blueprint can omit columns it does not need.
 */
UCLASS()
class ASCENDUNT_API UAscScoreboardPlayerRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Scoreboard")
    void SetEntry(const FAscScoreboardPlayerEntry& InEntry);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Scoreboard")
    FAscScoreboardPlayerEntry GetEntry() const { return Entry; }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Scoreboard")
    FAscScoreboardPlayerEntry Entry;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UImage> Img_Portrait;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_PlayerName;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_MonsterKills;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_DownedCount;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_ReviveCount;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_DamageDealt;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_HealingDone;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_HealingReceived;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UProgressBar> Progress_Ultimate;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UTextBlock> Txt_UltimatePercent;

    /** Optional highlight root for the owning player's row. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
    TObjectPtr<UWidget> LocalPlayerHighlight;
};
