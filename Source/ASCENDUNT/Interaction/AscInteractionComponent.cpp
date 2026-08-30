#include "AscInteractionComponent.h"

#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/Interaction/AscInteractable.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UAscInteractionComponent::UAscInteractionComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.10f;
}

void UAscInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UAscInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    EndLocalInteraction();
    ClearFocusedInteractable();
    Super::EndPlay(EndPlayReason);
}

void UAscInteractionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AHeroCharacter* Hero = GetHeroOwner();
    if (!Hero || !Hero->IsLocallyControlled())
    {
        return;
    }

    // While a Hold is active, keep the current prompt/progress stable.
    if (bLocalInteractionActive)
    {
        return;
    }

    if (const UAscPvEDownedComponent* Downed = Hero->GetPvEDownedComponent())
    {
        if (Downed->IsDowned())
        {
            ClearFocusedInteractable();
            return;
        }
    }

    RefreshFocusedInteractable();
}

AHeroCharacter* UAscInteractionComponent::GetHeroOwner() const
{
    return Cast<AHeroCharacter>(GetOwner());
}

void UAscInteractionComponent::GetInteractionView(
    FVector& OutViewLocation,
    FVector& OutViewDirection,
    float& OutFOVDegrees) const
{
    AHeroCharacter* Hero = GetHeroOwner();
    if (!Hero)
    {
        OutViewLocation = FVector::ZeroVector;
        OutViewDirection = FVector::ForwardVector;
        OutFOVDegrees = FallbackInteractionFOVDegrees;
        return;
    }

    FRotator EyeRotation = Hero->GetActorRotation();
    Hero->GetActorEyesViewPoint(OutViewLocation, EyeRotation);
    OutViewDirection = EyeRotation.Vector();
    OutFOVDegrees = FallbackInteractionFOVDegrees;

    if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
    {
        // On the owning client, use the actual rendered camera FOV/location/rotation.
        if (PC->IsLocalController() && PC->PlayerCameraManager)
        {
            OutViewLocation = PC->PlayerCameraManager->GetCameraLocation();
            OutViewDirection = PC->PlayerCameraManager->GetCameraRotation().Vector();
            OutFOVDegrees = PC->PlayerCameraManager->GetFOVAngle();
        }
        else
        {
            // Dedicated server has no local camera; control rotation is the authoritative fallback.
            OutViewDirection = PC->GetControlRotation().Vector();
        }
    }

    OutViewDirection = OutViewDirection.GetSafeNormal();
    OutFOVDegrees = FMath::Clamp(OutFOVDegrees, 1.0f, 179.0f);
}

