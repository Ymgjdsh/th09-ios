#pragma once

#ifndef TH095_MODERN_IOS
#error "This header is only for the modern iOS build."
#endif

namespace th095
{
namespace modern
{
namespace ios
{

// Presents the native iOS text-entry dialog.  The completion callback invokes
// ApplyIosCheatCode on the UIKit main thread after the user taps OK.
void ShowCheatCodeDialog();

// Applies and persists the supported mobile cheat.  Returns false for an
// empty, unknown, or otherwise invalid code.
bool ApplyIosCheatCode(const char *code);

} // namespace ios
} // namespace modern
} // namespace th095
