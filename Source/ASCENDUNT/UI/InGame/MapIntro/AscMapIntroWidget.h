#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscMapIntroWidget.generated.h"

class AMatchGameState;
class UAscMapIntroCatalogData;
class UAscMapIntroPlayerEntryWidget;
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * Map-intro content widget.
 *
 * The owning in-game HUD controls phase-level visibility and explicitly passes
 * the current MatchGameState through InitializeMapIntro(). This widget owns only
 * map presentation data and roster-entry rebuilding.
 */
UCLASS()
class ASCENDUNT_API UAscMapIntroWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Called by UAscInGameHUDWidget whenever its MatchGameState binding changes. */
    void InitializeMapIntro(AMatchGameState* InMatchGameState);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    void BindMatchGameState(AMatchGameState* InMatchGameState);
    void UnbindMatchGameState();
    void HandlePlayerRosterChanged();
    void HandleMatchGameStateChanged();

    void RefreshMapPresentation();
    void RebuildRoster();
    void RebuildTeamBox(UVerticalBox* TargetBox, const TArray<FString>& PlayerNames);

protected:
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TObjectPtr<UVerticalBox> VB_TeamOnePlayers;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TObjectPtr<UVerticalBox> VB_TeamTwoPlayers;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TObjectPtr<UImage> Img_MapBackground;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TObjectPtr<UTextBlock> Txt_MapName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TSubclassOf<UAscMapIntroPlayerEntryWidget> PlayerEntryWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Map Intro")
    TObjectPtr<const UAscMapIntroCatalogData> MapIntroCatalog;

private:
    TWeakObjectPtr<AMatchGameState> CachedMatchGameState;
    FString CachedPresentationMapPath;
};
