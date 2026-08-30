// Fill out your copyright notice in the Description page of Project Settings.

#include "HeroInitComponent.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Camera/AscenduntCameraComponent.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCameraComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroInputComponent.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/Character/Hero/PVE/AscPvEDownedComponent.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameMode.h"
#include "ASCENDUNT/GameModes/PVE/AscPvEGameState.h"
#include "ASCENDUNT/GameModes/PVP/MatchGameMode.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UHeroInitComponent::BeginPlay()
{
    Super::BeginPlay();

    bDeinitialized = false;
    OwnerPawn = Cast<AHeroCharacter>(GetOwner());

    // Evaluate() re-reads the current object graph, so values that arrived before BeginPlay
    // are handled exactly the same as values that arrive later through OnRep/lifecycle events.
    Evaluate();
}

void UHeroInitComponent::HandlePlayerStateChanged(AAscenduntPlayerState* NewPS)
{
    if (bDeinitialized)
    {
        return;
    }

    // OnRep_PlayerState can run before this component's BeginPlay. GetOwner() is already
    // valid for a default subobject, so never make binding depend on BeginPlay ordering.
    if (!OwnerPawn.IsValid())
    {
        OwnerPawn = Cast<AHeroCharacter>(GetOwner());
    }

    if (CachedPS.Get() != NewPS)
    {
        UnbindFromPlayerState();
        CachedPS = NewPS;
        CachedASC = nullptr;
    }

    if (CachedPS.IsValid())
    {
        CachedASC = CachedPS->GetAscenduntAbilitySystemComponent();
        BindToPlayerState(CachedPS.Get());
    }

    Evaluate();
}

void UHeroInitComponent::NotifyControllerChanged()
{
    if (!bDeinitialized)
    {
        Evaluate();
    }
}

void UHeroInitComponent::NotifyInputComponentReady()
{
    if (!bDeinitialized)
    {
        Evaluate();
    }
}

bool UHeroInitComponent::IsGameplayReady() const
{
    return CurrentState == AscenduntGameplayTags::InitState_GameplayReady;
}

void UHeroInitComponent::Evaluate()
{
    if (bDeinitialized || IsGameplayReady())
    {
        return;
    }

    // Replication callbacks can run before this component reaches BeginPlay. Those callbacks
    // may establish idempotent bindings, but gameplay initialization itself begins only after
    // the component lifecycle is ready. BeginPlay() is the guaranteed wake-up for this gate.
    if (!HasBegunPlay())
    {
        return;
    }

    RefreshDependencies();

    if (!OwnerPawn.IsValid())
    {
        return;
    }

    // Sequential if-statements intentionally allow one wake-up event to advance through
    // every state whose prerequisites were already prepared out of order.
    if (!CurrentState.IsValid())
    {
        if (!CanEnterSpawned())
        {
            return;
        }

        EnterSpawned();
    }

    if (CurrentState == AscenduntGameplayTags::InitState_Spawned)
    {
        if (!TryEnterGASInitialized())
        {
            return;
        }
    }

    if (CurrentState == AscenduntGameplayTags::InitState_GASInitialized)
    {
        PrepareGameplayDependencies();

        if (!CanEnterGameplayReady())
        {
            return;
        }

        EnterGameplayReady();
    }
}

void UHeroInitComponent::RefreshDependencies()
{
    if (!OwnerPawn.IsValid())
    {
        OwnerPawn = Cast<AHeroCharacter>(GetOwner());
    }

    if (!OwnerPawn.IsValid())
    {
        return;
    }

    // PlayerState is the source of truth. Do not cache replicated "ready" booleans;
    // every Evaluate() reads the current object graph again.
    AAscenduntPlayerState* CurrentPS = OwnerPawn->GetPlayerState<AAscenduntPlayerState>();
    if (CachedPS.Get() != CurrentPS)
    {
        UnbindFromPlayerState();
        CachedPS = CurrentPS;
        CachedASC = nullptr;
    }

    if (CachedPS.IsValid())
    {
        CachedASC = CachedPS->GetAscenduntAbilitySystemComponent();
        BindToPlayerState(CachedPS.Get());
    }
    else
    {
        CachedASC = nullptr;
    }

    BindToWeaponEquipment();
}

