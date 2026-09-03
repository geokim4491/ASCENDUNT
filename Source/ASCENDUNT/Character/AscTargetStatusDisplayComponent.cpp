#include "AscTargetStatusDisplayComponent.h"

#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/BaseCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/UI/InGame/Downed/AscDownedIndicatorWidget.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

UAscTargetStatusDisplayComponent::UAscTargetStatusDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);
}

UAscTargetStatusDisplayComponent* UAscTargetStatusDisplayComponent::FindTargetStatusDisplayComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UAscTargetStatusDisplayComponent>() : nullptr;
}

void UAscTargetStatusDisplayComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UAscenduntHealthComponent* HealthComponent = UAscenduntHealthComponent::FindHealthComponent(GetOwner()))
	{
		HealthComponent->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleOwnerDeathStarted);
	}

	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner()))
	{
		if (UAscPvEDownedComponent* Downed = Hero->GetPvEDownedComponent())
		{
			Downed->OnDownedStateChanged.RemoveAll(this);
			Downed->OnDownedStateChanged.AddUObject(this, &ThisClass::HandleOwnerDownedStateChanged);
		}
	}

	RefreshDownedIndicator();
}

void UAscTargetStatusDisplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAscenduntHealthComponent* HealthComponent = UAscenduntHealthComponent::FindHealthComponent(GetOwner()))
	{
		HealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleOwnerDeathStarted);
	}

	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner()))
	{
		if (UAscPvEDownedComponent* Downed = Hero->GetPvEDownedComponent())
		{
			Downed->OnDownedStateChanged.RemoveAll(this);
		}
	}

	ClearTargetHealthBar();
	ClearDownedIndicator();



	if (TargetHealthBarWidgetComponent)
	{
		TargetHealthBarWidgetComponent->DestroyComponent();
		TargetHealthBarWidgetComponent = nullptr;
		TargetHealthBarWidget = nullptr;
	}

	if (DownedIndicatorWidgetComponent)
	{
		DownedIndicatorWidgetComponent->DestroyComponent();
		DownedIndicatorWidgetComponent = nullptr;
		DownedIndicatorWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UAscTargetStatusDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);



	if (TargetHealthBarWidgetComponent && TargetHealthBarWidgetComponent->IsVisible())
	{
		if (const FAscWorldStatusWidgetLayout* Layout = GetTargetHealthBarLayout();
			Layout && Layout->bFaceLocalCamera)
		{
			UpdateWidgetFacingCamera(TargetHealthBarWidgetComponent, Layout->CameraFacingRotationOffset);
		}
	}

	if (DownedIndicatorWidgetComponent && DownedIndicatorWidgetComponent->IsVisible())
	{
		UpdateDownedIndicator();

		if (const FAscWorldStatusWidgetLayout* Layout = GetDownedIndicatorLayout();
			Layout && Layout->bFaceLocalCamera)
		{
			UpdateWidgetFacingCamera(DownedIndicatorWidgetComponent, Layout->CameraFacingRotationOffset);
		}
	}

	if (!ShouldTickForVisibleStatusWidget())
	{
		SetComponentTickEnabled(false);
	}
}


void UAscTargetStatusDisplayComponent::SetTargetHealthBar(
	const FAscenduntHealthSnapshot& HealthSnapshot, EAscTargetHealthBarRelation Relation)
{
	EnsureTargetHealthBarWidget();
	if (!TargetHealthBarWidgetComponent || !TargetHealthBarWidget)
	{
		return;
	}

	TargetHealthBarWidget->SetRelation(Relation);
	TargetHealthBarWidget->SetHealthSnapshot(HealthSnapshot);

	TargetHealthBarWidgetComponent->SetVisibility(true, true);
	TargetHealthBarWidgetComponent->SetHiddenInGame(false, true);

	if (const FAscWorldStatusWidgetLayout* Layout = GetTargetHealthBarLayout();
		Layout && Layout->bFaceLocalCamera)
	{
		UpdateWidgetFacingCamera(TargetHealthBarWidgetComponent, Layout->CameraFacingRotationOffset);
		SetComponentTickEnabled(true);
	}
	else
	{
		SetComponentTickEnabled(ShouldTickForVisibleStatusWidget());
	}
}

