#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AscObjectiveArea.generated.h"

class APawn;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FAscObjectiveAreaPawnEvent,
    AAscObjectiveArea*, Area,
    APawn*, PlayerPawn);

/** Lightweight level trigger used by reach-area conditions. */
UCLASS(Blueprintable)
class ASCENDUNT_API AAscObjectiveArea : public AActor
{
    GENERATED_BODY()

public:
    AAscObjectiveArea();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Area")
    int32 GetPlayerCountInside() const;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Mission|Area")
    bool IsPlayerInside(APawn* PlayerPawn) const;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Area")
    FAscObjectiveAreaPawnEvent OnPlayerEntered;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Mission|Area")
    FAscObjectiveAreaPawnEvent OnPlayerExited;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Area")
    TObjectPtr<UBoxComponent> AreaBox;

private:
    UFUNCTION()
    void HandleBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    TSet<TWeakObjectPtr<APawn>> PlayerPawnsInside;
};