bool UHeroInitComponent::CanEnterSpawned() const
{
    if (!OwnerPawn.IsValid() || !CachedPS.IsValid())
    {
        return false;
    }

    const UHeroPawnData* PawnData = CachedPS->GetPawnData();
    if (!PawnData)
    {
        return false;
    }

    if (!PawnData->PawnClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[HeroInit] PawnData has no PawnClass. Pawn=%s PawnData=%s"),
            *GetNameSafe(OwnerPawn.Get()),
            *GetNameSafe(PawnData));
        return false;
    }

    // During hero replacement the new Pawn can temporarily observe the previous PawnData.
    // This is an asynchronous wait, not a configuration failure.
    return OwnerPawn->IsA(PawnData->PawnClass);
}

void UHeroInitComponent::EnterSpawned()
{
    CurrentState = AscenduntGameplayTags::InitState_Spawned;

    bStaticConfigurationChecked = true;
    bStaticConfigurationValid = ValidateStaticConfiguration();

    UE_LOG(LogTemp, Log,
        TEXT("[HeroInit] %s -> Spawned (Config=%s)"),
        *GetNameSafe(OwnerPawn.Get()),
        bStaticConfigurationValid ? TEXT("Valid") : TEXT("Invalid"));
}

bool UHeroInitComponent::ValidateStaticConfiguration()
{
    check(OwnerPawn.IsValid());
    check(CachedPS.IsValid());

    bool bValid = true;
    const UHeroPawnData* PawnData = CachedPS->GetPawnData();

    auto RequireObject = [this, &bValid](const auto* Object, const TCHAR* Name)
    {
        if (!Object)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[HeroInit] Missing static dependency '%s'. Pawn=%s"),
                Name,
                *GetNameSafe(OwnerPawn.Get()));
            bValid = false;
        }
    };

    RequireObject(PawnData, TEXT("PawnData"));
    RequireObject(OwnerPawn->GetHeroInputComponent(), TEXT("HeroInputComponent"));
    RequireObject(OwnerPawn->GetHeroCameraComponent(), TEXT("HeroCameraComponent"));
    RequireObject(UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get()), TEXT("AscenduntCameraComponent"));
    RequireObject(OwnerPawn->GetAscenduntHealthComponent(), TEXT("AscenduntHealthComponent"));
    RequireObject(OwnerPawn->GetWeaponEquipmentComponent(), TEXT("WeaponEquipmentComponent"));
    RequireObject(OwnerPawn->GetMesh(), TEXT("MainSkeletalMeshComponent"));

    if (PawnData)
    {
        RequireObject(PawnData->InputConfig.Get(), TEXT("PawnData.InputConfig"));
        if (!PawnData->DefaultCameraMode)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[HeroInit] Missing static dependency 'PawnData.DefaultCameraMode'. Pawn=%s PawnData=%s"),
                *GetNameSafe(OwnerPawn.Get()),
                *GetNameSafe(PawnData));
            bValid = false;
        }
    }

    if (USkeletalMeshComponent* MeshComp = OwnerPawn->GetMesh())
    {
        if (!MeshComp->GetSkeletalMeshAsset() || !MeshComp->GetAnimClass())
        {
            UE_LOG(LogTemp, Error,
                TEXT("[HeroInit] Main animation configuration is incomplete. Pawn=%s Mesh=%s SkeletalMesh=%s AnimClass=%s"),
                *GetNameSafe(OwnerPawn.Get()),
                *GetNameSafe(MeshComp),
                *GetNameSafe(MeshComp->GetSkeletalMeshAsset()),
                *GetNameSafe(MeshComp->GetAnimClass()));
            bValid = false;
        }
    }

    return bValid;
}