void UAscTargetStatusDisplayComponent::ClearTargetHealthBar()
{
	if (TargetHealthBarWidgetComponent)
	{
		TargetHealthBarWidgetComponent->SetVisibility(false, true);
		TargetHealthBarWidgetComponent->SetHiddenInGame(true, true);
	}

	SetComponentTickEnabled(ShouldTickForVisibleStatusWidget());
}

bool UAscTargetStatusDisplayComponent::IsTargetHealthBarVisible() const
{
	return TargetHealthBarWidgetComponent && TargetHealthBarWidgetComponent->IsVisible();
}

void UAscTargetStatusDisplayComponent::RefreshDownedIndicator()
{
	if (!ShouldShowDownedIndicatorToLocalViewer())
	{
		ClearDownedIndicator();
		return;
	}

	EnsureDownedIndicatorWidget();
	if (!DownedIndicatorWidgetComponent || !DownedIndicatorWidget)
	{
		return;
	}

	DownedIndicatorWidgetComponent->SetVisibility(true, true);
	DownedIndicatorWidgetComponent->SetHiddenInGame(false, true);
	UpdateDownedIndicator();

	if (const FAscWorldStatusWidgetLayout* Layout = GetDownedIndicatorLayout();
		Layout && Layout->bFaceLocalCamera)
	{
		UpdateWidgetFacingCamera(DownedIndicatorWidgetComponent, Layout->CameraFacingRotationOffset);
	}

	// The timer text changes locally from replicated server time, so tick only while this widget is visible.
	SetComponentTickEnabled(true);
}

void UAscTargetStatusDisplayComponent::ClearDownedIndicator()
{
	if (DownedIndicatorWidget)
	{
		DownedIndicatorWidget->SetBleedoutRemainingTime(0.0f);
	}

	if (DownedIndicatorWidgetComponent)
	{
		DownedIndicatorWidgetComponent->SetVisibility(false, true);
		DownedIndicatorWidgetComponent->SetHiddenInGame(true, true);
	}

	SetComponentTickEnabled(ShouldTickForVisibleStatusWidget());
}


void UAscTargetStatusDisplayComponent::EnsureTargetHealthBarWidget()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FAscWorldStatusWidgetLayout* Layout = GetTargetHealthBarLayout();
	if (!Layout)
	{
		if (!bReportedMissingTargetHealthLayout)
		{
			bReportedMissingTargetHealthLayout = true;
			UE_LOG(LogTemp, Warning,
				TEXT("TargetStatus: Target Health Bar layout is missing on [%s]. Assign WorldStatusLayoutData and add the Target Health Bar entry."),
				*GetNameSafe(OwnerActor));
		}
		return;
	}

	const TSubclassOf<UUserWidget> DesiredWidgetClass = Layout->WidgetClass;
	if (!DesiredWidgetClass || !DesiredWidgetClass->IsChildOf(UAscTargetHealthBarWidget::StaticClass()))
	{
		if (!bReportedInvalidTargetHealthWidgetClass)
		{
			bReportedInvalidTargetHealthWidgetClass = true;
			UE_LOG(LogTemp, Warning,
				TEXT("TargetStatus: Target Health Bar WidgetClass on [%s] must derive from UAscTargetHealthBarWidget."),
				*GetNameSafe(WorldStatusLayoutData));
		}
		return;
	}

	if (!TargetHealthBarWidgetComponent)
	{
		TargetHealthBarWidgetComponent = NewObject<UWidgetComponent>(OwnerActor, TEXT("TargetHealthBarWidget"), RF_Transient);
		TargetHealthBarWidgetComponent->SetIsReplicated(false);
		TargetHealthBarWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TargetHealthBarWidgetComponent->SetGenerateOverlapEvents(false);
		TargetHealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
		TargetHealthBarWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
		TargetHealthBarWidgetComponent->SetTwoSided(true);
		// Keep a fixed render target. More maximum health shrinks segment width instead of expanding the world-space bar.
		TargetHealthBarWidgetComponent->SetDrawAtDesiredSize(false);
		TargetHealthBarWidgetComponent->RegisterComponent();
	}

	if (TargetHealthBarWidgetComponent->GetWidgetClass() != DesiredWidgetClass)
	{
		TargetHealthBarWidgetComponent->SetWidgetClass(DesiredWidgetClass);
		TargetHealthBarWidgetComponent->InitWidget();
	}
	else if (!TargetHealthBarWidgetComponent->GetUserWidgetObject())
	{
		TargetHealthBarWidgetComponent->InitWidget();
	}

	TargetHealthBarWidget = Cast<UAscTargetHealthBarWidget>(TargetHealthBarWidgetComponent->GetUserWidgetObject());
	if (!TargetHealthBarWidget)
	{
		return;
	}

	ApplyTargetHealthBarLayout(*Layout);
}

