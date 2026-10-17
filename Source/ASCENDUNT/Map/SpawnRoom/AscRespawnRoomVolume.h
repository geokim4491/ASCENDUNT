#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "AscRespawnRoomVolume.generated.h"

class AAscenduntPlayerState;
class UAscenduntAbilitySystemComponent;
class UBoxComponent;

/**
 * Server-authoritative spawn/respawn room volume.
 *
 * Level designers should make BP children of this class and place them in maps.
 * The C++ parent owns the trusted gameplay state:
 * - grants spawn-room / invincible server-authoritative loose gameplay tags to the player's ASC
 * - allows MatchGameMode to validate InGameRespawnRoom hero selection from ASC tags
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AAscRespawnRoomVolume : public AActor
{
    GENERATED_BODY()

public:
    AAscRespawnRoomVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Respawn Room")
    UBoxComponent* GetBoxComponent() const { return BoxComponent; }

    /** This uses PlayerState TeamId. Current match teams are 1 and 2. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Respawn Room")
    uint8 GetTeamId() const { return TeamId; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Respawn Room")
    bool IsPlayerStateInside(const AAscenduntPlayerState* PlayerState) const;

    /** Re-evaluates one actor immediately. Useful when a pawn initially spawns inside the volume. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Respawn Room")
    bool RefreshActorRoomState(AActor* Actor);

    /** Re-evaluates all current pawns against this volume. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Respawn Room")
    void RefreshOverlappingActors();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Respawn Room")
    TObjectPtr<UBoxComponent> BoxComponent;

    /** Match PlayerState team id. For the current team modes use 1 for Team 1 and 2 for Team 2. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Respawn Room", meta = (ClampMin = "0"))
    uint8 TeamId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Respawn Room")
    FGameplayTag SpawnRoomTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Respawn Room")
    FGameplayTag InvincibleTag;

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Respawn Room")
    void BP_OnPlayerEntered(AAscenduntPlayerState* PlayerState);

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Respawn Room")
    void BP_OnPlayerExited(AAscenduntPlayerState* PlayerState);

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

    AAscenduntPlayerState* ResolvePlayerStateFromActor(AActor* Actor) const;
    UAscenduntAbilitySystemComponent* ResolveASC(AAscenduntPlayerState* PlayerState) const;
    /* Player의 TeamId와 리스폰 룸 TeamId가 동일해야 리스폰 룸 효과 적용됨. */
    bool CanAffectPlayerState(const AAscenduntPlayerState* PlayerState) const;

    void ApplyRoomState(AAscenduntPlayerState* PlayerState);
    void RemoveRoomState(AAscenduntPlayerState* PlayerState);

    /* Set 순회하면서 컨테이너 원소 Remove하면 iterator invalidation이 생길 수 있어 Array로 복사 후 Remove */
    void ClearAllTrackedPlayers();

private:

    /* 해당 볼륨으로 TAG를 부여한 PlayerState 목록을 추적함
    * 이게 있어야 게임 중간에 리스폰 룸 볼륨 없애고 새로운 장소로 리스폰 룸 설정하는 구조가 가능해짐.
    * 중복 관리 위해서 Set으로 관리
    */
    TSet<TWeakObjectPtr<AAscenduntPlayerState>> AppliedPlayerStates;
};