FVector UAscInteractionComponent::ResolveClosestInteractionPoint(
    AActor* TargetActor,
    const FVector& QueryPoint) const
{
    if (!TargetActor)
    {
        return FVector::ZeroVector;
    }

    FVector BestPoint = TargetActor->GetActorLocation();
    float BestDistanceSq = TNumericLimits<float>::Max();

    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
    TargetActor->GetComponents(PrimitiveComponents);

    for (const UPrimitiveComponent* Primitive : PrimitiveComponents)
    {
        if (!Primitive || !Primitive->IsRegistered() ||
            Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        FVector ClosestPoint = FVector::ZeroVector;
        const float Distance = Primitive->GetClosestPointOnCollision(QueryPoint, ClosestPoint);
        if (Distance >= 0.0f)
        {
            const float DistanceSq = FVector::DistSquared(QueryPoint, ClosestPoint);
            if (DistanceSq < BestDistanceSq)
            {
                BestDistanceSq = DistanceSq;
                BestPoint = ClosestPoint;
            }
        }
    }

    if (BestDistanceSq < TNumericLimits<float>::Max())
    {
        return BestPoint;
    }

    const FBox Bounds = TargetActor->GetComponentsBoundingBox(/*bNonColliding=*/false);
    if (Bounds.IsValid)
    {
        return Bounds.GetClosestPointTo(QueryPoint);
    }

    return TargetActor->GetActorLocation();
}

bool UAscInteractionComponent::IsInsideInteractionFOV(AActor* TargetActor, float* OutViewDot) const
{
    if (!TargetActor)
    {
        return false;
    }

    FVector ViewLocation;
    FVector ViewDirection;
    float FOVDegrees = FallbackInteractionFOVDegrees;
    GetInteractionView(ViewLocation, ViewDirection, FOVDegrees);

    FVector BoundsOrigin = TargetActor->GetActorLocation();
    FVector BoundsExtent = FVector::ZeroVector;
    TargetActor->GetActorBounds(/*bOnlyCollidingComponents=*/false, BoundsOrigin, BoundsExtent);

    const FVector ToCenter = BoundsOrigin - ViewLocation;
    const float Distance = ToCenter.Size();
    if (Distance <= KINDA_SMALL_NUMBER)
    {
        if (OutViewDot)
        {
            *OutViewDot = 1.0f;
        }
        return true;
    }

    const FVector Direction = ToCenter / Distance;
    const float ViewDot = FVector::DotProduct(ViewDirection, Direction);
    if (OutViewDot)
    {
        *OutViewDot = ViewDot;
    }

    // Treat the actor bounds as a sphere so an object is accepted when any meaningful
    // part of it enters the camera FOV, not only when its pivot/center is inside.
    const float BoundingRadius = BoundsExtent.Size();
    if (BoundingRadius >= Distance)
    {
        return true;
    }

    const float HalfFOVRadians = FMath::DegreesToRadians(FOVDegrees * 0.5f);
    const float AngularRadius = BoundingRadius > KINDA_SMALL_NUMBER
        ? FMath::Asin(FMath::Clamp(BoundingRadius / Distance, 0.0f, 1.0f))
        : 0.0f;
    const float ViewAngle = FMath::Acos(FMath::Clamp(ViewDot, -1.0f, 1.0f));

    return ViewAngle <= (HalfFOVRadians + AngularRadius);
}

AActor* UAscInteractionComponent::FindBestInteractable(FAscInteractionOption* OutOption) const
{
    AHeroCharacter* Hero = GetHeroOwner();
    UWorld* World = GetWorld();
    if (!Hero || !World)
    {
        LastInteractionOverlapCount = 0;
        LastInteractionDebugReason = TEXT("Missing Hero owner or World");
        return nullptr;
    }

    LastInteractionOverlapCount = 0;
    LastInteractionDebugReason = TEXT("Overlaps found, but no valid IAscInteractable passed validation");

    const FCollisionObjectQueryParams ObjectQuery(FCollisionObjectQueryParams::AllObjects);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AscInteractionSearch), false, Hero);

    TArray<FOverlapResult> Overlaps;
    const bool bAnyOverlap = World->OverlapMultiByObjectType(
        Overlaps,
        Hero->GetActorLocation(),
        FQuat::Identity,
        ObjectQuery,
        FCollisionShape::MakeSphere(InteractionSearchRadius),
        QueryParams);

    LastInteractionOverlapCount = Overlaps.Num();
    if (!bAnyOverlap)
    {
        LastInteractionDebugReason = TEXT("No collision overlaps inside InteractionSearchRadius");
        return nullptr;
    }

    TSet<AActor*> VisitedActors;
    AActor* BestTarget = nullptr;
    FAscInteractionOption BestOption;
    float BestScore = -TNumericLimits<float>::Max();

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        if (!Candidate || Candidate == Hero || VisitedActors.Contains(Candidate))
        {
            continue;
        }
        VisitedActors.Add(Candidate);

        FAscInteractionOption CandidateOption;
        if (!ValidateInteractable(Candidate, &CandidateOption))
        {
            continue;
        }

        const FVector ClosestPoint = ResolveClosestInteractionPoint(Candidate, Hero->GetActorLocation());
        const float Distance = FVector::Distance(Hero->GetActorLocation(), ClosestPoint);

        float ViewDot = 0.0f;
        IsInsideInteractionFOV(Candidate, &ViewDot); // Already validated; used only for tie-breaking.

        const float Score = (CandidateOption.Priority * 10000.0f) + (ViewDot * 500.0f) - Distance;
        if (!BestTarget || Score > BestScore)
        {
            BestTarget = Candidate;
            BestOption = CandidateOption;
            BestScore = Score;
        }
    }

    if (BestTarget)
    {
        LastInteractionDebugReason = FString::Printf(TEXT("Focused %s"), *GetNameSafe(BestTarget));
        if (OutOption)
        {
            *OutOption = BestOption;
        }
    }

    return BestTarget;
}

