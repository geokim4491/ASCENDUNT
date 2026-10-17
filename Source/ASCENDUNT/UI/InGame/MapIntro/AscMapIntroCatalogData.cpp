#include "AscMapIntroCatalogData.h"

#include "Misc/PackageName.h"

namespace AscMapIntroCatalogPrivate
{
    FString NormalizeMapId(const FString& MapPathOrName)
    {
        FString ShortName = FPackageName::GetShortName(MapPathOrName);

        // PIE world names can look like UEDPIE_0_L_Arena01.
        if (ShortName.StartsWith(TEXT("UEDPIE_")))
        {
            const int32 PrefixEnd = ShortName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7);
            if (PrefixEnd != INDEX_NONE)
            {
                ShortName.RightChopInline(PrefixEnd + 1, EAllowShrinking::No);
            }
        }

        return ShortName;
    }
}

bool UAscMapIntroCatalogData::FindPresentation(const FString& MapPathOrName, FAscMapIntroPresentation& OutPresentation) const
{
    if (const FAscMapIntroPresentation* Found = FindPresentationNative(MapPathOrName))
    {
        OutPresentation = *Found;
        return true;
    }

    return false;
}

UTexture2D* UAscMapIntroCatalogData::GetCharacterSelectImageForTeam(const FString& MapPathOrName, uint8 TeamId) const
{
    if (TeamId == 0)
    {
        return nullptr;
    }

    const FAscMapIntroPresentation* Presentation = FindPresentationNative(MapPathOrName);
    if (!Presentation)
    {
        return nullptr;
    }

    const FAscCharacterSelectSidePresentation& SidePresentation = Presentation->CharacterSelectPresentation;

    if (TeamId == SidePresentation.AttackingTeamId)
    {
        return SidePresentation.AttackerBackgroundImage;
    }

    if (TeamId == SidePresentation.DefendingTeamId)
    {
        return SidePresentation.DefenderBackgroundImage;
    }

    return nullptr;
}

const FAscMapIntroPresentation* UAscMapIntroCatalogData::FindPresentationNative(const FString& MapPathOrName) const
{
    const FName NormalizedMapId(*AscMapIntroCatalogPrivate::NormalizeMapId(MapPathOrName));

    return Maps.FindByPredicate(
        [NormalizedMapId](const FAscMapIntroPresentation& Entry)
        {
            return Entry.MapId == NormalizedMapId;
        });
}
