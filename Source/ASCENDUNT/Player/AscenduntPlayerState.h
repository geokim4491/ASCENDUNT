// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySet.h"
#include "AscenduntPlayerState.generated.h"

class AAscenduntPlayerController;
class UAscenduntAbilitySystemComponent;
class UHeroPawnData;
class UAscenduntHealthSet;
class UAscenduntCharacterStatSet;
class UAscenduntWeaponSet;
class UAscenduntUltimateSet;
class UAscCombatContributionComponent;

DECLARE_MULTICAST_DELEGATE(FOnAscenduntPawnDataEvent);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAscTeamIdChangedNative, uint8 /*Old*/, uint8 /*New*/);
DECLARE_MULTICAST_DELEGATE(FOnAscPlayerStateChangedNative);

/**
 * PlayerState trimmed after removing the old Lobby system.
 *
 * Kept:
 * - TeamId as authoritative replicated team source
 * - DisplayName as a generic player-facing name, not a lobby concept
 * - PawnData / ASC setup
 *
 * Removed:
 * - lobby ready
 * - room host
 * - spectator flag
 * - lobby join order
 */
UCLASS()
class ASCENDUNT_API AAscenduntPlayerState : public APlayerState, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    AAscenduntPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // ===== Team (Source of Truth for players) =====
    virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(TeamId); }
    virtual void SetGenericTeamId(const FGenericTeamId& NewID) override;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Team")
    void SetTeamId(uint8 NewTeamId) { SetGenericTeamId(FGenericTeamId(NewTeamId)); }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Team")
    uint8 GetTeamId() const { return TeamId; }

    // ===== Player Identity =====
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Player")
    void SetDisplayName(const FString& InName);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Player")
    const FString& GetDisplayName() const { return DisplayName; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Player")
    void SetMatchPlayerId(const FString& InPlayerId);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Player")
    const FString& GetMatchPlayerId() const { return MatchPlayerId; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Player")
    void SetMatchTicketId(const FString& InTicketId);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Player")
    const FString& GetMatchTicketId() const { return MatchTicketId; }

    // ===== Hero Selection =====
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void SetSelectedHeroId(int32 InHeroId);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero Selection")
    int32 GetSelectedHeroId() const { return SelectedHeroId; }

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero Selection")
    void SetConfirmedHero(int32 InHeroId, EAscHeroRole InHeroRole);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero Selection")
    int32 GetConfirmedHeroId() const { return ConfirmedHeroId; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero Selection")
    EAscHeroRole GetConfirmedHeroRole() const { return ConfirmedHeroRole; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero Selection")
    bool HasConfirmedHero() const { return bHeroConfirmed; }

    // ===== Respawn =====
    void SetRespawnState(EAscPlayerRespawnState NewState, float InRespawnEndServerTime = 0.0f);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Respawn")
    EAscPlayerRespawnState GetRespawnState() const { return RespawnState; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Respawn")
    bool IsWaitingForRespawn() const { return RespawnState == EAscPlayerRespawnState::DeadWaitingRespawn; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Respawn")
    float GetRespawnEndServerTime() const { return RespawnEndServerTime; }

    // ===== Helpers =====
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|PlayerState")
    AAscenduntPlayerController* GetAscenduntPlayerController() const;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|PlayerState")
    UAscenduntAbilitySystemComponent* GetAscenduntAbilitySystemComponent() const { return AbilitySystemComponent; }

    const UAscenduntHealthSet* GetHealthSet() const { return HealthSet; }
    const UAscenduntCharacterStatSet* GetCharacterStatSet() const { return CharacterStatSet; }
    const UAscenduntWeaponSet* GetWeaponSet() const { return WeaponSet; }
    const UAscenduntUltimateSet* GetUltimateSet() const { return UltimateSet; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|PlayerState")
    UAscCombatContributionComponent* GetCombatContributionComponent() const { return CombatContributionComponent; }

    /** Server: called by HeroInit when the current Pawn reaches GASInitialized. */
    void InitializeCombatForCurrentPawnData();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    template <class T>
    const T* GetPawnData() const { return Cast<T>(PawnData); }

    UFUNCTION(BlueprintCallable) // TODO: 나중에 제거
    void SetPawnData(const UHeroPawnData* InPawnData);

    /** Removes the AbilitySet grants owned by the currently active hero PawnData. */
    void RemovePawnDataAbilitySets();

    /** Grants the currently selected PawnData AbilitySets after the new Pawn is the ASC AvatarActor. */
    bool ApplyPawnDataAbilitySets();

    //~AActor interface
    virtual void PreInitializeComponents() override;
    virtual void PostInitializeComponents() override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    const UHeroPawnData* GetPawnData() const { return PawnData; }
    bool IsPawnDataApplied() const { return AppliedPawnData && AppliedPawnData == PawnData; }
    const UHeroPawnData* GetAppliedPawnData() const { return AppliedPawnData; }

public:
    FOnAscenduntPawnDataEvent OnPawnDataAvailable;
    FOnAscenduntPawnDataEvent OnPawnDataApplied;
    FOnAscTeamIdChangedNative OnTeamIdChanged;
    FOnAscPlayerStateChangedNative OnPlayerStateChanged;

protected:
    UFUNCTION()
    void OnRep_PawnData();

    UFUNCTION()
    void OnRep_AppliedPawnData();

    UFUNCTION()
    void OnRep_TeamId(uint8 OldTeamId);

    UFUNCTION()
    void OnRep_DisplayName();

    UFUNCTION()
    void OnRep_MatchPlayerId();

    UFUNCTION()
    void OnRep_MatchTicketId();

    UFUNCTION()
    void OnRep_SelectedHeroId();

    UFUNCTION()
    void OnRep_ConfirmedHeroId();

    UFUNCTION()
    void OnRep_ConfirmedHeroRole();

    UFUNCTION()
    void OnRep_HeroConfirmed();

    UFUNCTION()
    void OnRep_RespawnState();

    UFUNCTION()
    void OnRep_RespawnEndServerTime();

protected:
    UPROPERTY(ReplicatedUsing = OnRep_PawnData)
    TObjectPtr<const UHeroPawnData> PawnData = nullptr;

    UPROPERTY(ReplicatedUsing = OnRep_AppliedPawnData)
    TObjectPtr<const UHeroPawnData> AppliedPawnData = nullptr;

    UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_TeamId, Category = "Ascendunt|Team")
    uint8 TeamId = 1;

    UPROPERTY(ReplicatedUsing = OnRep_DisplayName, BlueprintReadOnly, Category = "Ascendunt|Player")
    FString DisplayName;

    UPROPERTY(ReplicatedUsing = OnRep_MatchPlayerId, BlueprintReadOnly, Category = "Ascendunt|Player")
    FString MatchPlayerId;

    UPROPERTY(ReplicatedUsing = OnRep_MatchTicketId, BlueprintReadOnly, Category = "Ascendunt|Player")
    FString MatchTicketId;

    UPROPERTY(ReplicatedUsing = OnRep_SelectedHeroId, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    int32 SelectedHeroId = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_ConfirmedHeroId, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    int32 ConfirmedHeroId = INDEX_NONE;

    UPROPERTY(ReplicatedUsing = OnRep_ConfirmedHeroRole, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    EAscHeroRole ConfirmedHeroRole = EAscHeroRole::MainDPS;

    UPROPERTY(ReplicatedUsing = OnRep_HeroConfirmed, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    bool bHeroConfirmed = false;

    UPROPERTY(ReplicatedUsing = OnRep_RespawnState, BlueprintReadOnly, Category = "Ascendunt|Respawn")
    EAscPlayerRespawnState RespawnState = EAscPlayerRespawnState::Alive;

    /** Absolute server world time. Clients subtract GameState::GetServerWorldTimeSeconds(). */
    UPROPERTY(ReplicatedUsing = OnRep_RespawnEndServerTime, BlueprintReadOnly, Category = "Ascendunt|Respawn")
    float RespawnEndServerTime = 0.0f;

private:
    UPROPERTY(VisibleAnywhere, Category = "Ascendunt|PlayerState")
    TObjectPtr<UAscenduntAbilitySystemComponent> AbilitySystemComponent;

    /** Persistent AttributeSets. Their lifetime matches the PlayerState ASC. */
    UPROPERTY(VisibleAnywhere, Category = "Ascendunt|PlayerState")
    TObjectPtr<UAscenduntHealthSet> HealthSet;

    UPROPERTY(VisibleAnywhere, Category = "Ascendunt|PlayerState")
    TObjectPtr<UAscenduntCharacterStatSet> CharacterStatSet;

    UPROPERTY(VisibleAnywhere, Category = "Ascendunt|PlayerState")
    TObjectPtr<UAscenduntWeaponSet> WeaponSet;

    UPROPERTY(VisibleAnywhere, Category = "Ascendunt|PlayerState")
    TObjectPtr<UAscenduntUltimateSet> UltimateSet;

    UPROPERTY(VisibleAnywhere, Category = "Ascendunt|PlayerState")
    TObjectPtr<UAscCombatContributionComponent> CombatContributionComponent;

    /** Handles for the ability sets granted by the currently selected PawnData. */
    UPROPERTY()
    FAscenduntAbilitySet_GrantedHandles PawnDataAbilitySetHandles;
};
