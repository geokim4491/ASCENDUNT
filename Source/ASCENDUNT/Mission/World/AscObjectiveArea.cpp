#include "ASCENDUNT/Mission/World/AscObjectiveArea.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

AAscObjectiveArea::AAscObjectiveArea()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    AreaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBox"));
    SetRootComponent(AreaBox);

    AreaBox->SetCollisionProfileName(TEXT("Trigger"));
    AreaBox->SetGenerateOverlapEvents(true);
    AreaBox->SetBoxExtent(FVector(200.0f));

    AreaBox->OnComponentBeginOverlap.AddDynamic(this, &AAscObjectiveArea::HandleBeginOverlap);
    AreaBox->OnComponentEndOverlap.AddDynamic(this, &AAscObjectiveArea::HandleEndOverlap);
}

int32 AAscObjectiveArea::GetPlayerCountInside() const
{
    int32 ValidCount = 0;
    for (const TWeakObjectPtr<APawn>& Pawn : PlayerPawnsInside)
    {
        if (Pawn.IsValid())
        {
            ++ValidCount;
        }
    }
    return ValidCount;
}

bool AAscObjectiveArea::IsPlayerInside(APawn* PlayerPawn) const
{
    return PlayerPawn && PlayerPawnsInside.Contains(TWeakObjectPtr<APawn>(PlayerPawn));
}

void AAscObjectiveArea::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !Pawn->IsPlayerControlled())
    {
        return;
    }

    const TWeakObjectPtr<APawn> WeakPawn(Pawn);
    if (!PlayerPawnsInside.Contains(WeakPawn))
    {
        PlayerPawnsInside.Add(WeakPawn);
        OnPlayerEntered.Broadcast(this, Pawn);
    }
}

void AAscObjectiveArea::HandleEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
    {
        return;
    }

    if (PlayerPawnsInside.Remove(TWeakObjectPtr<APawn>(Pawn)) > 0)
    {
        OnPlayerExited.Broadcast(this, Pawn);
    }
}
