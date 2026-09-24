#include "ios_localization.hpp"

#include "ios_dialogue_i18n.hpp"

#include <string.h>

namespace th095
{
namespace modern
{
namespace ios
{
namespace
{
char g_dialogueMessageFile[32] = {};

const char *BaseName(const char *path)
{
    if (path == NULL) return "";
    const char *base = path;
    for (const char *cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\\') base = cursor + 1;
    }
    return base;
}
}

void SetDialogueMessageFile(const char *path)
{
    const char *base = BaseName(path);
    strncpy(g_dialogueMessageFile, base, sizeof(g_dialogueMessageFile) - 1);
    g_dialogueMessageFile[sizeof(g_dialogueMessageFile) - 1] = '\0';
}

void ResetLocalizationRuntime()
{
    g_dialogueMessageFile[0] = '\0';
}

bool FindLocalizedDialogue(int script, int time, const char *japaneseCp932,
                           int requestedLine, LocalizedDialogueText *result)
{
    if (result == NULL || japaneseCp932 == NULL || GetLanguage() == IOS_LANGUAGE_JP)
        return false;

    const IosDialogueTranslationEntry *fallback = NULL;
    for (unsigned int index = 0; index < g_iosDialogueTranslationCount; ++index)
    {
        const IosDialogueTranslationEntry &entry = g_iosDialogueTranslations[index];
        if (entry.script != script || entry.time != time ||
            strcmp(entry.file, g_dialogueMessageFile) != 0)
            continue;

        // The VM can normalize control bytes differently between the first
        // and second line.  Keep an exact match as the fast path, then fall
        // back to the generated line identity so the second translated line
        // cannot leak the retail CP932 text.
        if (strcmp(entry.japaneseCp932, japaneseCp932) != 0)
        {
            if (fallback == NULL && entry.originalLine == requestedLine)
                fallback = &entry;
            continue;
        }

        const char *const *translated = GetLanguage() == IOS_LANGUAGE_ZH
                                            ? entry.chinese
                                            : entry.english;
        if (translated[0][0] == '\0') return false;
        result->lines[0] = translated[0];
        result->lines[1] = translated[1];
        result->lineCount = translated[1][0] == '\0' ? 1 : 2;
        result->originalLine = entry.originalLine;
        return true;
    }

    if (fallback != NULL)
    {
        const char *const *translated = GetLanguage() == IOS_LANGUAGE_ZH
                                            ? fallback->chinese
                                            : fallback->english;
        if (translated[0][0] == '\0') return false;
        result->lines[0] = translated[0];
        result->lines[1] = translated[1];
        result->lineCount = translated[1][0] == '\0' ? 1 : 2;
        result->originalLine = fallback->originalLine;
        return true;
    }
    return false;
}

namespace
{
// TH095 has 24 retail Music Room entries.  Keeping these labels in the iOS
// layer avoids changing the CP932 data file shared by the desktop ports.
const char *g_musicTitlesZh[24] = {
    "永夜抄 ～ 东方之夜", "幻视之夜 ～ 幽灵之眼", "蠢蠢秋月 ～ 昆虫之月",
    "夜雀的歌声 ～ 夜鸟", "已经只能听到歌声", "令人怀念的东方之血 ～ 旧世界",
    "普莱恩亚洲", "永夜的报应 ～ 不朽之夜", "少女绮想曲 ～ 梦境之战",
    "恋色魔法使", "灰姑娘笼 ～ 鹅妈妈", "狂气之瞳 ～ 看不见的月亮",
    "Voyage 1969", "千年幻想乡 ～ 月之历史", "竹取飞翔 ～ 疯狂公主",
    "Voyage 1970", "延展之灰 ～ 蓬莱人", "传达到月亮的不死之烟",
    "月见草", "永恒之梦 ～ 幽玄之槭树", "东方妖怪小町", "少女秘封俱乐部",
    "童祭 ～ Innocent Treasures", "月面旅行车"
};
const char *g_musicTitlesEn[24] = {
    "Imperishable Night ~ Eastern Night", "Illusionary Night ~ Ghostly Eyes",
    "Stirring Autumn Moon ~ Mooned Insect", "Song of the Night Sparrow ~ Night Bird",
    "I Only Listen to the Song", "Nostalgic Blood of the East ~ Old World",
    "Plain Asia", "Retribution for the Eternal Night ~ Imperishable Night",
    "Maiden's Capriccio ~ Dream Battle", "Love-colored Master Spark",
    "Cinderella Cage ~ Kagome-Kagome", "Lunatic Eyes ~ Invisible Full Moon",
    "Voyage 1969", "Millennium Fantasy ~ History of the Moon", "Flight of the Bamboo Cutter ~ Lunatic Princess",
    "Voyage 1970", "Extend Ash ~ Hourai Victim", "Reach for the Moon, Immortal Smoke",
    "Starmap Flower", "Eternal Dream ~ Mystic Maple", "Eastern Yokai Beauty",
    "Girls' Sealing Club", "Merry the Innocent Treasures", "Lunar Voyage"
};

// The retail Music Room comments are CP932 strings embedded in TH095.dat.
// These short, authored summaries keep the mobile pages readable in both
// alternate languages without ever feeding CP932 bytes to the UTF-8 font.
const char *g_musicCommentsZh[24][2] = {
    {"主标题曲，永恒之夜的序幕。", "夜色降临，幻想乡的月亮停止了转动。"},
    {"幽灵在林间徘徊的主题。", "冷冽的音色描绘出朦胧的幻视之夜。"},
    {"竹林小径上悄然响起的旋律。", "秋月与虫鸣交织，时间仿佛停在夜里。"},
    {"夜雀歌唱的妖怪之夜。", "轻快而神秘的旋律，带着夜鸟的回声。"},
    {"米斯蒂娅的主题曲。", "当歌声传来，黑暗中的方向便不再可靠。"},
    {"穿越旧世界的回忆之歌。", "古老的血脉在月光下重新苏醒。"},
    {"慧音老师守护历史的主题。", "亚洲的平原沐浴在静谧而温柔的月色中。"},
    {"永恒之夜逐渐失控的主题。", "不朽的月夜化作紧迫的追逐与报应。"},
    {"灵梦的主题曲。", "巫女的灵符划过夜空，梦境与现实交汇。"},
    {"魔理沙的主题曲。", "星屑般的魔法火花在夜色中不断迸发。"},
    {"爱丽丝的主题曲。", "人偶的舞步如童谣般精致，却透着一丝寂寞。"},
    {"铃仙的主题曲。", "疯狂的月瞳映照出无法逃离的满月。"},
    {"穿越时空的航行曲。", "1969年的旅程，将幻想与真实连接起来。"},
    {"永琳的主题曲。", "千年的幻想在月之历史中缓缓展开。"},
    {"辉夜的主题曲。", "竹取公主以辉夜之梦，向挑战者发出邀请。"},
    {"第二次月面航行曲。", "1970年的航迹延伸到更遥远的月面。"},
    {"藤原妹红的主题曲。", "灰烬中延展出的生命，诉说蓬莱人的宿命。"},
    {"不死之烟的主题曲。", "烟火直达月面，不死者的执念永不熄灭。"},
    {"月见草的短曲。", "夜风吹过草叶，留下清澈而孤独的余韵。"},
    {"永恒之梦的终曲。", "幽玄的槭树在梦与现实之间静静摇曳。"},
    {"妖怪小町的主题。", "熟悉的旋律为长夜画下轻盈的尾声。"},
    {"少女秘封俱乐部的回忆。", "两个少女在月下交换只属于她们的秘密。"},
    {"童祭的回望。", "遥远的记忆与未曾抵达的宝藏彼此呼应。"},
    {"月面旅行的收束曲。", "旅程结束，永夜仍在心中留下回响。"},
};
const char *g_musicCommentsEn[24][2] = {
    {"The title theme and the prologue to an imperishable night.", "Night falls, and the moon over Gensokyo stops moving."},
    {"A theme for spirits wandering through the forest.", "Cold tones paint a hazy night of ghostly visions."},
    {"A quiet melody along the path through the bamboo grove.", "Autumn moonlight and insects intertwine as time stands still."},
    {"A night of monsters, carried by the song of a night sparrow.", "A nimble, mysterious tune with an echo of wings."},
    {"Mystia's theme.", "Once the song begins, every direction in the dark becomes uncertain."},
    {"A song of memories from the old world.", "Ancient blood awakens again beneath the moon."},
    {"Keine's theme, guardian of history.", "The plain of Asia rests beneath a calm and tender moon."},
    {"The night of eternity beginning to lose control.", "An imperishable moon becomes a chase, and then a reckoning."},
    {"Reimu's theme.", "Ofuda cut across the sky where dreams and reality meet."},
    {"Marisa's theme.", "Star-like sparks of magic burst through the night."},
    {"Alice's theme.", "A doll's dance is as delicate as a nursery rhyme, and just as lonely."},
    {"Reisen's theme.", "Lunatic eyes reflect a full moon from which there is no escape."},
    {"A voyage through time.", "The journey of 1969 links the fantastic with the real."},
    {"Eirin's theme.", "A millennium of fantasy unfolds through the history of the moon."},
    {"Kaguya's theme.", "The Bamboo Princess offers a night of impossible challenges."},
    {"The second voyage to the moon.", "The trail of 1970 reaches farther across the lunar surface."},
    {"Fujiwara no Mokou's theme.", "Life extends from the ashes, bound to the fate of a Hourai immortal."},
    {"The theme of the immortal smoke.", "Fire rises to the moon, and an immortal's obsession never fades."},
    {"A brief theme for moon viewing.", "A night breeze through the grass leaves a clear, solitary afterglow."},
    {"The closing theme of an eternal dream.", "A mysterious maple tree sways between dream and reality."},
    {"The theme of the youkai district.", "A familiar melody gives the long night a light-footed ending."},
    {"A memory of the Secret Sealing Club.", "Two girls trade secrets beneath a moon meant only for them."},
    {"A look back at the festival.", "Distant memories answer the call of treasures never reached."},
    {"The final piece of the lunar journey.", "The voyage ends, but the imperishable night still echoes within."},
};

const char *g_musicLockedZh[7] = {
    "＊＊＊ 注意 ＊＊＊", "这首曲目尚未在游戏中出现。", "如果不想提前听到曲目或看到说明，",
    "建议现在离开 Music Room。", "", "按下确认后仍会播放曲目并显示说明。", "请注意。"};
const char *g_musicLockedEn[7] = {
    "*** Warning ***", "This track has not yet appeared in the game.", "If you do not want to hear it or read its comments early,",
    "we recommend leaving the Music Room now.", "", "Press Confirm to play it and show its comments anyway.", "Please take care."};

struct UiTranslation { const char *key; const char *zh; const char *en; };
const UiTranslation g_musicLabels[] = {
    {"now-playing", "正在播放", "Now Playing"},
    {"not-unlocked", "尚未解锁", "Not unlocked"},
    {"back", "返回", "Back"},
};
const UiTranslation g_resultLabels[] = {
    {"total-time", "总启动时间   %02d:%02d:%02d", "Total time       %02d:%02d:%02d"},
    {"total-playtime", "总游戏时间 %02d:%02d:%02d", "Total play time %02d:%02d:%02d"},
    {"play-count", "游戏次数           Easy   Normal   Hard   Luna   Extra   Total", "Play count          Easy   Normal   Hard   Luna   Extra   Total"},
    {"clear-count", "通关次数       %6d %6d %6d %6d %6d %6d", "Clears          %6d %6d %6d %6d %6d %6d"},
    {"continue-count", "续关次数       %6d %6d %6d %6d %6d %6d", "Continues       %6d %6d %6d %6d %6d %6d"},
    {"practice-count", "练习次数       %6d %6d %6d %6d %6d %6d", "Practice        %6d %6d %6d %6d %6d %6d"},
    {"stage-clear", "通关", "Clear"},
};
const UiTranslation g_replayLabels[] = {
    {"header", "编号   名称       日期   自机     难度", "No.   Name       Date  Player   Rank"},
    {"stage-score", "关卡       最后得分", "Stage    LastScore"},
    {"hi-score", "编号   名称     日期   自机得分", "No.   Name     Date   Player Score"},
};
const char *g_characterZh[] = {
    "灵梦＆紫", "魔理沙＆爱丽丝", "咲夜＆蕾米莉亚", "妖梦＆幽幽子", "博丽灵梦", "八云紫",
    "雾雨魔理沙", "爱丽丝", "十六夜咲夜", "蕾米莉亚", "魂魄妖梦", "西行寺幽幽子", "全角色合计"
};
const char *g_characterEn[] = {
    "Reimu & Yukari", "Marisa & Alice", "Sakuya & Remilia", "Youmu & Yuyuko", "Reimu", "Yukari",
    "Marisa", "Alice", "Sakuya", "Remilia", "Youmu", "Yuyuko", "All shots"
};
const char *g_difficultyEn[] = {"Easy", "Normal", "Hard", "Lunatic", "Extra"};
const char *g_difficultyZh[] = {"简单", "普通", "困难", "疯狂", "Extra"};
const char *g_resultMenuZh[] = {
    "最高分", "符卡练习", "其他统计", "返回标题",
    "简单", "普通", "困难", "疯狂", "Extra",
    "简单", "普通", "困难", "疯狂", "Extra", "全部",
};
const char *g_resultMenuEn[] = {
    "High Scores", "Spell Practice", "Other Stats", "Back to Title",
    "Easy", "Normal", "Hard", "Lunatic", "Extra",
    "Easy", "Normal", "Hard", "Lunatic", "Extra", "All",
};

const char *Lookup(const UiTranslation *items, unsigned int count, const char *key)
{
    if (key == NULL) return NULL;
    for (unsigned int i = 0; i < count; ++i)
        if (strcmp(items[i].key, key) == 0)
            return GetLanguage() == IOS_LANGUAGE_ZH ? items[i].zh : items[i].en;
    return NULL;
}
}

const char *MusicRoomTitle(int index, const char *original)
{
    if (GetLanguage() == IOS_LANGUAGE_JP || index < 0 || index >= 24) return original;
    return GetLanguage() == IOS_LANGUAGE_ZH ? g_musicTitlesZh[index] : g_musicTitlesEn[index];
}

const char *MusicRoomComment(int index, int line, bool unlocked, const char *original)
{
    if (GetLanguage() == IOS_LANGUAGE_JP || index < 0 || index >= 24 || line < 0 || line >= 7)
        return original != NULL ? original : "";
    if (!unlocked)
        return GetLanguage() == IOS_LANGUAGE_ZH ? g_musicLockedZh[line] : g_musicLockedEn[line];
    // The retail file has up to seven lines per track.  The authored mobile
    // translation currently has two; return an empty string for the remaining
    // lines so the draw pass actively clears stale text instead of falling
    // back to CP932 bytes from the previous song.
    if (line >= 2)
        return "";
    const char *translated = GetLanguage() == IOS_LANGUAGE_ZH
                                 ? g_musicCommentsZh[index][line]
                                 : g_musicCommentsEn[index][line];
    return translated != NULL && translated[0] != '\0' ? translated : "";
}

const char *MusicRoomLabel(const char *key)
{
    const char *value = Lookup(g_musicLabels, sizeof(g_musicLabels) / sizeof(g_musicLabels[0]), key);
    return value != NULL ? value : key;
}

const char *ResultLabel(const char *key)
{
    const char *value = Lookup(g_resultLabels, sizeof(g_resultLabels) / sizeof(g_resultLabels[0]), key);
    return value != NULL ? value : key;
}

const char *ResultMenuLabel(int scriptIndex)
{
    if (GetLanguage() == IOS_LANGUAGE_JP || scriptIndex < 0 || scriptIndex >= 15)
        return NULL;
    return GetLanguage() == IOS_LANGUAGE_ZH ? g_resultMenuZh[scriptIndex] : g_resultMenuEn[scriptIndex];
}

const char *ResultCharacterName(int index, const char *original)
{
    if (GetLanguage() == IOS_LANGUAGE_JP || index < 0 || index >= 13) return original;
    return GetLanguage() == IOS_LANGUAGE_ZH ? g_characterZh[index] : g_characterEn[index];
}

const char *ReplayLabel(const char *key)
{
    const char *value = Lookup(g_replayLabels, sizeof(g_replayLabels) / sizeof(g_replayLabels[0]), key);
    return value != NULL ? value : key;
}

const char *ReplayCharacterName(int index, const char *original)
{
    return ResultCharacterName(index, original);
}

const char *ReplayDifficultyName(int index, const char *original)
{
    if (GetLanguage() == IOS_LANGUAGE_JP || index < 0 || index >= 5) return original;
    return GetLanguage() == IOS_LANGUAGE_ZH ? g_difficultyZh[index] : g_difficultyEn[index];
}

} // namespace ios
} // namespace modern
} // namespace th095