const FAscWorldStatusWidgetLayout* UAscTargetStatusDisplayComponent::GetTargetHealthBarLayout() const
{
	return WorldStatusLayoutData
		? WorldStatusLayoutData->FindWidgetLayoutPtr(EAscWorldStatusWidgetType::TargetHealthBar)
		: nullptr;
}

USkeletalMeshComponent* UAscTargetStatusDisplayComponent::ResolveLayoutMesh(
	const FAscWorldStatusWidgetLayout& Layout, bool& bReportedMissingMesh, const TCHAR* WidgetContext) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (Layout.SkeletalMeshComponentName.IsNone())
	{
		return GetOwnerMesh();
	}

	TArray<USkeletalMeshComponent*> MeshComponents;
	OwnerActor->GetComponents<USkeletalMeshComponent>(MeshComponents);
	for (USkeletalMeshComponent* MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent) && MeshComponent->GetFName() == Layout.SkeletalMeshComponentName)
		{
			return MeshComponent;
		}
	}

	if (!bReportedMissingMesh)
	{
		bReportedMissingMesh = true;
		UE_LOG(LogTemp, Warning,
			TEXT("TargetStatus: %s SkeletalMeshComponent [%s] was not found on [%s]. Falling back to the character mesh."),
			WidgetContext,
			*Layout.SkeletalMeshComponentName.ToString(),
			*GetNameSafe(OwnerActor));
	}

	return GetOwnerMesh();
}

FName UAscTargetStatusDisplayComponent::ResolveLayoutSocket(
	const FAscWorldStatusWidgetLayout& Layout,
	const USkeletalMeshComponent* MeshComponent, bool& bReportedMissingSocket, const TCHAR* WidgetContext) const
{
	if (!MeshComponent || Layout.SocketName.IsNone())
	{
		return NAME_None;
	}

	if (MeshComponent->DoesSocketExist(Layout.SocketName))
	{
		return Layout.SocketName;
	}

	if (!bReportedMissingSocket)
	{
		bReportedMissingSocket = true;
		UE_LOG(LogTemp, Warning,
			TEXT("TargetStatus: %s socket [%s] is missing on mesh [%s] for [%s]. The widget will attach to the mesh root."),
			WidgetContext,
			*Layout.SocketName.ToString(),
			*GetNameSafe(MeshComponent),
			*GetNameSafe(GetOwner()));
	}

	return NAME_None;
}

void UAscTargetStatusDisplayComponent::ApplyTargetHealthBarLayout(
	const FAscWorldStatusWidgetLayout& Layout)
{
	if (!TargetHealthBarWidgetComponent || !TargetHealthBarWidget)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = ResolveLayoutMesh(
		Layout, bReportedMissingTargetHealthMesh, TEXT("Target Health Bar"));
	if (!MeshComponent)
	{
		return;
	}

	const FName SocketName = ResolveLayoutSocket(
		Layout, MeshComponent, bReportedMissingTargetHealthSocket, TEXT("Target Health Bar"));
	TargetHealthBarWidgetComponent->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules::KeepRelativeTransform,
		SocketName);
	TargetHealthBarWidgetComponent->SetRelativeLocation(Layout.RelativeLocation);
	TargetHealthBarWidgetComponent->SetRelativeRotation(Layout.RelativeRotation);
	TargetHealthBarWidgetComponent->SetPivot(Layout.Pivot);
	TargetHealthBarWidgetComponent->SetDrawAtDesiredSize(false);

	const float RenderResolutionScale = FMath::Clamp(Layout.RenderResolutionScale, 1.0f, 8.0f);

	// Width/height are owned by DA_HealthBarStyles through the widget. This layout asset only
	// controls socket placement, pivot, world scale, and render-resolution policy.
	TargetHealthBarWidget->SetPresentationScale(RenderResolutionScale);
	const FIntPoint ResolvedDrawSize(
		FMath::Max(1, FMath::RoundToInt(TargetHealthBarWidget->GetRenderedHealthBarWidth())),
		FMath::Max(1, FMath::RoundToInt(TargetHealthBarWidget->GetRenderedHealthBarHeight())));

	TargetHealthBarWidgetComponent->SetDrawSize(ResolvedDrawSize);

	const float CompensatedWorldScale =
		FMath::Max(KINDA_SMALL_NUMBER, Layout.WorldScale) / RenderResolutionScale;
	TargetHealthBarWidgetComponent->SetRelativeScale3D(FVector(CompensatedWorldScale));
}

