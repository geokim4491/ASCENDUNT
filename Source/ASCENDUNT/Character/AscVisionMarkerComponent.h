// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/WidgetComponent.h"
#include "AscVisionMarkerComponent.generated.h"

class UUserWidget;

UCLASS(ClassGroup = (Ascendunt), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UAscVisionMarkerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscVisionMarkerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Vision")
	void SetCriticalVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Vision")
	bool IsCriticalVisible() const { return bCriticalVisible; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Vision")
	UWidgetComponent* GetCriticalWidgetComponent() const { return CriticalWidgetComponent; }

protected:
	void EnsureCriticalWidgetCreated();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	TSubclassOf<UUserWidget> CriticalWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	FVector RelativeLocation = FVector(0.f, 0.f, 110.f);

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	EWidgetSpace WidgetSpace = EWidgetSpace::Screen;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> CriticalWidgetComponent;

	bool bCriticalVisible = false;
};
