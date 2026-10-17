#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "AscHitCollisionComponent.generated.h"

/**
 * Gameplay hit zone represented by an editor-placeable collision primitive.
 *
 * Multiple collision components may use the same zone. For example, an arm can
 * be covered by two capsules while both are still classified as LeftArm.
 */
UENUM(BlueprintType)
enum class EAscHitZone : uint8
{
	Head UMETA(DisplayName = "Head"),
	Torso UMETA(DisplayName = "Torso"),

	LeftUpperArm UMETA(DisplayName = "Left Upper Arm"),
	LeftForearm UMETA(DisplayName = "Left Forearm"),
	RightUpperArm UMETA(DisplayName = "Right Upper Arm"),
	RightForearm UMETA(DisplayName = "Right Forearm"),

	LeftThigh UMETA(DisplayName = "Left Thigh"),
	LeftCalf UMETA(DisplayName = "Left Calf"),
	RightThigh UMETA(DisplayName = "Right Thigh"),
	RightCalf UMETA(DisplayName = "Right Calf")
};

/**
 * Gameplay-only hit collision.
 *
 * Add these directly from a Character/Pawn Blueprint's Components panel and
 * place/rotate/resize them in the Blueprint viewport. The component itself does not
 * depend on ACharacter or PhysicsAsset: AI can attach it to the root/capsule, while
 * Hero can attach it to a SkeletalMesh bone so the same hit-zone API is reused.
 */
UCLASS(ClassGroup = (Ascendunt), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent, DisplayName = "Asc Hit Collision"))
class ASCENDUNT_API UAscHitCollisionComponent : public UCapsuleComponent
{
	GENERATED_BODY()

public:
	UAscHitCollisionComponent();

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Hit Collision")
	EAscHitZone GetHitZone() const { return HitZone; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Hit Collision")
	bool IsHeadZone() const { return HitZone == EAscHitZone::Head; }

	/** Enables/disables every Asc Hit Collision attached to the actor. */
	static void SetHitCollisionsEnabled(AActor* OwnerActor, bool bEnabled);

private:
	/** Select the semantic hit region. The names are fixed; Blueprint only chooses one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Hit Collision", meta = (AllowPrivateAccess = "true"))
	EAscHitZone HitZone = EAscHitZone::Torso;
};
