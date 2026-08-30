#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASCENDUNT/Interaction/AscInteractionTypes.h"
#include "AscInteractionComponent.generated.h"

class AHeroCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FAscFocusedInteractionChanged,
    AActor*, TargetActor,
    FText, PromptText,
    EAscInteractionInputMode, InputMode,
    float, HoldDuration);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FAscInteractionProgressChanged,
    float, Progress,
    bool, bIsInteracting);

/**
 * Local focus/query component used by the common interaction GameplayAbility.
 *
 * - Owning client periodically finds the best nearby IAscInteractable for HUD.
 * - Candidates must be inside the interaction FOV; there is no LOS trace.
 * - Interaction point is resolved internally from collision/bounds. BP targets do not provide a location.
 * - This component does not execute gameplay state changes; target callbacks are server-only.
 */
UCLASS(ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAscInteractionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Interaction")
    AActor* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Interaction")
    FAscInteractionOption GetFocusedInteractionOption() const { return FocusedOption; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Interaction")
    bool IsLocallyInteracting() const { return bLocalInteractionActive; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Interaction|Debug")
    int32 GetLastInteractionOverlapCount() const { return LastInteractionOverlapCount; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Interaction|Debug")
    FString GetLastInteractionDebugReason() const { return LastInteractionDebugReason; }

    /** Performs an immediate world query. Safe to call on owning client or authority. */
    AActor* FindBestInteractable(FAscInteractionOption* OutOption = nullptr) const;

    /** Re-validates a specific target against interface, range and FOV policy. */
    bool ValidateInteractable(AActor* TargetActor, FAscInteractionOption* OutOption = nullptr) const;

    /** Local presentation hooks called by GA_Interact. */
    void BeginLocalInteraction(AActor* TargetActor, const FAscInteractionOption& Option);
    void UpdateLocalInteractionProgress(float Progress);
    void EndLocalInteraction();

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Interaction")
    FAscFocusedInteractionChanged OnFocusedInteractionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Interaction")
    FAscInteractionProgressChanged OnInteractionProgressChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void RefreshFocusedInteractable();
    void SetFocusedInteractable(AActor* NewTarget, const FAscInteractionOption& NewOption);
    void ClearFocusedInteractable();

    FVector ResolveClosestInteractionPoint(AActor* TargetActor, const FVector& QueryPoint) const;
    bool IsInsideInteractionFOV(AActor* TargetActor, float* OutViewDot = nullptr) const;
    void GetInteractionView(FVector& OutViewLocation, FVector& OutViewDirection, float& OutFOVDegrees) const;
    AHeroCharacter* GetHeroOwner() const;

private:
    /** Broad-phase sphere. Individual targets can use a smaller MaxDistance. */
    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Interaction", meta = (ClampMin = "10.0"))
    float InteractionSearchRadius = 300.0f;

    /** Server/fallback interaction FOV when a local PlayerCameraManager FOV is unavailable. */
    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Interaction", meta = (ClampMin = "1.0", ClampMax = "179.0"))
    float FallbackInteractionFOVDegrees = 100.0f;

    TWeakObjectPtr<AActor> FocusedInteractable;
    FAscInteractionOption FocusedOption;

    TWeakObjectPtr<AActor> LocalActiveTarget;
    FAscInteractionOption LocalActiveOption;
    bool bLocalInteractionActive = false;

    mutable int32 LastInteractionOverlapCount = 0;
    mutable FString LastInteractionDebugReason;
};