bool UHeroInitComponent::TryEnterGASInitialized()
{
    if (!bStaticConfigurationChecked || !bStaticConfigurationValid)
    {
        return false;
    }

    check(OwnerPawn.IsValid());
    check(CachedPS.IsValid());

    UAscenduntAbilitySystemComponent* ASC = CachedPS->GetAscenduntAbilitySystemComponent();
    if (!ASC)
    {
        // ASC is a PlayerState default subobject. Once PlayerState exists, a null ASC is
        // a structural/configuration failure rather than a replication dependency.
        UE_LOG(LogTemp, Error,
            TEXT("[HeroInit] PlayerState exists but ASC is null. Pawn=%s PS=%s"),
            *GetNameSafe(OwnerPawn.Get()),
            *GetNameSafe(CachedPS.Get()));
        return false;
    }

    CachedASC = ASC;

    const bool bActorInfoMatches =
        ASC->AbilityActorInfo.IsValid() &&
        ASC->GetOwnerActor() == CachedPS.Get() &&
        ASC->GetAvatarActor() == OwnerPawn.Get();

    if (!bActorInfoMatches)
    {
        ASC->InitAbilityActorInfo(CachedPS.Get(), OwnerPawn.Get());
    }

    if (!ASC->AbilityActorInfo.IsValid() ||
        ASC->GetOwnerActor() != CachedPS.Get() ||
        ASC->GetAvatarActor() != OwnerPawn.Get())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[HeroInit] Failed to initialize ASC ActorInfo. Pawn=%s PS=%s Owner=%s Avatar=%s"),
            *GetNameSafe(OwnerPawn.Get()),
            *GetNameSafe(CachedPS.Get()),
            *GetNameSafe(ASC->GetOwnerActor()),
            *GetNameSafe(ASC->GetAvatarActor()));
        return false;
    }

    // GAS montage tasks must target the gameplay mesh rather than auxiliary meshes.
    ASC->AbilityActorInfo->SkeletalMeshComponent = OwnerPawn->GetMesh();

    if (OwnerPawn->HasAuthority())
    {
        // The server owns AbilitySet application. Failure here is not something a timer can
        // fix: the PlayerState/ASC/PawnData relationship is already fully available.
        if (!CachedPS->ApplyPawnDataAbilitySets())
        {
            return false;
        }
    }
    else if (!CachedPS->IsPawnDataApplied())
    {
        // Asynchronous replicated completion marker. OnRep_AppliedPawnData wakes Evaluate().
        return false;
    }

    if (OwnerPawn->HasAuthority())
    {
        // Contribution coefficients and the persistent UltimateSet are hero data, so bind them
        // only after this PawnData has actually been applied to the ASC.
        CachedPS->InitializeCombatForCurrentPawnData();
    }

    if (!bOnSpawnAbilitiesActivatedForAvatar)
    {
        ASC->TryActivateAbilitiesOnSpawn();
        bOnSpawnAbilitiesActivatedForAvatar = true;
    }

    CurrentState = AscenduntGameplayTags::InitState_GASInitialized;

    UE_LOG(LogTemp, Log,
        TEXT("[HeroInit] %s -> GASInitialized"),
        *GetNameSafe(OwnerPawn.Get()));

    return true;
}

void UHeroInitComponent::PrepareGameplayDependencies()
{
    check(CurrentState == AscenduntGameplayTags::InitState_GASInitialized);
    check(OwnerPawn.IsValid());
    check(CachedASC.IsValid());

    // Preparation may happen in any order, but user control is not committed until every
    // required dependency below reports ready.
    if (!bAnimationInitialized)
    {
        bAnimationInitialized = OwnerPawn->TryInitializeAnimationForGameplay();
    }

    if (!bHealthCompInitialized)
    {
        if (UAscenduntHealthComponent* HealthComp = OwnerPawn->GetAscenduntHealthComponent())
        {
            bHealthCompInitialized = HealthComp->InitializeWithAbilitySystem(CachedASC.Get());
            if (bHealthCompInitialized)
            {
                BindToHealthComponent();
            }
        }
    }

    if (UWeaponEquipmentComponent* EquipmentComp = OwnerPawn->GetWeaponEquipmentComponent())
    {
        if (OwnerPawn->HasAuthority())
        {
            EquipmentComp->ServerEnsureDefaultWeapons();

            if (!EquipmentComp->GetCurrentWeapon() && EquipmentComp->GetWeaponInventory().Num() > 0)
            {
                // Visual-only companion slots are rejected by ServerEquipWeapon(); try each
                // inventory entry until the actual gameplay weapon becomes CurrentWeapon.
                for (AAscenduntWeapon* Weapon : EquipmentComp->GetWeaponInventory())
                {
                    EquipmentComp->ServerEquipWeapon(Weapon);
                    if (EquipmentComp->GetCurrentWeapon())
                    {
                        break;
                    }
                }
            }
        }
    }
}

bool UHeroInitComponent::CanEnterGameplayReady() const
{
    if (CurrentState != AscenduntGameplayTags::InitState_GASInitialized || !bStaticConfigurationValid)
    {
        return false;
    }

    check(OwnerPawn.IsValid());
    check(CachedPS.IsValid());
    check(CachedASC.IsValid());

    if (!bAnimationInitialized || !bHealthCompInitialized)
    {
        return false;
    }

    const UWeaponEquipmentComponent* EquipmentComp = OwnerPawn->GetWeaponEquipmentComponent();
    if (!EquipmentComp || !EquipmentComp->IsWeaponLoadoutReady())
    {
        return false;
    }

    if (!RequiresLocalPlayerInitialization())
    {
        return true;
    }

    // Local user-facing state is committed only after all local prerequisites exist.
    if (!OwnerPawn->IsLocallyControlled() || !OwnerPawn->InputComponent)
    {
        return false;
    }

    const UHeroInputComponent* InputComp = OwnerPawn->GetHeroInputComponent();
    if (!InputComp || !InputComp->CanInitializePlayerInput(OwnerPawn->InputComponent))
    {
        return false;
    }

    return CanInitializeLocalCamera();
}

