#include "AscenduntWorldSettings.h"

const UHeroPawnData* AAscenduntWorldSettings::GetEditorTestPawnData() const
{
#if WITH_EDITORONLY_DATA
    return EditorTestPawnData;
#else
    return nullptr;
#endif
}
