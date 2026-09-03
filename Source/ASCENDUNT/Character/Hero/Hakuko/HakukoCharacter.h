#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Weapon/AscWeaponAOEProviderInterface.h"
#include "HakukoCharacter.generated.h"

class UHakukoSkillComponent;
class UHakukoFoxComponent;

UCLASS()
class ASCENDUNT_API AHakukoCharacter : public AHeroCharacter, public IAscWeaponAOEProviderInterface
{
	GENERATED_BODY()

public:
	AHakukoCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category="Hakuko")
	UHakukoSkillComponent* GetHakukoSkillComponent() const { return HakukoSkillComponent; }

	/** Legacy Blueprint compatibility. The reworked Hakuko no longer creates a fox component. */
	UFUNCTION(BlueprintPure, Category="Hakuko", meta=(DeprecatedFunction, DeprecationMessage="Use GetHakukoSkillComponent"))
	UHakukoFoxComponent* GetHakukoFoxComponent() const;

	virtual bool GetWeaponAOEParameters(float& OutRadius, float& OutDamageCoefficient, bool& bOutDebugDrawRadius, float& OutDebugDrawDuration) const override;
	virtual void InitializeGameplayBindings() override;
	virtual void UninitializeGameplayBindings() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hakuko", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UHakukoSkillComponent> HakukoSkillComponent;
};
