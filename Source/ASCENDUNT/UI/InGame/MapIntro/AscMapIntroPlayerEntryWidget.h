#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscMapIntroPlayerEntryWidget.generated.h"

class UTextBlock;

/** Reusable one-line player-name entry placed inside a VerticalBox. */
UCLASS()
class ASCENDUNT_API UAscMapIntroPlayerEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Map Intro")
    void SetPlayerName(const FString& InPlayerName);

protected:
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TObjectPtr<UTextBlock> Txt_PlayerName;
};
