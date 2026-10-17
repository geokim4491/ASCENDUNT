#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ASCENDUNT/Interaction/AscInteractionTypes.h"
#include "AscInteractable.generated.h"

class AHeroCharacter;
class AActor;
class UWorld;

/** Global server-side notification emitted after GA_Interact completes an interaction. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FAscInteractionCompletedNative,
    UWorld* /* World */,
    AActor* /* Target */,
    AHeroCharacter* /* Interactor */);

/**
 * Lightweight interaction event hub.
 *
 * Interaction targets do not need to expose their own delegate. GA_Interact emits the
 * successful completion here and systems such as mission conditions can observe it.
 */
class ASCENDUNT_API FAscInteractionEvents
{
public:
    static FAscInteractionCompletedNative& OnInteractionCompleted();

    static void BroadcastInteractionCompleted(
        UWorld* World,
        AActor* Target,
        AHeroCharacter* Interactor);
};

/** Implement on actors that can be used by the common GA_Interact ability. */
UINTERFACE(BlueprintType)
class ASCENDUNT_API UAscInteractable : public UInterface
{
    GENERATED_BODY()
};

class ASCENDUNT_API IAscInteractable
{
    GENERATED_BODY()

public:
    /** Whether this actor currently accepts interaction from this hero. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ascendunt|Interaction")
    bool CanInteract(AHeroCharacter* Interactor) const;
    virtual bool CanInteract_Implementation(AHeroCharacter* Interactor) const;

    /** Prompt/input/range policy used by both local HUD and authoritative validation. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ascendunt|Interaction")
    FAscInteractionOption GetInteractionOption(AHeroCharacter* Interactor) const;
    virtual FAscInteractionOption GetInteractionOption_Implementation(AHeroCharacter* Interactor) const;

    /** Server-authoritative callback when a valid interaction begins. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ascendunt|Interaction")
    void BeginInteraction(AHeroCharacter* Interactor);
    virtual void BeginInteraction_Implementation(AHeroCharacter* Interactor);

    /** Server-authoritative callback when a started Hold interaction is cancelled. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ascendunt|Interaction")
    void CancelInteraction(AHeroCharacter* Interactor);
    virtual void CancelInteraction_Implementation(AHeroCharacter* Interactor);

    /** Server-authoritative callback when Pressed fires or Hold reaches 100%. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ascendunt|Interaction")
    void CompleteInteraction(AHeroCharacter* Interactor);
    virtual void CompleteInteraction_Implementation(AHeroCharacter* Interactor);
};
