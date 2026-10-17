#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscDownedIndicatorWidget.generated.h"

class UTextBlock;

/**
 * Minimal world-space widget base for a Downed teammate.
 * The WBP owns the icon/layout; C++ only pushes the remaining bleedout time.
 */
UCLASS()
class ASCENDUNT_API UAscDownedIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|PvE|Downed")
    void SetBleedoutRemainingTime(float RemainingSeconds);

protected:
    /** Optional WBP name: Txt_DownedRemaining. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|PvE|Downed")
    TObjectPtr<UTextBlock> Txt_DownedRemaining;
};
