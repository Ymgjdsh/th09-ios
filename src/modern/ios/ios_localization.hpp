#pragma once

#include "ios_touch.hpp"

namespace th095
{
namespace modern
{
namespace ios
{

struct LocalizedDialogueText
{
    const char *lines[2];
    int lineCount;
    int originalLine;
};

void SetDialogueMessageFile(const char *path);
void ResetLocalizationRuntime();
bool FindLocalizedDialogue(int script, int time, const char *japaneseCp932,
                           int requestedLine, LocalizedDialogueText *result);

// Localized labels used by the non-title screens.  The original title page
// remains untouched; these helpers are only queried by Replay/Result/Music
// Room when an alternate language is active.
const char *MusicRoomTitle(int index, const char *original);
const char *MusicRoomComment(int index, int line, bool unlocked, const char *original);
const char *MusicRoomLabel(const char *key);
const char *ResultLabel(const char *key);
const char *ResultMenuLabel(int scriptIndex);
const char *ResultCharacterName(int index, const char *original);
const char *ReplayLabel(const char *key);
const char *ReplayCharacterName(int index, const char *original);
const char *ReplayDifficultyName(int index, const char *original);

} // namespace ios
} // namespace modern
} // namespace th095
