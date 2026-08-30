// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscMainMenuPageWidgetBase.generated.h"

class UAscMainMenuWidget;

/**
 * 
 */
UCLASS()
class ASCENDUNT_API UAscMainMenuPageWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
    virtual void InitializePage(UAscMainMenuWidget* InOwnerMainMenu);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|MainMenu")
    UAscMainMenuWidget* GetOwnerMainMenu() const { return OwnerMainMenu; }

    virtual void OnPageActivated();
    virtual void OnPageDeactivated();

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|MainMenu")
    TObjectPtr<UAscMainMenuWidget> OwnerMainMenu = nullptr;
};
