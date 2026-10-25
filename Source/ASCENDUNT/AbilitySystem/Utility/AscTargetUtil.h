#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AscTargetUtil.generated.h"

UCLASS()
class ASCENDUNT_API UAscTargetUtil : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Target")
	static void FindEnemyActorsInRadius(AActor* SourceActor, float Radius, TArray<AActor*>& OutTargets);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Target")
	static bool IsEnemyActor(const AActor* SourceActor, const AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Target")
	static bool IsFriendlyActor(const AActor* SourceActor, const AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Movement")
	static void ApplyKnockback(AActor* SourceActor, AActor* TargetActor, float Strength, float UpwardStrength = 200.0f);
};
