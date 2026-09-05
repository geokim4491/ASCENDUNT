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
class UAscenduntHealthComponent;

/**
 * Local-only presentation component attached to every ABaseCharacter, including AI.
 * 로컬 뷰어는 표시 여부와 관계만 결정하고, 표시 중인 체력 Snapshot 갱신은 이 컴포넌트가 직접 처리한다.
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


	/** 로컬 뷰어 기준 체력바 표시 여부만 외부에서 결정한다. 실제 Snapshot 갱신은 이 컴포넌트가 담당한다. */
	void SetTargetHealthBarVisible(bool bVisible, EAscTargetHealthBarRelation Relation);

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

	// 체력바 생성과 Health Snapshot 반영
	void EnsureTargetHealthBarWidget();
	void RefreshTargetHealthBar();
	void ShowTargetHealthBar(const FAscenduntHealthSnapshot& HealthSnapshot);
	void HideTargetHealthBar();
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
	void HandleOwnerHealthChanged(
		UAscenduntHealthComponent* HealthComponent,
		AActor* OwningActor,
		AActor* Instigator,
		bool bWasDamaging,
		float RawValue,
		const FAscenduntHealthSnapshot& HealthSnapshot);

	UFUNCTION()
	void HandleOwnerDeathStarted(AActor* OwningActor);

	void HandleOwnerDownedStateChanged(bool bIsDowned);

private:
	bool bTargetHealthBarRequestedVisible = false;
	EAscTargetHealthBarRelation TargetHealthBarRelation = EAscTargetHealthBarRelation::Enemy;

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
