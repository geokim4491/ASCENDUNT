#include "AscWorldStatusLayoutData.h"

bool UAscWorldStatusLayoutData::FindWidgetLayout(
	EAscWorldStatusWidgetType WidgetType,
	FAscWorldStatusWidgetLayout& OutLayout) const
{
	if (const FAscWorldStatusWidgetLayout* Found = FindWidgetLayoutPtr(WidgetType))
	{
		OutLayout = *Found;
		return true;
	}

	return false;
}

const FAscWorldStatusWidgetLayout* UAscWorldStatusLayoutData::FindWidgetLayoutPtr(
	EAscWorldStatusWidgetType WidgetType) const
{
	return WidgetLayouts.Find(WidgetType);
}