bool UHeroInitComponent::CanInitializeLocalCamera() const
{
    if (!OwnerPawn.IsValid() || !CachedPS.IsValid())
    {
        return false;
    }

    const UHeroPawnData* PawnData = CachedPS->GetPawnData();
    return
        OwnerPawn->IsLocallyControlled() &&
        PawnData && PawnData->DefaultCameraMode &&
        OwnerPawn->GetHeroCameraComponent() &&
        UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get());
}

bool UHeroInitComponent::InitializeLocalCamera()
{
    if (bCameraInitialized)
    {
        return true;
    }

    if (!CanInitializeLocalCamera())
    {
        return false;
    }

    UAscenduntCameraComponent* CameraComponent =
        UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get());
    UHeroCameraComponent* HeroCamera = OwnerPawn->GetHeroCameraComponent();

    CameraComponent->DetermineCameraModeDelegate.BindUObject(
        HeroCamera,
        &UHeroCameraComponent::DetermineCameraMode);

    bCameraInitialized = CameraComponent->DetermineCameraModeDelegate.IsBound();
    return bCameraInitialized;
}

bool UHeroInitComponent::EnterGameplayReady()
{
    if (!OwnerPawn.IsValid())
    {
        return false;
    }

    const bool bNeedsLocalInitialization = RequiresLocalPlayerInitialization();

    // Movement attributes are prepared before input is enabled, so the first user movement
    // already observes the ASC-driven movement values.
    OwnerPawn->HandleGameplayReady();

    if (bNeedsLocalInitialization)
    {
        if (!InitializeLocalCamera())
        {
            OwnerPawn->HandleGameplayDeinitialized();
            return false;
        }

        if (!bInputCompInitialized)
        {
            UHeroInputComponent* InputComp = OwnerPawn->GetHeroInputComponent();
            bInputCompInitialized = InputComp &&
                InputComp->InitializePlayerInput(OwnerPawn->InputComponent);
        }

        if (!bInputCompInitialized)
        {
            if (UAscenduntCameraComponent* CameraComponent =
                UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get()))
            {
                CameraComponent->DetermineCameraModeDelegate.Unbind();
            }
            bCameraInitialized = false;
            OwnerPawn->HandleGameplayDeinitialized();
            return false;
        }
    }
    else
    {
        bInputCompInitialized = true;
        bCameraInitialized = true;
    }

    CurrentState = AscenduntGameplayTags::InitState_GameplayReady;

    UE_LOG(LogTemp, Log,
        TEXT("[HeroInit] %s -> GameplayReady"),
        *GetNameSafe(OwnerPawn.Get()));

    OnGameplayReady.Broadcast(OwnerPawn.Get());
    return true;
}

