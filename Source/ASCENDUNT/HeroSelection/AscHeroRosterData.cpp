#include "AscHeroRosterData.h"

#include "ASCENDUNT/Character/Hero/HeroPawnData.h"

FAscHeroDefinition UAscHeroRosterData::GetHeroDefinitionByIndex(int32 Index) const
{
    return HeroDefinitions.IsValidIndex(Index) ? HeroDefinitions[Index] : FAscHeroDefinition();
}

const FAscHeroDefinition* UAscHeroRosterData::FindHeroDefinition(int32 HeroId) const
{
    return HeroDefinitions.FindByPredicate(
        [HeroId](const FAscHeroDefinition& Definition)
        {
            return Definition.HeroId == HeroId;
        });
}

const FAscHeroDefinition* UAscHeroRosterData::FindHeroDefinitionByPawnClass(const UClass* PawnClass) const
{
    if (!PawnClass)
    {
        return nullptr;
    }

    return HeroDefinitions.FindByPredicate(
        [PawnClass](const FAscHeroDefinition& Definition)
        {
            return Definition.PawnData && Definition.PawnData->PawnClass.Get() == PawnClass;
        });
}
