#include "AscRespawnRoomVolume.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AAscRespawnRoomVolume::AAscRespawnRoomVolume(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bReplicates = false;
    SetCanBeDamaged(false);

    BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
    SetRootComponent(BoxComponent);

    BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
    BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    BoxComponent->SetGenerateOverlapEvents(true);
    BoxComponent->SetBoxExtent(FVector(600.0, 400.0, 250.0));

    SpawnRoomTag = AscenduntGameplayTags::Status_SpawnRoom;
    InvincibleTag = AscenduntGameplayTags::Status_Invincible;
}

void AAscRespawnRoomVolume::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && BoxComponent)
    {
        BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
        BoxComponent->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);

        // Actors can already be inside this volume when the level starts or when
        // a pawn is spawned directly inside the room. BeginOverlap is not guaranteed
        // for that case, so do one authoritative refresh on the next tick.
        GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshOverlappingActors);
    }
}

void AAscRespawnRoomVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        ClearAllTrackedPlayers();
    }

    Super::EndPlay(EndPlayReason);
}

bool AAscRespawnRoomVolume::IsPlayerStateInside(const AAscenduntPlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return false;
    }

    for (const TWeakObjectPtr<AAscenduntPlayerState>& WeakPlayerState : AppliedPlayerStates)
    {
        if (WeakPlayerState.Get() == PlayerState)
        {
            return true;
        }
    }

    return false;
}

bool AAscRespawnRoomVolume::RefreshActorRoomState(AActor* Actor)
{
    if (!HasAuthority() || !Actor)
    {
        return false;
    }

    AAscenduntPlayerState* PlayerState = ResolvePlayerStateFromActor(Actor);
    if (!CanAffectPlayerState(PlayerState))
    {
        return false;
    }

    ApplyRoomState(PlayerState);
    return true;
}

void AAscRespawnRoomVolume::RefreshOverlappingActors()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        RefreshActorRoomState(*It);
    }
}

void AAscRespawnRoomVolume::HandleBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!HasAuthority())
    {
        return;
    }

    AAscenduntPlayerState* PlayerState = ResolvePlayerStateFromActor(OtherActor);
    if (!CanAffectPlayerState(PlayerState) || IsPlayerStateInside(PlayerState))
    {
        return;
    }

    ApplyRoomState(PlayerState);
}

void AAscRespawnRoomVolume::HandleEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (!HasAuthority())
    {
        return;
    }

    AAscenduntPlayerState* PlayerState = ResolvePlayerStateFromActor(OtherActor);
    if (!PlayerState || !IsPlayerStateInside(PlayerState))
    {
        return;
    }

    RemoveRoomState(PlayerState);
}

AAscenduntPlayerState* AAscRespawnRoomVolume::ResolvePlayerStateFromActor(AActor* Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }

    if (APawn* Pawn = Cast<APawn>(Actor))
    {
        return Pawn->GetPlayerState<AAscenduntPlayerState>();
    }

    if (AController* Controller = Cast<AController>(Actor))
    {
        return Controller->GetPlayerState<AAscenduntPlayerState>();
    }

    return nullptr;
}

UAscenduntAbilitySystemComponent* AAscRespawnRoomVolume::ResolveASC(AAscenduntPlayerState* PlayerState) const
{
    return PlayerState ? PlayerState->GetAscenduntAbilitySystemComponent() : nullptr;
}

bool AAscRespawnRoomVolume::CanAffectPlayerState(const AAscenduntPlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return false;
    }

    if (PlayerState->GetTeamId() != TeamId)
    {
        return false;
    }

    return true;
}

void AAscRespawnRoomVolume::ApplyRoomState(AAscenduntPlayerState* PlayerState)
{
    if (!PlayerState)
    {
        return;
    }

    AppliedPlayerStates.Add(TWeakObjectPtr<AAscenduntPlayerState>(PlayerState));

    if (UAscenduntAbilitySystemComponent* ASC = ResolveASC(PlayerState))
    {
        if (SpawnRoomTag.IsValid())
        {
            ASC->AddLooseGameplayTag(SpawnRoomTag);
        }

        if (InvincibleTag.IsValid())
        {
            ASC->AddLooseGameplayTag(InvincibleTag);
        }
    }

    BP_OnPlayerEntered(PlayerState);

    UE_LOG(LogTemp, Warning,
        TEXT("[RespawnRoom] Enter PlayerState=%s Team=%d RoomTeam=%d SpawnRoomTag=%d InvincibleTag=%d"),
        *GetNameSafe(PlayerState),
        PlayerState->GetTeamId(),
        TeamId,
        SpawnRoomTag.IsValid() ? 1 : 0,
        InvincibleTag.IsValid() ? 1 : 0);
}

void AAscRespawnRoomVolume::RemoveRoomState(AAscenduntPlayerState* PlayerState)
{
    if (!PlayerState)
    {
        return;
    }

    AppliedPlayerStates.Remove(TWeakObjectPtr<AAscenduntPlayerState>(PlayerState));

    if (UAscenduntAbilitySystemComponent* ASC = ResolveASC(PlayerState))
    {
        if (SpawnRoomTag.IsValid())
        {
            ASC->RemoveLooseGameplayTag(SpawnRoomTag);
        }

        if (InvincibleTag.IsValid())
        {
            ASC->RemoveLooseGameplayTag(InvincibleTag);
        }
    }

    BP_OnPlayerExited(PlayerState);

    UE_LOG(LogTemp, Warning,
        TEXT("[RespawnRoom] Exit PlayerState=%s RoomTeam=%d"),
        *GetNameSafe(PlayerState),
        TeamId);
}

void AAscRespawnRoomVolume::ClearAllTrackedPlayers()
{
    TArray<TWeakObjectPtr<AAscenduntPlayerState>> Snapshot;
    Snapshot.Reserve(AppliedPlayerStates.Num());

    for (const TWeakObjectPtr<AAscenduntPlayerState>& WeakPlayerState : AppliedPlayerStates)
    {
        Snapshot.Add(WeakPlayerState);
    }

    for (const TWeakObjectPtr<AAscenduntPlayerState>& WeakPlayerState : Snapshot)
    {
        if (AAscenduntPlayerState* PlayerState = WeakPlayerState.Get())
        {
            RemoveRoomState(PlayerState);
        }
    }

    AppliedPlayerStates.Reset();
}
