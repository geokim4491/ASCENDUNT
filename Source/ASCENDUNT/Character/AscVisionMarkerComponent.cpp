// Fill out your copyright notice in the Description page of Project Settings.


#include "AscVisionMarkerComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

UAscVisionMarkerComponent::UAscVisionMarkerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
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
		return;
	}

	EnsureCriticalWidgetCreated();
	SetCriticalVisible(false);
}

void UAscVisionMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CriticalWidgetComponent)
	{
		CriticalWidgetComponent->DestroyComponent();
		CriticalWidgetComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
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
	USkeletalMeshComponent* AttachMesh = CharacterOwner ? CharacterOwner->GetMesh() : Owner->FindComponentByClass<USkeletalMeshComponent>();
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
	// 위로는 안커지도록
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
	bCriticalVisible = bVisible;

	EnsureCriticalWidgetCreated();

	if (!CriticalWidgetComponent)
	{
		return;
	}

	CriticalWidgetComponent->SetVisibility(bVisible, true);
	CriticalWidgetComponent->SetHiddenInGame(!bVisible);
}
