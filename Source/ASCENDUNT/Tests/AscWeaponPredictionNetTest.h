#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemInterface.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "AscWeaponPredictionNetTest.generated.h"

/** Isolated fixture selected only through an explicit test GameMode URL. */
UCLASS(Transient, NotBlueprintable)
class AAscWeaponPredictionTestWeapon : public AAscenduntWeapon
{
	GENERATED_BODY()
public:
	AAscWeaponPredictionTestWeapon();
};

UCLASS(Transient, NotBlueprintable)
class AAscWeaponPredictionTestWall : public AActor
{
	GENERATED_BODY()
public:
	AAscWeaponPredictionTestWall();
};

UCLASS(Transient, NotBlueprintable)
class AAscWeaponPredictionTestCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AAscWeaponPredictionTestCharacter();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void GrantFixtureAbilities();
	UPROPERTY(ReplicatedUsing=OnRep_TestWeapon) TObjectPtr<AAscenduntWeapon> TestWeapon;
	UFUNCTION() void OnRep_TestWeapon(AAscenduntWeapon* PreviousWeapon);
};

UCLASS(Transient, NotBlueprintable)
class AAscWeaponPredictionTestController : public APlayerController
{
	GENERATED_BODY()
public:
	AAscWeaponPredictionTestController();
	virtual void PlayerTick(float DeltaTime) override;
private:
	UFUNCTION(Server,Reliable) void Server_StartScenario();
	UFUNCTION(Server,Reliable) void Server_ReportResults(int32 ClientAmmo, int32 UIEvents);
	void BlockShots();
	void UnblockShots();
	void TransitionWeapon();
	FTimerHandle BlockTimer, UnblockTimer;
	FDelegateHandle AmmoHandle;
	double StartTime = -1.;
	int32 Stage = 0;
	int32 AmmoEvents = 0;
	uint32 WaitFrames = 0;
	double ReadyTime = -1.;
	int32 LastAmmo = 20;
	int32 ReloadRefills = 0;
};

UCLASS(Transient, NotBlueprintable)
class AAscWeaponPredictionTestGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAscWeaponPredictionTestGameMode();
	virtual void StartPlay() override;
};
