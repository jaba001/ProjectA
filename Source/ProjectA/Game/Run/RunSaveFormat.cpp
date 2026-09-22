#include "Game/Run/RunSaveFormat.h"

bool FRunSaveFormat::Resolve(int32 Version, FRunSaveFormat& OutFormat)
{
    FRunSaveFormat Format;
    switch (Version)
    {
    case 1:
        Format.bLegacyOffline = true;
        break;
    case 2:
        break;
    case 3:
        Format.bRequiresCombat = Format.bSupportsCombat = Format.bRejectsCurrentCheckpoint = true;
        break;
    case 4:
        Format.bManaged = Format.bSupportsCombat = true;
        break;
    case 5:
        Format.bRequiresCombat = Format.bSupportsCombat = Format.bRequiresCurrentCheckpoint = true;
        break;
    case 6:
        Format.bLegacyOffline = Format.bRequiresCombat = Format.bSupportsCombat = Format.bRequiresCurrentCheckpoint = true;
        break;
    default:
        return false;
    }
    OutFormat = Format;
    return true;
}

bool FRunSaveFormat::IsManaged(int32 Version)
{
    FRunSaveFormat Format;
    return Resolve(Version, Format) && Format.bManaged;
}

int32 FRunSaveFormat::Select(bool bManaged, ERunIdentityOrigin Origin, ERunPhase Phase)
{
    if (bManaged) return 4;
    const bool bLegacyOffline = Origin == ERunIdentityOrigin::LegacyOffline;
    if (Phase == ERunPhase::Combat) return bLegacyOffline ? 6 : 5;
    return bLegacyOffline ? 1 : 2;
}