void UAscTargetStatusDisplayComponent::EnsureDownedIndicatorWidget()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FAscWorldStatusWidgetLayout* Layout = GetDownedIndicatorLayout();
	if (!Layout)
	{
		if (!bReportedMissingDownedLayout)
		{
			bReportedMissingDownedLayout = true;
			UE_LOG(LogTemp, Warning,
				TEXT("TargetStatus: Downed Indicator layout is missing on [%s]. Add the Downed Indicator entry to WorldStatusLayoutData."),
				*GetNameSafe(OwnerActor));
		}
		return;
	}

	const TSubclassOf<UUserWidget> DesiredWidgetClass = Layout->WidgetClass;
	if (!DesiredWidgetClass || !DesiredWidgetClass->IsChildOf(UAscDownedIndicatorWidget::StaticClass()))
	{
		if (!bReportedInvalidDownedWidgetClass)
		{
			bReportedInvalidDownedWidgetClass = true;
			UE_LOG(LogTemp, Warning,
				TEXT("TargetStatus: Downed Indicator WidgetClass on [%s] must derive from UAscDownedIndicatorWidget."),
				*GetNameSafe(WorldStatusLayoutData));
		}
		return;
	}

	if (!DownedIndicatorWidgetComponent)
	{
		DownedIndicatorWidgetComponent = NewObject<UWidgetComponent>(OwnerActor, TEXT("DownedIndicatorWidget"), RF_Transient);
		DownedIndicatorWidgetComponent->SetIsReplicated(false);
		DownedIndicatorWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DownedIndicatorWidgetComponent->SetGenerateOverlapEvents(false);
		DownedIndicatorWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
		DownedIndicatorWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
		DownedIndicatorWidgetComponent->SetTwoSided(true);
		DownedIndicatorWidgetComponent->SetDrawAtDesiredSize(true);
		DownedIndicatorWidgetComponent->RegisterComponent();
	}

	if (DownedIndicatorWidgetComponent->GetWidgetClass() != DesiredWidgetClass)
	{
		DownedIndicatorWidgetComponent->SetWidgetClass(DesiredWidgetClass);
		DownedIndicatorWidgetComponent->InitWidget();
	}
	else if (!DownedIndicatorWidgetComponent->GetUserWidgetObject())
	{
		DownedIndicatorWidgetComponent->InitWidget();
	}

	DownedIndicatorWidget = Cast<UAscDownedIndicatorWidget>(DownedIndicatorWidgetComponent->GetUserWidgetObject());
	if (!DownedIndicatorWidget)
	{
		return;
	}

	ApplyDownedIndicatorLayout(*Layout);
}

const FAscWorldStatusWidgetLayout* UAscTargetStatusDisplayComponent::GetDownedIndicatorLayout() const
{
	return WorldStatusLayoutData
		? WorldStatusLayoutData->FindWidgetLayoutPtr(EAscWorldStatusWidgetType::DownedIndicator)
		: nullptr;
}