bool UHeroInitComponent::RequiresLocalPlayerInitialization() const
{
    if (!OwnerPawn.IsValid() || OwnerPawn->GetNetMode() != NM_Client)
    {
        return false;
    }

    if (OwnerPawn->GetLocalRole() == ROLE_AutonomousProxy || OwnerPawn->IsLocallyControlled())
    {
        return true;
    }

    // Role/Controller references can lag behind PlayerState. Matching the local PC's
    // PlayerState prevents an owning Pawn from being misclassified as a simulated proxy.
    if (CachedPS.IsValid())
    {
        if (const UWorld* World = OwnerPawn->GetWorld())
        {
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                const APlayerController* PC = It->Get();
                if (PC && PC->IsLocalController() &&
                    PC->GetPlayerState<AAscenduntPlayerState>() == CachedPS.Get())
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void UHeroInitComponent::BindToPlayerState(AAscenduntPlayerState* PS)
{
    if (!PS || !OwnerPawn.IsValid())
    {
        return;
    }

    // Idempotent by design. "CachedPS == PS" does not imply these bindings exist: an
    // earlier call may have happened before the component had all local prerequisites.
    if (!PawnDataAvailHandle.IsValid())
    {
        PawnDataAvailHandle = PS->OnPawnDataAvailable.AddUObject(
            this,
            &UHeroInitComponent::HandlePawnDataAvailable);
    }

    if (!PawnDataAppliedHandle.IsValid())
    {
        PawnDataAppliedHandle = PS->OnPawnDataApplied.AddUObject(
            this,
            &UHeroInitComponent::HandlePawnDataApplied);
    }
}

void UHeroInitComponent::UnbindFromPlayerState()
{
    if (AAscenduntPlayerState* PS = CachedPS.Get())
    {
        if (PawnDataAvailHandle.IsValid())
        {
            PS->OnPawnDataAvailable.Remove(PawnDataAvailHandle);
        }
        if (PawnDataAppliedHandle.IsValid())
        {
            PS->OnPawnDataApplied.Remove(PawnDataAppliedHandle);
        }
    }

    PawnDataAvailHandle.Reset();
    PawnDataAppliedHandle.Reset();
}

void UHeroInitComponent::HandlePawnDataAvailable()
{
    Evaluate();
}

void UHeroInitComponent::HandlePawnDataApplied()
{
    Evaluate();
}

void UHeroInitComponent::BindToWeaponEquipment()
{
    if (!OwnerPawn.IsValid() || OwnerPawn->GetNetMode() != NM_Client)
    {
        return;
    }

    UWeaponEquipmentComponent* EquipmentComp = OwnerPawn->GetWeaponEquipmentComponent();
    if (!EquipmentComp)
    {
        return;
    }

    if (!CurrentWeaponChangedHandle.IsValid())
    {
        CurrentWeaponChangedHandle = EquipmentComp->OnCurrentWeaponChanged.AddUObject(
            this,
            &UHeroInitComponent::HandleCurrentWeaponChanged);
    }

    if (!WeaponInventoryChangedHandle.IsValid())
    {
        WeaponInventoryChangedHandle = EquipmentComp->OnWeaponInventoryChanged.AddUObject(
            this,
            &UHeroInitComponent::HandleWeaponInventoryChanged);
    }
}

void UHeroInitComponent::UnbindFromWeaponEquipment()
{
    AHeroCharacter* Pawn = OwnerPawn.Get();
    if (!Pawn)
    {
        Pawn = Cast<AHeroCharacter>(GetOwner());
    }

    if (Pawn)
    {
        if (UWeaponEquipmentComponent* EquipmentComp = Pawn->GetWeaponEquipmentComponent())
        {
            if (CurrentWeaponChangedHandle.IsValid())
            {
                EquipmentComp->OnCurrentWeaponChanged.Remove(CurrentWeaponChangedHandle);
            }
            if (WeaponInventoryChangedHandle.IsValid())
            {
                EquipmentComp->OnWeaponInventoryChanged.Remove(WeaponInventoryChangedHandle);
            }
        }
    }

    CurrentWeaponChangedHandle.Reset();
    WeaponInventoryChangedHandle.Reset();
}

void UHeroInitComponent::HandleCurrentWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PrevWeapon)
{
    Evaluate();
}

void UHeroInitComponent::HandleWeaponInventoryChanged()
{
    Evaluate();
}

void UHeroInitComponent::BindToHealthComponent()
{
    if (bDeathEventBound || !OwnerPawn.IsValid())
    {
        return;
    }

    if (UAscenduntHealthComponent* HealthComp = OwnerPawn->GetAscenduntHealthComponent())
    {
        HealthComp->SetAutoFinishDeath(false);
        HealthComp->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleDeathStarted);
        HealthComp->OnDeathFinished.AddUniqueDynamic(this, &ThisClass::HandleDeathFinished);
        bDeathEventBound = true;
    }
}

void UHeroInitComponent::UnbindFromHealthComponent()
{
    AHeroCharacter* Pawn = OwnerPawn.Get();
    if (!Pawn)
    {
        Pawn = Cast<AHeroCharacter>(GetOwner());
    }

    if (Pawn)
    {
        if (UAscenduntHealthComponent* HealthComp = Pawn->GetAscenduntHealthComponent())
        {
            HealthComp->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
            HealthComp->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleDeathFinished);
            HealthComp->SetAutoFinishDeath(true);
        }
    }

    bDeathEventBound = false;
}

