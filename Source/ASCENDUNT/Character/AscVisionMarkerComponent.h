// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/WidgetComponent.h"
#include "AscVisionMarkerComponent.generated.h"

class APlayerController;
class UUserWidget;

UCLASS(ClassGroup = (Ascendunt), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UAscVisionMarkerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscVisionMarkerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Vision")
	void SetCriticalVisible(bool bVisible);

	void SetCriticalVisibleForViewer(bool bVisible, APlayerController* ViewPlayerController);

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Vision")
	bool IsCriticalVisible() const { return bCriticalVisible; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Vision")
	UWidgetComponent* GetCriticalWidgetComponent() const { return CriticalWidgetComponent; }

private:
	// 치명상 위젯 생성과 거리 기반 크기 갱신
	void EnsureCriticalWidgetCreated();
	void UpdateCriticalScale();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	TSubclassOf<UUserWidget> CriticalWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	FVector RelativeLocation = FVector(0.f, 0.f, 110.f);

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	EWidgetSpace WidgetSpace = EWidgetSpace::Screen;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision|Critical")
	float ScaleNearDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision|Critical")
	float ScaleFarDistance = 2200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision|Critical")
	float ScaleMax = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision|Critical")
	float ScaleMin = 0.3f;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> CriticalWidgetComponent;

	TWeakObjectPtr<APlayerController> CachedViewPlayerController;
	bool bCriticalVisible = false;
};
