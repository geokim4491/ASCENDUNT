// Fill out your copyright notice in the Description page of Project Settings.

#include "AscVisionMarkerComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UAscVisionMarkerComponent::UAscVisionMarkerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.15f;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FClassFinder<UUserWidget>
		CriticalWidgetClassFinder(TEXT("/Game/UI/Combat/WBP_FatalInjury"));

	if (CriticalWidgetClassFinder.Succeeded())
	{
		CriticalWidgetClass = CriticalWidgetClassFinder.Class;
	}
}

void UAscVisionMarkerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UWorld* World = GetWorld(); World && World->IsNetMode(NM_DedicatedServer))
	{
		SetComponentTickEnabled(false);
		return;
	}

	EnsureCriticalWidgetCreated();
	SetCriticalVisible(false);
}

void UAscVisionMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedViewPlayerController.Reset();
	SetComponentTickEnabled(false);

	if (CriticalWidgetComponent)
	{
		CriticalWidgetComponent->DestroyComponent();
		CriticalWidgetComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UAscVisionMarkerComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCriticalVisible)
	{
		UpdateCriticalScale();
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

void UAscVisionMarkerComponent::EnsureCriticalWidgetCreated()
{
	if (CriticalWidgetComponent)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
	USkeletalMeshComponent* AttachMesh = CharacterOwner
		? CharacterOwner->GetMesh()
		: Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!AttachMesh)
	{
		return;
	}

	CriticalWidgetComponent = NewObject<UWidgetComponent>(Owner, TEXT("CriticalStateWidget"));
	if (!CriticalWidgetComponent)
	{
		return;
	}

	Owner->AddOwnedComponent(CriticalWidgetComponent);

	CriticalWidgetComponent->SetIsReplicated(false);
	CriticalWidgetComponent->SetWidgetSpace(WidgetSpace);
	CriticalWidgetComponent->SetDrawAtDesiredSize(true);
	CriticalWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CriticalWidgetComponent->SetHiddenInGame(true);
	CriticalWidgetComponent->SetPivot(FVector2D(0.5f, 0.4f));

	if (CriticalWidgetClass)
	{
		CriticalWidgetComponent->SetWidgetClass(CriticalWidgetClass);
	}

	CriticalWidgetComponent->SetupAttachment(AttachMesh, AttachSocketName);
	CriticalWidgetComponent->RegisterComponent();
	CriticalWidgetComponent->SetRelativeLocation(RelativeLocation);
	CriticalWidgetComponent->SetVisibility(false, true);
}

void UAscVisionMarkerComponent::SetCriticalVisible(bool bVisible)
{
	SetCriticalVisibleForViewer(bVisible, nullptr);
}

void UAscVisionMarkerComponent::SetCriticalVisibleForViewer(
	bool bVisible,
	APlayerController* ViewPlayerController)
{
	bCriticalVisible = bVisible;

	if (ViewPlayerController)
	{
		CachedViewPlayerController = ViewPlayerController;
	}

	EnsureCriticalWidgetCreated();
	if (!CriticalWidgetComponent)
	{
		return;
	}

	CriticalWidgetComponent->SetVisibility(bVisible, true);
	CriticalWidgetComponent->SetHiddenInGame(!bVisible);

	if (!bVisible)
	{
		SetComponentTickEnabled(false);
		if (UUserWidget* Widget = CriticalWidgetComponent->GetUserWidgetObject())
		{
			Widget->SetRenderScale(FVector2D(ScaleMax, ScaleMax));
		}
		return;
	}

	UpdateCriticalScale();
	SetComponentTickEnabled(true);
}

void UAscVisionMarkerComponent::UpdateCriticalScale()
{
	if (!CriticalWidgetComponent || !GetOwner())
	{
		return;
	}

	APlayerController* PC = CachedViewPlayerController.Get();
	if (!IsValid(PC))
	{
		PC = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (!IsValid(PC))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const float Distance = FVector::Dist(ViewLocation, GetOwner()->GetActorLocation());
	const float Alpha = FMath::Clamp(
		FMath::GetRangePct(ScaleNearDistance, ScaleFarDistance, Distance),
		0.f,
		1.f);
	const float Scale = FMath::Lerp(ScaleMax, ScaleMin, Alpha);

	if (UUserWidget* Widget = CriticalWidgetComponent->GetUserWidgetObject())
	{
		Widget->SetRenderScale(FVector2D(Scale, Scale));
	}
}
