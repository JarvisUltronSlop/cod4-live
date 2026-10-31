#pragma once
#include <universal/q_shared.h>
#include <qcommon/qcommon.h>

namespace live
{
inline bool __cdecl FixedAimValue(dvar_s* dvar, DvarValue value)
{
    return Dvar_ValuesEqual(dvar->type, value, dvar->reset) != 0;
}

inline void LockAimTuning(const dvar_t* dvar)
{
    // Registration can adopt an earlier +set/config value. Restore the classic
    // default first. ROM protects console/scripts; the domain also protects the
    // engine's DevGUI setter, which intentionally bypasses ROM for other dvars.
    auto* fixed = const_cast<dvar_t*>(dvar);
    Dvar_Reset(fixed, DVAR_SOURCE_INTERNAL);
    fixed->flags |= DVAR_ROM;
    Dvar_SetDomainFunc(fixed, FixedAimValue);
}
}