bool UAscInteractionComponent::ValidateInteractable(AActor* TargetActor, FAscInteractionOption* OutOption) const
{
    AHeroCharacter* Hero = GetHeroOwner();
    if (!Hero || !IsValid(TargetActor) || TargetActor == Hero)
    {
        LastInteractionDebugReason = TEXT("Rejected invalid/self target");
        return false;
    }

    if (!TargetActor->GetClass()->ImplementsInterface(UAscInteractable::StaticClass()))
    {
        LastInteractionDebugReason = FString::Printf(
            TEXT("%s overlaps search sphere but does not implement AscInteractable"),
            *GetNameSafe(TargetActor));
        return false;
    }

    if (!IAscInteractable::Execute_CanInteract(TargetActor, Hero))
    {
        LastInteractionDebugReason = FString::Printf(
            TEXT("%s implements AscInteractable but CanInteract returned false"),
            *GetNameSafe(TargetActor));
        return false;
    }

    const FAscInteractionOption Option =
        IAscInteractable::Execute_GetInteractionOption(TargetActor, Hero);

    const FVector ClosestPoint = ResolveClosestInteractionPoint(TargetActor, Hero->GetActorLocation());
    const float MaxDistance = FMath::Max(1.0f, Option.MaxDistance);
    const float Distance = FVector::Distance(Hero->GetActorLocation(), ClosestPoint);
    if (Distance > MaxDistance)
    {
        LastInteractionDebugReason = FString::Printf(
            TEXT("%s rejected by range: %.1f > %.1f"),
            *GetNameSafe(TargetActor), Distance, MaxDistance);
        return false;
    }

    if (!IsInsideInteractionFOV(TargetActor))
    {
        LastInteractionDebugReason = FString::Printf(
            TEXT("%s rejected because it is outside interaction FOV"),
            *GetNameSafe(TargetActor));
        return false;
    }

    if (OutOption)
    {
        *OutOption = Option;
    }

    return true;
}

void UAscInteractionComponent::RefreshFocusedInteractable()
{
    FAscInteractionOption NewOption;
    AActor* NewTarget = FindBestInteractable(&NewOption);

    if (!NewTarget)
    {
        ClearFocusedInteractable();
        return;
    }

    SetFocusedInteractable(NewTarget, NewOption);
}

void UAscInteractionComponent::SetFocusedInteractable(
    AActor* NewTarget,
    const FAscInteractionOption& NewOption)
{
    const bool bChanged =
        FocusedInteractable.Get() != NewTarget ||
        !FocusedOption.PromptText.EqualTo(NewOption.PromptText) ||
        FocusedOption.InputMode != NewOption.InputMode ||
        !FMath::IsNearlyEqual(FocusedOption.HoldDuration, NewOption.HoldDuration);

    FocusedInteractable = NewTarget;
    FocusedOption = NewOption;

    if (bChanged)
    {
        OnFocusedInteractionChanged.Broadcast(
            NewTarget,
            NewOption.PromptText,
            NewOption.InputMode,
            FMath::Max(0.01f, NewOption.HoldDuration));
    }
}

void UAscInteractionComponent::ClearFocusedInteractable()
{
    if (!FocusedInteractable.IsValid() && FocusedOption.PromptText.IsEmpty())
    {
        return;
    }

    FocusedInteractable.Reset();
    FocusedOption = FAscInteractionOption();
    OnFocusedInteractionChanged.Broadcast(
        nullptr,
        FText::GetEmpty(),
        EAscInteractionInputMode::Pressed,
        0.0f);
}

void UAscInteractionComponent::BeginLocalInteraction(
    AActor* TargetActor,
    const FAscInteractionOption& Option)
{
    AHeroCharacter* Hero = GetHeroOwner();
    if (!Hero || !Hero->IsLocallyControlled())
    {
        return;
    }

    bLocalInteractionActive = true;
    LocalActiveTarget = TargetActor;
    LocalActiveOption = Option;
    SetFocusedInteractable(TargetActor, Option);
    OnInteractionProgressChanged.Broadcast(0.0f, true);
}

void UAscInteractionComponent::UpdateLocalInteractionProgress(float Progress)
{
    if (!bLocalInteractionActive)
    {
        return;
    }

    OnInteractionProgressChanged.Broadcast(FMath::Clamp(Progress, 0.0f, 1.0f), true);
}

void UAscInteractionComponent::EndLocalInteraction()
{
    if (!bLocalInteractionActive)
    {
        return;
    }

    bLocalInteractionActive = false;
    LocalActiveTarget.Reset();
    LocalActiveOption = FAscInteractionOption();
    OnInteractionProgressChanged.Broadcast(0.0f, false);

    if (AHeroCharacter* Hero = GetHeroOwner())
    {
        if (Hero->IsLocallyControlled())
        {
            RefreshFocusedInteractable();
        }
    }
}
