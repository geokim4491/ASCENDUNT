#include "BTDecorator_CheckHealth.h"
#include "AIController.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"

UBTDecorator_CheckHealth::UBTDecorator_CheckHealth()
{
	NodeName = "Check Health Percentage";
	ThresholdPercentage = 30.0f;
}

bool UBTDecorator_CheckHealth::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	if (!AIC) return false;

	ABaseAICharacter* AIPawn = Cast<ABaseAICharacter>(AIC->GetPawn());
	if (!AIPawn) return false;

	UAscenduntHealthComponent* HealthComp = AIPawn->GetAscenduntHealthComponent();
	if (!HealthComp) return false;

	float CurrentHealth = HealthComp->GetHealth();
	float MaxHealth = HealthComp->GetMaxHealth();
	
	if (MaxHealth <= 0.0f) return false;

	float CurrentPercentage = (CurrentHealth / MaxHealth) * 100.0f;

	return CurrentPercentage <= ThresholdPercentage;
}
