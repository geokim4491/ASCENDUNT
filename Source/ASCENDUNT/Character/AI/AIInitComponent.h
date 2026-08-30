// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayTagContainer.h"

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIInitComponent.generated.h"

class ABaseAIController;
class ABaseAICharacter;
class UAscenduntAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAIGameplayReady, APawn*, Owner);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ASCENDUNT_API UAIInitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIInitComponent();

	// 서버: PossessedBy 이후 호출
	void NotifyPossessed();

	// 클라: OnRep_Controller 이후 호출
	void NotifyControllerRep();

	// 필요 시: BT/Blackboard/Weapon 등 외부 준비 완료 신호를 여기에 추가 가능
	// void NotifyXYZReady();

	void Shutdown();

	UPROPERTY(BlueprintAssignable)
	FOnAIGameplayReady OnGameplayReady;

private:
	void Evaluate();

	bool CanEnterDataAvailable() const;
	bool CanEnterDataInitialized() const;
	bool CanEnterGameplayReady() const;

	void EnterSpawned();
	void EnterDataAvailable();
	void EnterDataInitialized();
	void EnterGameplayReady();

	void TryStartBTFromReady();
private:
	FGameplayTag CurrentState;

	TWeakObjectPtr<ABaseAICharacter> OwnerPawn;
	TWeakObjectPtr<ABaseAIController> CachedAIC;
	TWeakObjectPtr<UAscenduntAbilitySystemComponent> CachedASC;

	mutable bool bActorInfoSet = false;
	bool bHealthCompInitialized = false;

	UPROPERTY(Transient)
	bool bGameplayReady = false;

	UPROPERTY(Transient)
	bool bBTStarted = false;
};
