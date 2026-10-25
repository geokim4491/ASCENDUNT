#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "AscCharacterSelectionWidget.generated.h"

class AAscenduntPlayerController;
class AAscenduntPlayerState;
class AMatchGameState;
class UAscHeroRosterData;
class UAscMapIntroCatalogData;
class UButton;
class UImage;
class UTextBlock;

/** Reusable character-selection UI used by initial match select and respawn-room changes. */
UCLASS()
class ASCENDUNT_API UAscCharacterSelectionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Called by PlayerController right after CreateWidget. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character Selection")
    void InitializeCharacterSelection(AAscenduntPlayerController* InOwningAscPC, EAscHeroSelectionContext InContext);

    /** Local preview/selection only. Does not call the server. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character Selection")
    void SelectHero(int32 HeroId);

    /** Confirm button path. Sends selected HeroId to server. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Character Selection")
    void ConfirmSelectedHero();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Character Selection")
    int32 GetSelectedHeroId() const { return SelectedHeroId; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Character Selection")
    EAscHeroSelectionContext GetSelectionContext() const { return SelectionContext; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    void BindMatchGameState();
    void BindOwningPlayerState();
    void UnbindOwningPlayerState();

    void HandleMatchGameStateChanged();
    void HandleOwningPlayerStateChanged();

    void RefreshRemainingTimeText();
    void RefreshCharacterSelectPresentation();
    FText GetHeroNameById(int32 HeroId) const;

    UFUNCTION()
    void HandleHero0Clicked();

    UFUNCTION()
    void HandleHero1Clicked();

    UFUNCTION()
    void HandleHero2Clicked();

    UFUNCTION()
    void HandleHero3Clicked();

    UFUNCTION()
    void HandleConfirmClicked();

    void RefreshSelectionText();

protected:
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UButton> Btn_Hero0;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UButton> Btn_Hero1;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UButton> Btn_Hero2;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UButton> Btn_Hero3;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UButton> Btn_Confirm;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UTextBlock> Txt_SelectedHero;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UTextBlock> Txt_RemainingTime;

    /**
     * Optional full-screen image in WBP_HeroSelection.
     * It is shown whenever the selection UI is open in a match and is resolved by SelectedMapPath + local TeamId.
     */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<UImage> Img_CharacterSelectBackground;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<AAscenduntPlayerController> OwningAscPC;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    EAscHeroSelectionContext SelectionContext = EAscHeroSelectionContext::InGameRespawnRoom;

    UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    int32 SelectedHeroId = INDEX_NONE;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<const UAscHeroRosterData> HeroRosterData;

    /** Reuses the same DA_MapIntroCatalog assigned to WBP_MapIntro. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Character Selection")
    TObjectPtr<const UAscMapIntroCatalogData> MapIntroCatalog;

private:
    TWeakObjectPtr<AMatchGameState> CachedMatchGameState;
    TWeakObjectPtr<AAscenduntPlayerState> CachedOwningPlayerState;
};
