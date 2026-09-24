/*
 * Several exact reconstruction units still materialize original 1.00d data
 * addresses.  Keep the target data arena present and bind the corresponding
 * reconstructed owners to their original virtual addresses.  The regular ELF
 * image remains at the toolchain's native base; only target-owned data lives
 * here.  NOLOAD prevents this zero-initialized arena from inflating the file.
 */
SECTIONS
{
    .TH095_target_data 0x004b4000 (NOLOAD) :
    {
        . = 0x018da000 - 0x004b4000;
    }

    _ZN4TH09516g_LastSpellCountE = 0x004c6c3c;
    _ZN4TH09517g_EffectTemplatesE = 0x004c6d30;
    _ZN4TH09521g_GuiStageScoreTablesE = 0x004c7158;
    _ZN4TH09522g_GuiMessageTextColorsE = 0x004c7180;
    _ZN4TH09523g_GuiStageMusicContextsE = 0x004c7240;
    _ZN4TH09530g_AsciiManagerDrawChainLowPrioE = 0x004cce00;
    _ZN4TH09514g_AsciiManagerE = 0x004cce20;
    _ZN4TH09534g_EclCallbackPublishedEnemyField56E = 0x004e3d24;
    _ZN4TH09534g_EclCallbackPublishedEnemyField24E = 0x004e3d28;
    _ZN4TH09523g_AsciiManagerCalcChainE = 0x004e3fd0;
    _ZN4TH09531g_AsciiManagerDrawChainHighPrioE = 0x004e3ff0;
    _ZN4TH09529g_BackgroundDrawChainHighPrioE = 0x004e4010;
    _ZN4TH09512g_BackgroundE = 0x004e4030;
    _ZN4TH09521g_GuiMessageStageModeE = 0x004e4b64;
    _ZN4TH09528g_BackgroundDrawChainLowPrioE = 0x004ea630;
    _ZN4TH09521g_BackgroundCalcChainE = 0x004ea650;

    /* ECL opcode 147 names Background::unk6260 as a standalone global. */
    _ZN4TH09519g_EclGlobal004EA290E = 0x004ea290;

    _ZN4TH09511g_SpellcardE = 0x004ea670;
    _ZN4TH09520g_SpellcardCalcChainE = 0x004eccac;

    /* ECL opcode 155 names Spellcard::scoreLimit as a standalone global. */
    _ZN4TH09519g_EclGlobal004ECCA8E = 0x004ecca8;

    _ZN4TH09512g_EclManagerE = 0x004eccb8;
    _ZN4TH09517EclRunLowProposal19g_EclCallParametersE = 0x004ece20;
    _ZN4TH09515g_EffectManagerE = 0x004ece60;

    /*
     * The original linker laid the two EffectManager ANM pointers at the end
     * of the aggregate directly before its chain elements.  Spellcard.cpp
     * names the second pointer as a separate global, while EffectManager.cpp
     * writes it through g_EffectManager + 0x8b058.  Keep those views aliased.
     */
    _ZN4TH09528g_AsciiManagerDemoAnm0577EB4E = 0x00577eb4;
    _ZN4TH09524g_SpellcardBackgroundAnmE = 0x00577eb8;
    _ZN4TH09524g_EffectManagerCalcChainE = 0x00577ebc;
    _ZN4TH09524g_EffectManagerDrawChainE = 0x00577edc;

    _ZN4TH09531g_EnemyManagerDrawChainHighPrioE = 0x00577f00;
    _ZN4TH09514g_EnemyManagerE = 0x00577f20;
    _ZN4TH09517EclRunLowProposal21g_EclEnemyTableF54CC0E = 0x00f54cc0;
    _ZN4TH09518g_EnemyDropCounterE = 0x00f54ce0;
    _ZN4TH09524g_EnemyDropScheduleIndexE = 0x00f54ce2;
    _ZN4TH09523g_EnemyManagerCalcChainE = 0x00f54e30;
    _ZN4TH09530g_EnemyManagerDrawChainLowPrioE = 0x00f54e50;
    _ZN4TH09524g_BulletManagerDrawChainE = 0x00f54e70;
    _ZN4TH09515g_BulletManagerE = 0x00f54e90;
    _ZN4TH09524g_BulletManagerCalcChainE = 0x0160f408;
    _ZN4TH0955g_GuiE = 0x0160f428;
    _ZN4TH09514g_GuiDrawChainE = 0x0160f4c4;
    _ZN4TH09514g_GuiCalcChainE = 0x0160f4e4;
    _ZN4TH09513g_GameManagerE = 0x0160f508;
    _ZN4TH09531g_EclScriptedGlobalUpdateFreezeE = 0x0160f534;
    _ZN4TH09523g_PlayerNormalBombCountE = 0x0164cfa8;
    _ZN4TH09522g_PlayerDeathbombCountE = 0x0164cfac;
    _ZN4TH09529g_MaxValuePointItemsCollectedE = 0x0164cf9c;
    _ZN4TH09519g_PlayerGaugeBoundsE = 0x0164d300;
    _ZN4TH09522g_GameManagerCalcChainE = 0x0164d344;
    _ZN4TH09522g_GameManagerDrawChainE = 0x0164d364;
    _ZN4TH09514g_JoystickCapsE = 0x0164d388;
    _ZN4TH09526g_FocusButtonConflictStateE = 0x0164d51c;
    _ZN4TH0955g_RngE = 0x0164d520;
    _ZN4TH09515g_CurFrameInputE = 0x0164d528;
    _ZN4TH09524g_GuiMessageInputCurrentE = 0x0164d52c;
    _ZN4TH09516g_LastFrameInputE = 0x0164d530;
    _ZN4TH09525g_GuiMessageInputPreviousE = 0x0164d534;
    _ZN4TH09526g_IsEighthFrameOfHeldInputE = 0x0164d538;
    _ZN4TH09527g_NumOfFramesInputsWereHeldE = 0x0164d53c;
    _ZN4TH09518g_GameErrorContextE = 0x0164d540;
    _ZN4TH0957g_ChainE = 0x0164f548;
    _ZN4TH09512g_PbgArchiveE = 0x0164f588;
    _ZN4TH09511g_ZunMemoryE = 0x0164f598;
    _ZN4TH09516g_ControllerDataE = 0x016535a0;
    _ZN4TH09519g_TextBufferSurfaceE = 0x01653620;
    _ZN4TH09513g_ItemManagerE = 0x01653648;
    _ZN4TH09516g_ExclusiveMutexE = 0x017ce6fc;
    _ZN4TH09512g_GameWindowE = 0x017ce700;
    _ZN4TH09534g_DummyMidiTimerPerformanceCounterE = 0x017ce744;
    _ZN4TH09524g_SupervisorScreenEffectE = 0x017ce750;
    _ZN4TH09512g_SupervisorE = 0x017ce758;
    _ZN4TH09524g_GuiFullPowerModeFramesE = 0x017ce8cc;
    _ZN4TH09532g_GuiMessageScreenEffectDurationE = 0x017ce8cc;

    /* These target globals are overlapping views into aggregate owners. */
    _ZN4TH09518g_EclGameTimeScaleE = 0x017ce8e0;
    _ZN4TH09523g_EclGameTimeScaleFlagsE = 0x017ce8fc;
    _ZN4TH0958g_PlayerE = 0x017d5ef8;
    _ZN4TH09511EclOperands30g_TargetPlayerPosition017D61ACE = 0x017d61ac;

    _ZN4TH09522g_SupervisorLoadingVmsE = 0x017ceac0;
    _ZN4TH09529g_SupervisorFpsQpcSampleCountE = 0x017cf2ac;
    _ZN4TH09522g_SupervisorFpsLastQpcE = 0x017cf2b0;
    _ZN4TH09526g_SupervisorFpsDebugBufferE = 0x017cf2b8;
    _ZN4TH09521g_SupervisorFpsBufferE = 0x017cf3b8;
    _ZN4TH09523g_SupervisorFpsLastTimeE = 0x017cf4b8;
    _ZN4TH09530g_SupervisorFpsTimeInitializedE = 0x017cf4bc;
    _ZN4TH09525g_SupervisorFpsFrameCountE = 0x017cf4c0;
    _ZN4TH09515g_ReplayManagerE = 0x018b8a28;
    _ZN4TH09521g_ScreenEffectCounterE = 0x018b8a2c;
    _ZN4TH09514g_ScreenEffectE = 0x018b8a30;
    _ZN4TH09513g_SoundPlayerE = 0x018b8a68;
    _ZN4TH09512g_AnmManagerE = 0x018bdc90;
    _ZN4TH09514g_QuadVerticesE = 0x018bdc98;
    _ZN4TH09534g_AnmManagerUntexturedQuadVerticesE = 0x018bdd08;
    _ZN4TH09524g_BackgroundQuadVerticesE = 0x018bdd68;
    _ZN4TH09513g_TitleScreenE = 0x018bde08;
    _ZN4TH0954Lzss6m_TreeE = 0x018bdf58;
    _ZN4TH0954Lzss6m_DictE = 0x018d5f68;
}
INSERT BEFORE .bss;