void UAscTargetStatusDisplayComponent::ApplyDownedIndicatorLayout(const FAscWorldStatusWidgetLayout& Layout)
{
	if (!DownedIndicatorWidgetComponent)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = ResolveLayoutMesh(
		Layout, bReportedMissingDownedMesh, TEXT("Downed Indicator"));
	if (!MeshComponent)
	{
		return;
	}

	const FName SocketName = ResolveLayoutSocket(
		Layout, MeshComponent, bReportedMissingDownedSocket, TEXT("Downed Indicator"));

	DownedIndicatorWidgetComponent->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules::KeepRelativeTransform,
		SocketName);
	DownedIndicatorWidgetComponent->SetRelativeLocation(Layout.RelativeLocation);
	DownedIndicatorWidgetComponent->SetRelativeRotation(Layout.RelativeRotation);
	DownedIndicatorWidgetComponent->SetPivot(Layout.Pivot);
	DownedIndicatorWidgetComponent->SetDrawAtDesiredSize(true);
	DownedIndicatorWidgetComponent->SetRelativeScale3D(FVector(FMath::Max(KINDA_SMALL_NUMBER, Layout.WorldScale)));
}

void UAscTargetStatusDisplayComponent::UpdateDownedIndicator()
{
	if (!DownedIndicatorWidget)
	{
		return;
	}

	const AHeroCharacter* Hero = Cast<AHeroCharacter>(GetOwner());
	const UAscPvEDownedComponent* Downed = Hero ? Hero->GetPvEDownedComponent() : nullptr;
	if (!Downed || !Downed->IsDowned() || !ShouldShowDownedIndicatorToLocalViewer())
	{
		ClearDownedIndicator();
		return;
	}

	DownedIndicatorWidget->SetBleedoutRemainingTime(Downed->GetBleedoutRemainingTime());
}

bool UAscTargetStatusDisplayComponent::ShouldShowDownedIndicatorToLocalViewer() const
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const AHeroCharacter* OwnerHero = Cast<AHeroCharacter>(GetOwner());
	const UAscPvEDownedComponent* Downed = OwnerHero ? OwnerHero->GetPvEDownedComponent() : nullptr;
	if (!OwnerHero || !Downed || !Downed->IsDowned())
	{
		return false;
	}

	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(this, 0);
	if (!LocalPC || LocalPC->GetPawn() == OwnerHero)
	{
		// The local Downed player uses the dedicated InGameHUD state instead of its own overhead marker.
		return false;
	}

	const ABaseCharacter* LocalCharacter = Cast<ABaseCharacter>(LocalPC->GetPawn());
	if (!LocalCharacter)
	{
		return false;
	}

	return LocalCharacter->GetGenericTeamId().GetId() == OwnerHero->GetGenericTeamId().GetId();
}


USkeletalMeshComponent* UAscTargetStatusDisplayComponent::GetOwnerMesh() const
{
	const ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner());
	return Character ? Character->GetMesh() : nullptr;
}


void UAscTargetStatusDisplayComponent::UpdateWidgetFacingCamera(UWidgetComponent* WidgetComponent, const FRotator& RotationOffset)
{
	if (!WidgetComponent)
	{
		return;
	}

	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return;
	}

	const FVector WidgetLocation = WidgetComponent->GetComponentLocation();
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(WidgetLocation, CameraManager->GetCameraLocation());
	WidgetComponent->SetWorldRotation(LookAtRotation + RotationOffset);
}

bool UAscTargetStatusDisplayComponent::ShouldTickForVisibleStatusWidget() const
{
	const FAscWorldStatusWidgetLayout* HealthLayout = GetTargetHealthBarLayout();
	const bool bHealthBarNeedsFacing =
		HealthLayout &&
		HealthLayout->bFaceLocalCamera &&
		TargetHealthBarWidgetComponent &&
		TargetHealthBarWidgetComponent->IsVisible();

	// Downed indicator ticks while visible because its remaining time is derived locally
	// from the replicated server deadline. No per-frame RPC or replicated countdown is used.
	const bool bDownedVisible =
		DownedIndicatorWidgetComponent &&
		DownedIndicatorWidgetComponent->IsVisible();

	return bHealthBarNeedsFacing || bDownedVisible;
}

void UAscTargetStatusDisplayComponent::HandleOwnerDeathStarted(AActor* OwningActor)
{
	ClearTargetHealthBar();
}

void UAscTargetStatusDisplayComponent::HandleOwnerDownedStateChanged(bool bIsDowned)
{
	if (bIsDowned)
	{
		RefreshDownedIndicator();
	}
	else
	{
		ClearDownedIndicator();
	}
}