void UHeroInitComponent::HandleDeathStarted(AActor* OwningActor)
{
    AHeroCharacter* Pawn = OwnerPawn.Get();
    if (!Pawn || OwningActor != Pawn)
    {
        return;
    }

    const bool bIsPvE = GetWorld() && GetWorld()->GetGameState<AAscPvEGameState>() != nullptr;
    if (bIsPvE)
    {
        if (Pawn->HasAuthority())
        {
            if (UAscPvEDownedComponent* DownedComp = Pawn->GetPvEDownedComponent())
            {
                DownedComp->EnterDowned();
            }
        }
        return;
    }

    Pawn->EnterDeathRagdoll();

    if (Pawn->HasAuthority())
    {
        if (UAscenduntHealthComponent* HealthComp = Pawn->GetAscenduntHealthComponent())
        {
            HealthComp->FinishDeath();
        }
    }
}

void UHeroInitComponent::HandleDeathFinished(AActor* OwningActor)
{
    AHeroCharacter* Pawn = OwnerPawn.Get();
    if (!Pawn || OwningActor != Pawn)
    {
        return;
    }

    Pawn->EnterDeathRagdoll();

    if (!Pawn->HasAuthority())
    {
        return;
    }

    if (AAscPvEGameMode* PvEGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAscPvEGameMode>() : nullptr)
    {
        PvEGameMode->HandleHeroFullDeath(Pawn);
        return;
    }

    if (AMatchGameMode* MatchGM = GetWorld() ? GetWorld()->GetAuthGameMode<AMatchGameMode>() : nullptr)
    {
        MatchGM->HandleHeroDeath(Pawn);
    }
}

void UHeroInitComponent::DeinitializeForPawnReplacement(bool bPreserveHeroState)
{
    DeinitializePawn(bPreserveHeroState);
}

void UHeroInitComponent::Shutdown()
{
    DeinitializePawn(/*bPreserveHeroState*/ false);
}

void UHeroInitComponent::DeinitializePawn(bool bPreserveHeroState)
{
    if (bDeinitialized)
    {
        return;
    }

    bDeinitialized = true;

    AHeroCharacter* Pawn = OwnerPawn.Get();
    if (!Pawn)
    {
        Pawn = Cast<AHeroCharacter>(GetOwner());
        OwnerPawn = Pawn;
    }

    AAscenduntPlayerState* PS = CachedPS.Get();
    if (!PS && Pawn)
    {
        PS = Pawn->GetPlayerState<AAscenduntPlayerState>();
        CachedPS = PS;
    }

    UAscenduntAbilitySystemComponent* ASC = CachedASC.Get();
    if (!ASC && PS)
    {
        ASC = PS->GetAscenduntAbilitySystemComponent();
        CachedASC = ASC;
    }

    if (ASC)
    {
        ASC->CancelAllAbilities();
        ASC->ClearAbilityInput();
    }

    if (Pawn)
    {
        Pawn->HandleGameplayDeinitialized();

        if (UHeroInputComponent* InputComp = Pawn->GetHeroInputComponent())
        {
            InputComp->UninitializePlayerInput();
        }

        if (UAscenduntCameraComponent* CameraComponent =
            UAscenduntCameraComponent::FindCameraComponent(Pawn))
        {
            CameraComponent->DetermineCameraModeDelegate.Unbind();
        }

        UnbindFromHealthComponent();

        if (UAscenduntHealthComponent* HealthComp = Pawn->GetAscenduntHealthComponent())
        {
            HealthComp->UninitializeFromAbilitySystem();
        }
    }

    if (Pawn && Pawn->HasAuthority())
    {
        if (UWeaponEquipmentComponent* EquipmentComp = Pawn->GetWeaponEquipmentComponent())
        {
            EquipmentComp->ServerClearWeaponsFromASC(/*bDestroyWeapons*/ true);
        }

        if (PS && !bPreserveHeroState)
        {
            PS->RemovePawnDataAbilitySets();
        }
    }

    if (ASC && Pawn && ASC->GetAvatarActor() == Pawn)
    {
        ASC->ClearActorInfo();
    }

    UnbindFromPlayerState();
    UnbindFromWeaponEquipment();
    ResetRuntimeState();
}

void UHeroInitComponent::ResetRuntimeState()
{
    CachedPS = nullptr;
    CachedASC = nullptr;
    OwnerPawn = nullptr;

    bStaticConfigurationChecked = false;
    bStaticConfigurationValid = false;
    bAnimationInitialized = false;
    bHealthCompInitialized = false;
    bInputCompInitialized = false;
    bCameraInitialized = false;
    bOnSpawnAbilitiesActivatedForAvatar = false;
    bDeathEventBound = false;

    CurrentState = FGameplayTag::EmptyTag;
}
