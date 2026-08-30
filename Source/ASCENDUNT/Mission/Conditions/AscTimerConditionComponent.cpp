#include "ASCENDUNT/Mission/Conditions/AscTimerConditionComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"

float UAscTimerConditionComponent::GetCurrentProgressValue_Implementation() const
{
    return FMath::Clamp(GetElapsedTime(), 0.0f, FMath::Max(0.0f, RequiredDuration));
}

float UAscTimerConditionComponent::GetTargetProgressValue_Implementation() const
{
    return FMath::Max(0.0f, RequiredDuration);
}

void UAscTimerConditionComponent::OnActivated()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        Fail();
        return;
    }

    StartTimeSeconds = World->GetTimeSeconds();
    bTimerStarted = true;

    if (RequiredDuration <= 0.0f)
    {
        Satisfy();
        return;
    }

    World->GetTimerManager().SetTimer(
        ProgressTimerHandle,
        this,
        &UAscTimerConditionComponent::HandleProgressTimer,
        FMath::Max(0.05f, ProgressUpdateInterval),
        true);
}

void UAscTimerConditionComponent::OnDeactivated()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ProgressTimerHandle);
    }
}

void UAscTimerConditionComponent::OnReset()
{
    StartTimeSeconds = 0.0;
    bTimerStarted = false;
}

void UAscTimerConditionComponent::HandleProgressTimer()
{
    if (!IsConditionActive())
    {
        return;
    }

    NotifyProgressChanged();

    if (GetElapsedTime() >= RequiredDuration)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(ProgressTimerHandle);
        }
        Satisfy();
    }
}

float UAscTimerConditionComponent::GetElapsedTime() const
{
    const UWorld* World = GetWorld();
    if (!World || !bTimerStarted)
    {
        return 0.0f;
    }

    return static_cast<float>(World->GetTimeSeconds() - StartTimeSeconds);
}
