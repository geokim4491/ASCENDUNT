#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscSettingsPageWidgetBase.generated.h"

/** Minimal common base so the root settings widget can refresh every page uniformly. */
UCLASS(Abstract)
class ASCENDUNT_API UAscSettingsPageWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Settings")
    virtual void RefreshFromSettings();
};
