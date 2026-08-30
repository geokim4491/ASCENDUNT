#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASCENDUNT/UI/InGame/TargetHealth/AscTargetHealthBarWidget.h"
#include "ASCENDUNT/UI/WorldStatus/AscWorldStatusLayoutData.h"
#include "AscTargetStatusDisplayComponent.generated.h"

class UWidgetComponent;
class USkeletalMeshComponent;
class UAscDownedIndicatorWidget;
class UAscPvEDownedComponent;

/**
 * Local-only presentation component attached to every ABaseCharacter, including AI.
 * It never owns replicated status data; it only renders state supplied by the local viewer.
 *
 * World-space widget placement is read from UAscWorldStatusLayoutData and attaches directly
 * to skeletal-mesh sockets. Character Blueprints do not need separate anchor components.
 */
UCLASS(ClassGroup = (ASCENDUNT), meta = (BlueprintSpawnableComponent))
class ASCENDUNT_API UAscTargetStatusDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscTargetStatusDisplayComponent();

	UFUNCTION(BlueprintPure, Category = "Ascendunt|TargetStatus")
	static UAscTargetStatusDisplayComponent* FindTargetStatusDisplayComponent(const AActor* Actor);


	/** Local-only overhead health bar controlled by the local viewer subsystem. */
	void SetTargetHealthBar(const FAscenduntHealthSnapshot& HealthSnapshot, EAscTargetHealthBarRelation Relation);

	void ClearTargetHealthBar();

	UFUNCTION(BlueprintPure, Category = "Ascendunt|TargetStatus|Health")
	bool IsTargetHealthBarVisible() const;

	/** Local-only Downed teammate indicator. The WBP owns the icon; C++ updates visibility/time. */
	void RefreshDownedIndicator();
	void ClearDownedIndicator();

	void SetConfig(UAscWorldStatusLayoutData* InConfig) { WorldStatusLayoutData = InConfig; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	USkeletalMeshComponent* GetOwnerMesh() const;

	void EnsureTargetHealthBarWidget();
	const FAscWorldStatusWidgetLayout* GetTargetHealthBarLayout() const;
	USkeletalMeshComponent* ResolveLayoutMesh(const FAscWorldStatusWidgetLayout& Layout,
		bool& bReportedMissingMesh, const TCHAR* WidgetContext) const;
	FName ResolveLayoutSocket(const FAscWorldStatusWidgetLayout& Layout,
		const USkeletalMeshComponent* MeshComponent, bool& bReportedMissingSocket, const TCHAR* WidgetContext) const;
	void ApplyTargetHealthBarLayout(const FAscWorldStatusWidgetLayout& Layout);

	void EnsureDownedIndicatorWidget();
	const FAscWorldStatusWidgetLayout* GetDownedIndicatorLayout() const;
	void ApplyDownedIndicatorLayout(const FAscWorldStatusWidgetLayout& Layout);
	void UpdateDownedIndicator();
	bool ShouldShowDownedIndicatorToLocalViewer() const;

	void UpdateWidgetFacingCamera(UWidgetComponent* WidgetComponent, const FRotator& RotationOffset = FRotator::ZeroRotator);
	bool ShouldTickForVisibleStatusWidget() const;

	UFUNCTION()
	void HandleOwnerDeathStarted(AActor* OwningActor);

	void HandleOwnerDownedStateChanged(bool bIsDowned);

private:

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> TargetHealthBarWidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAscTargetHealthBarWidget> TargetHealthBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> DownedIndicatorWidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAscDownedIndicatorWidget> DownedIndicatorWidget;

	/**
	 * Assign one character/archetype layout asset here. The Target Health Bar entry selects
	 * the mesh component and socket directly; no Blueprint anchor components are required.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|TargetStatus|Layout",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscWorldStatusLayoutData> WorldStatusLayoutData;

	mutable bool bReportedMissingTargetHealthLayout = false;
	mutable bool bReportedInvalidTargetHealthWidgetClass = false;
	mutable bool bReportedMissingTargetHealthMesh = false;
	mutable bool bReportedMissingTargetHealthSocket = false;

	mutable bool bReportedMissingDownedLayout = false;
	mutable bool bReportedInvalidDownedWidgetClass = false;
	mutable bool bReportedMissingDownedMesh = false;
	mutable bool bReportedMissingDownedSocket = false;

};
