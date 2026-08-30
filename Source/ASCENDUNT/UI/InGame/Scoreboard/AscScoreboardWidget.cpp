#include "AscScoreboardWidget.h"

#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntUltimateSet.h"
#include "ASCENDUNT/AbilitySystem/Components/AscCombatContributionComponent.h"
#include "ASCENDUNT/HeroSelection/AscHeroRosterData.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/UI/InGame/Scoreboard/AscScoreboardPlayerRowWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"

void UAscScoreboardWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UAscScoreboardWidget::InitializeScoreboard(AAscenduntPlayerController* InOwningPC)
{
    OwningAscPC = InOwningPC;
}

void UAscScoreboardWidget::SetScoreboardVisible(bool bVisible)
{
    RefreshAccumulator = 0.0f;

    if (bVisible)
    {
        RefreshScoreboard();
        // Scoreboard never captures mouse/keyboard input. Gameplay keeps running while TAB is held.
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UAscScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
    {
        return;
    }

    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator >= RefreshInterval)
    {
        RefreshAccumulator = 0.0f;
        RefreshScoreboard();
    }
}

void UAscScoreboardWidget::RefreshScoreboard()
{
    BuildEntries(LastEntries, LocalWeaponStats);
    RefreshRows(LastEntries);

    if (Txt_WeaponAccuracy)
    {
        Txt_WeaponAccuracy->SetText(FText::Format(
            FText::FromString(TEXT("{0}%")),
            FText::AsNumber(FMath::RoundToInt(LocalWeaponStats.WeaponAccuracyPercent))));
    }

    if (Txt_CriticalHitAccuracy)
    {
        Txt_CriticalHitAccuracy->SetText(FText::Format(
            FText::FromString(TEXT("{0}%")),
            FText::AsNumber(FMath::RoundToInt(LocalWeaponStats.CriticalHitAccuracyPercent))));
    }

    BP_OnScoreboardRefreshed();
}

void UAscScoreboardWidget::BuildEntries(
    TArray<FAscScoreboardPlayerEntry>& OutEntries,
    FAscScoreboardLocalWeaponStats& OutLocalStats) const
{
    OutEntries.Reset();
    OutLocalStats = FAscScoreboardLocalWeaponStats();

    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    if (!GameState)
    {
        return;
    }

    TArray<AAscenduntPlayerState*> Players;
    Players.Reserve(GameState->PlayerArray.Num());
    for (APlayerState* RawPS : GameState->PlayerArray)
    {
        if (AAscenduntPlayerState* AscPS = Cast<AAscenduntPlayerState>(RawPS))
        {
            Players.Add(AscPS);
        }
    }

    Players.Sort([](const AAscenduntPlayerState& A, const AAscenduntPlayerState& B)
    {
        return A.GetPlayerId() < B.GetPlayerId();
    });

    for (AAscenduntPlayerState* PlayerState : Players)
    {
        OutEntries.Add(BuildEntry(PlayerState));
    }

    const AAscenduntPlayerState* LocalPS = OwningAscPC
        ? OwningAscPC->GetPlayerState<AAscenduntPlayerState>()
        : nullptr;

    if (LocalPS)
    {
        if (const UAscCombatContributionComponent* Contribution = LocalPS->GetCombatContributionComponent())
        {
            OutLocalStats.WeaponShotsFired = Contribution->GetWeaponShotsFired();
            OutLocalStats.WeaponHits = Contribution->GetWeaponHits();
            OutLocalStats.CriticalWeaponHits = Contribution->GetCriticalWeaponHits();
            OutLocalStats.WeaponAccuracyPercent = Contribution->GetWeaponAccuracyPercent();
            OutLocalStats.CriticalHitAccuracyPercent = Contribution->GetCriticalHitAccuracyPercent();
        }
    }
}

FAscScoreboardPlayerEntry UAscScoreboardWidget::BuildEntry(AAscenduntPlayerState* PlayerState) const
{
    FAscScoreboardPlayerEntry Entry;
    Entry.PlayerState = PlayerState;
    if (!PlayerState)
    {
        return Entry;
    }

    const FString DisplayName = PlayerState->GetDisplayName().IsEmpty()
        ? PlayerState->GetPlayerName()
        : PlayerState->GetDisplayName();
    Entry.PlayerName = FText::FromString(DisplayName);
    Entry.HeroId = PlayerState->GetConfirmedHeroId();
    Entry.bIsLocalPlayer = OwningAscPC && OwningAscPC->GetPlayerState<AAscenduntPlayerState>() == PlayerState;

    if (const UAscHeroRosterData* RosterData = OwningAscPC ? OwningAscPC->GetHeroRosterData() : nullptr)
    {
        if (const FAscHeroDefinition* Definition = RosterData->FindHeroDefinition(Entry.HeroId))
        {
            Entry.Portrait = Definition->Portrait;
        }
        else if (const UHeroPawnData* PawnData = PlayerState->GetPawnData())
        {
            for (const FAscHeroDefinition& Def : RosterData->HeroDefinitions)
            {
                if (Def.PawnData == PawnData)
                {
                    Entry.Portrait = Def.Portrait;
                    Entry.HeroId = Def.HeroId;
                    break;
                }
            }
        }
    }

    if (const UAscCombatContributionComponent* Contribution = PlayerState->GetCombatContributionComponent())
    {
        Entry.MonsterKills = Contribution->GetTotalKills();
        Entry.DownedCount = Contribution->GetDownedCount();
        Entry.ReviveCount = Contribution->GetReviveCount();
        Entry.DamageDealt = Contribution->GetTotalDamageDealt();
        Entry.HealingDone = Contribution->GetTotalHealingDone();
        Entry.HealingReceived = Contribution->GetTotalHealingReceived();
    }

    if (const UAscenduntUltimateSet* UltimateSet = PlayerState->GetUltimateSet())
    {
        const float MaxCharge = FMath::Max(1.0f, UltimateSet->GetMaxUltimateCharge());
        Entry.UltimateChargePercent = FMath::Clamp(UltimateSet->GetUltimateCharge() / MaxCharge, 0.0f, 1.0f);
    }

    return Entry;
}

void UAscScoreboardWidget::RefreshRows(const TArray<FAscScoreboardPlayerEntry>& Entries)
{
    if (!Panel_PlayerRows || !PlayerRowWidgetClass)
    {
        return;
    }

    while (SpawnedRows.Num() < Entries.Num())
    {
        UAscScoreboardPlayerRowWidget* NewRow = CreateWidget<UAscScoreboardPlayerRowWidget>(
            GetOwningPlayer(),
            PlayerRowWidgetClass);
        if (!NewRow)
        {
            break;
        }

        Panel_PlayerRows->AddChild(NewRow);
        SpawnedRows.Add(NewRow);
    }

    while (SpawnedRows.Num() > Entries.Num())
    {
        UAscScoreboardPlayerRowWidget* Row = SpawnedRows.Pop();
        if (Row)
        {
            Row->RemoveFromParent();
        }
    }

    const int32 Count = FMath::Min(SpawnedRows.Num(), Entries.Num());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (UAscScoreboardPlayerRowWidget* Row = SpawnedRows[Index])
        {
            Row->SetEntry(Entries[Index]);
        }
    }
}
