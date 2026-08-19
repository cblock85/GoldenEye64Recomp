#include "patches.h"

int demoMode = 0;

void waitForNextFrame2();  // @theboy181 - Improtant for proper fps
#if 1
RECOMP_PATCH Gfx *renderGunbarrelEyeIntroSequence(Gfx *gdl) {
    D_8002A7D0 = (1 - D_8002A7D0);
    switch (gunbarrel_mode - 2)
    {
    case 0:
        gdl = manipulateGunbarrelAndLogoMatrices(gdl);
        g_TitleX += XINC;
        if (word_CODE_bss_80069584 < 0) {
            word_CODE_bss_80069584 = 200;
            titleTransitionX = (g_TitleX - XDEC);
        } else {
#if defined(VERSION_EU)
            word_CODE_bss_80069584 -= 7;
#else
            word_CODE_bss_80069584 -= 6;
#endif
        }
        if (g_TitleX > 1390.0f) {
            gunbarrel_mode++;
            g_TitleX = 1276.0f;
        }
        break;

    case 1:
#if defined(LEFTOVERDEBUG)
        gSPDisplayList(gdl++, &dlBasicGeometry);
        gdl = clear_framebuffer_black(gdl++);
        gdl = clear_framebuffer_black(gdl++);
        gdl = clear_framebuffer_black(gdl++);
        gdl = clear_framebuffer_black(gdl++);
        gdl = clear_framebuffer_black(gdl++);
#endif
        gdl = insert_sniper_sight_eye_intro(gdl++);
        gdl = insert_sight_backdrop_eye_intro(gdl++);
        
        if (g_TitleX < 600.0f) {
            gdl = insert_bond_eye_intro(gdl);
        }
        g_TitleX -= XDEC3;
        if (g_TitleX <= -80.0f) {
            gunbarrel_mode++;
            intro_eye_counter = 20;
        }
        break;

    case 2:
        gdl = insert_sniper_sight_eye_intro(gdl);
        gdl = insert_sight_backdrop_eye_intro(gdl);
        gdl = insert_bond_eye_intro(gdl);
        intro_eye_counter--;
        if (intro_eye_counter < 0) {
            gunbarrel_mode++;
            die_blood_image_routine(0);
            intro_state_blood_animation = 0;
            intro_eye_counter = 1;
        }
        break;

    case 3:
        intro_eye_counter--;
        if (intro_eye_counter == 0) {
            intro_state_blood_animation = die_blood_image_routine(1);
            intro_eye_counter = 2;
        }
        gdl = insert_sniper_sight_eye_intro(gdl);
        gdl = insert_sight_backdrop_eye_intro(gdl);
        gdl = insert_bond_eye_intro(gdl);
        gdl = gunbarrelBloodOverlayDL(gdl);
        if (intro_state_blood_animation != 0) {
            gunbarrel_mode++;
            word_CODE_bss_80069584 = 0;
            titleTransitionX = g_TitleX;
            intro_eye_counter = 0;
        }
        break;

    case 4:
        word_CODE_bss_80069584 += INCVAL;
        intro_eye_counter++;
        g_TitleX = ((sins(word_CODE_bss_80069584) * 64.0f) / 32768.0f) + titleTransitionX;
        gdl = insert_sniper_sight_eye_intro(gdl);
        gdl = insert_sight_backdrop_eye_intro(gdl);
        gdl = insert_bond_eye_intro(gdl);
        gdl = sub_GAME_7F01CA18(gdl);
        if (intro_eye_counter >= INTRO_EYE_COUNTER_CASE_4)
        {
            intro_eye_counter = 0;
            gunbarrel_mode++;
        }
        break;

    case 5:
        word_CODE_bss_80069584 += INCVAL;
        g_TitleX = ((sins(word_CODE_bss_80069584) * 64.0f) / 32768.0f) + titleTransitionX;
        gdl = insert_sniper_sight_eye_intro(gdl);
        gdl = insert_sight_backdrop_eye_intro(gdl);
        gdl = insert_bond_eye_intro(gdl);
        gdl = sub_GAME_7F01CA18(gdl);
        
        intro_eye_counter += INTRO_EYE_COUNTER_CASE_5_ADD;
        
        gdl = sub_GAME_7F007E70(gdl, intro_eye_counter);
        if (intro_eye_counter >= 0xF7) {
            intro_eye_counter = 0;
            gunbarrel_mode++;                
        }
        break;

    case 6:
        gSPDisplayList(gdl++, &dlBasicGeometry);
        gdl = clear_framebuffer_black(gdl);
        if (intro_eye_counter++ >= INTRO_EYE_COUNTER_CASE_6) {
            intro_eye_counter = 0;
            gunbarrel_mode++;
        }
        break;
    };

    if ((gunbarrel_mode - 2) != 0) { // @theboy181 - fps fix - Gunbarrel mode
        waitForNextFrame2();
    }

    return gdl;
}
#endif

#if 0
RECOMP_PATCH s16 bondviewGetCurrentPlayerViewportUly(void)
{
    f32 t;

    if (getPlayerCount() == 2)
    {
        if (get_cur_playernum() == 0)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_2P_PLAYER_1;
#endif
        }

        return VIEWPORT_ULY_2P_PLAYER_2;
    }

    if (getPlayerCount() >= 3)
    {
        if (get_cur_playernum() < 2)
        {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_4P_PLAYER_12;
#endif
        }

        return VIEWPORT_ULY_4P_PLAYER_34;
    }

    if (cameraBufferToggle != 0)
    {
        if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
        {
            return VIEWPORT_ULY_CAM_WIDESCREEN;
        }
        else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
        {
            return VIEWPORT_ULY_CAM_CINEMA;
        }
        else
        {
            return VIEWPORT_ULY_CAM_FULLSCREEN;
        }
    }

    if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
    {
        t = bondviewGetPauseAnimationPercent();
        return (s16) ((s32) (WIDESCREEN_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_WIDESCREEN_OFFSET);
    }
    else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
    {
        t = bondviewGetPauseAnimationPercent();
        return (s16) ((s32) (CINEMA_ULY_SCALE_FACTOR * t) + VIEWPORT_ULY_CINEMA_OFFSET);
    }
    else
    {
#ifdef VERSION_EU
            return 0;
#else
            return VIEWPORT_ULY_DEFAULT;
#endif
    }
}
#endif

#if 0
RECOMP_PATCH void bondviewMovePlayerUpdateViewport(s8 stick_x, s8 stick_y, u16 buttons)
{
#ifdef VERSION_EU
    f32 faspect;
#endif

    set_cur_player_fovy(FOV_Y_F);

    // This call doesn't do anything, the call viSetFovY(g_CurrentPlayer->fovy); in lvlRender
    // will actually change the field of view.
    // The call above should set g_CurrentPlayer->fovy, but it doesn't seem to affect
    // the fov....
    viSetFovY(FOV_Y_F);

    if (cameraFrameCounter1 != 0)
    {
        if ((cameraFrameCounter1 >= 4) && (resolution != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
        {
            cameraBufferToggle = 1;
            cameraFrameCounter1 = 0;
        }
        else
        {
            cameraFrameCounter1 += 1;
        }
    }
    else
    {
        if (cameraFrameCounter2 != 0)
        {
            if ((cameraFrameCounter2 >= 4) && (viGetFrameBuf2() == (u8*)(cfb_16[0])))
            {
                cameraBufferToggle = 0;
                cameraFrameCounter2 = 0;
            }
            else
            {
                cameraFrameCounter2 += 1;
            }
        }
    }

    if ((cameraBufferToggle != 0) && (viGetFrameBuf2() == (u8*)(cfb_16[1])))
    {
        viSetFrameBuf2((u8 *) resolution);
    }

#ifdef VERSION_EU
    if (get_screen_ratio() == SCREEN_RATIO_16_9)
    {
        faspect = ((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight()) * 0.75f * WIDESCREEN_ASPECT;
    }
    else
    {
        faspect = (f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight();
    }

    if (cameraBufferToggle == 0)
    {
        faspect *= EU_CAMERA_8003642C_ASPECT;
    }

    set_cur_player_aspect(faspect);
    viSetAspect(faspect);

#else

    if (get_screen_ratio() == SCREEN_RATIO_16_9)
    {
        set_cur_player_aspect(((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
        viSetAspect(((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight()) * 0.75f * WIDESCREEN_ASPECT);
    }
    else
    {
        set_cur_player_aspect((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight());
        viSetAspect((f32) bondviewGetCurrentPlayerViewportWidth() / (f32) bondviewGetCurrentPlayerViewportHeight());
    }
#endif

    set_cur_player_screen_size( bondviewGetCurrentPlayerViewportWidth(), bondviewGetCurrentPlayerViewportHeight());
    set_cur_player_viewport_size( get_curplayer_viewport_ulx(), bondviewGetCurrentPlayerViewportUly());
    viSetXY(getWidth320or440(), getHeight330or240());
    viSetBuf(getWidth320or440(), getHeight330or240());
    viSetViewSize(bondviewGetCurrentPlayerViewportWidth(), bondviewGetCurrentPlayerViewportHeight());
    viSetViewPosition(get_curplayer_viewport_ulx(), bondviewGetCurrentPlayerViewportUly());
    currentPlayerUpdateColourScreenProperties();
    currentPlayerTickChrFade();
    set_BONDdata_autoaim_y(cur_player_get_autoaim());
    set_BONDdata_autoaim_x(cur_player_get_autoaim());
    set_BONDdata_lookahead_setting(cur_player_get_lookahead());
    gunSetGunAmmoVisible(GUNAMMOREASON_OPTION, cur_player_get_ammo_onscreen_setting());

    gunSetSightVisible(
        GUNSIGHTREASON_1,
        (getPlayerCount() == 1 && cur_player_get_sight_onscreen_control())
            || (getPlayerCount() >= 2 && g_playerPerm->sight)
    );

#if defined(VERSION_EU)
    if (1);
#endif

    if ((g_CameraMode == CAMERAMODE_NONE) || ((g_CameraMode == CAMERAMODE_FP) && (is_timer_active != 0)) || (g_CameraMode == CAMERAMODE_FADE_TO_TITLE))
    {
        if (get_cur_playernum() == 0)
        {
            mission_timer += g_ClockTimer;
        }

        MoveBond(stick_x, stick_y, buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }
    else
    {
        bondviewFrozenMoveBond(stick_x, stick_y, buttons, (u16) g_CurrentPlayer->buttons_pressed);
    }

#if defined(BUGFIX_R1)
    watch_time_0 += jpD_800484D0;
#else
    // VERSION_US
    watch_time_0 += speedgraphframes;
#endif

    if (stop_time_flag != 0)
    {
        if ((lvlGetControlsLockedFlag() == 0) && ((buttons & ~(g_CurrentPlayer->buttons_pressed) & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON | CONT_R | CONT_L))))
        {
            stop_time_flag = 2;

            if (currentPlayerIsFadeComplete())
            {
                if (g_CurrentPlayer->colourscreenfrac == 0.0f)
                {
                    currentPlayerSetFadeColour(0, 0, 0, 0.0f);
                    currentPlayerSetFadeFrac(60.0f, 1.0f);
                }
            }
            else
            {
                if (g_CurrentPlayer->colourfadefracnew == 0.0f)
                {
                    currentPlayerSetFadeFrac(g_CurrentPlayer->colourfadetime60, 1.0f);
                }
            }
        }

        if ((stop_time_flag == 2) && currentPlayerIsFadeComplete() && (g_CurrentPlayer->colourscreenfrac == 1.0f))
        {
            bossReturnTitleStage();
        }
    }

    if (g_CameraAfterCinema)
    {
        sub_GAME_7F07B1A4();
    }

    if (g_CurrentPlayer->bonddead)
    {
        if (g_CurrentPlayer->redbloodfinished == FALSE)
        {
            currentPlayerEquipWeaponWrapper(GUNLEFT, 0);
            currentPlayerEquipWeaponWrapper(GUNRIGHT, 0);

            if (0)
            {
                // removed?
            };
        }

        if (g_CurrentPlayer->redbloodfinished && g_CurrentPlayer->deathanimfinished && (camera_mode >= CAMERAMODE_SWIRL))
        {
            bossRunTitleStage();
        }
    }

    if ((g_CameraMode == CAMERAMODE_FADE_TO_TITLE) && currentPlayerIsFadeComplete())
    {
        bossRunTitleStage();
    }

    g_CurrentPlayer->buttons_pressed = buttons;
}
#endif

#if 1

long strtol(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    long result = 0;
    int sign = 1;

    // Skip leading whitespace manually
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\f' || *s == '\r') {
        s++;
    }

    // Handle optional sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // Auto-detect base if needed
    if (base == 0) {
        if (*s == '0') {
            if (s[1] == 'x' || s[1] == 'X') {
                base = 16;
                s += 2;
            } else {
                base = 8;
                s++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    const char* start = s;

    while (*s) {
        int digit;

        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base)
            break;

        result = result * base + digit;
        s++;
    }

    if (endptr) {
        *endptr = (s == start) ? (char*) nptr : (char*) s;
    }

    return sign * result;
}

#define BASE_FPS 60
static int desiredFPS = 30;
static int frameSkipInterval;
static int frameSkipCounter = 0;

void InitFrameRateControl(void)
{
    if (desiredFPS <= 0 || desiredFPS > BASE_FPS) {
        desiredFPS = BASE_FPS;
    }
    frameSkipInterval = BASE_FPS / desiredFPS;
    if (frameSkipInterval < 1) {
        frameSkipInterval = 1;
    }
}

#define MAIN_LOOP_TICK_INTERVAL 0x5eb61U

extern s32 g_DebugMode;
extern s32 g_DebugHighlightedOption;

RECOMP_PATCH void bossMainloop(void) {
    // declarations

    s32 done;
    const unsigned char* tokenFindLevel;
    GFXMsg* localGfxFrameMsg; // sp 468
    OSScMsg localGfxDoneMsg;  // sp 436
    s32 stringIndex;
    s32 toggleFlag; // sp 428
    Gfx* gdl;       // sp424
    Gfx* firstGdl;
    u32 nowCount;
    s32 i;
    s32 count;
    s8 joyStickXPos;
    s8 joyStickYPos;
    u16 joyButtons;
    struct player* localPlayer;
    s32 localSelectedNumPlayers;
    u32 pendingGfx = 0;
    s32 freeGfx;
    s32 mainTickElapsed;
    s32 rspReplyMsg;

    u32 unused_stackpadding_[56];

    s32 multiplier = speedgraphframes;

    // end declarations


    done = 0;
    reset_mem_bank_5();

    if (tokenFind(1, "-level_") != NULL) {
        tokenFindLevel = (const unsigned char*) tokenFind(1, "-level_");

        // quick hack strltolon, converts the two digit ASCII level to 32bit int
        // eg, "09" = 48*10 + 57 - 528 = 9
        g_StageNum = (((s32) (tokenFindLevel[0] * 10) + (s32) tokenFindLevel[1]) - 0x210);
    }

    if (g_StageNum != LEVELID_TITLE) {
        fileValidateSaves();
        fileSetCurrentFolder(0);
        set_selected_difficulty(DIFFICULTY_AGENT);
        set_solo_and_ptr_briefing(g_StageNum);

        if (tokenFind(1, "-hard")) {
            // convert ASCII difficulty value to int in set difficulty calls eg '1' = 49, 49-48 = 1
            set_selected_difficulty(*(const unsigned char*) tokenFind(1, "-hard") - '0');
            lvlSetSelectedDifficulty(*(const unsigned char*) tokenFind(1, "-hard") - '0');
        }
    }

    nowCount = osGetCount_recomp();
    randomSetSeed(nowCount);

    InitFrameRateControl();

    // 'done' value never changes, and control never breaks -- infinite loop
    while (!done) {
        localGfxFrameMsg = NULL;
        localGfxDoneMsg = g_bossGfxDoneMsg;
        toggleFlag = 0;
        pendingGfx = 0;

        test_if_recording_demos_this_stage_load(g_StageNum, lvlGetSelectedDifficulty());
        if (g_DebugAndUpdateStageFlag) {
            stringIndex = -1;

            if (g_StageNum != LEVELID_TITLE && get_selected_num_players() >= 2) {
                stringIndex = 0;

                while (memallocstringtable[stringIndex].id) {
                    if (memallocstringtable[stringIndex].id == (g_StageNum + 400)) {
                        break;
                    }

                    stringIndex++;
                }

                if (memallocstringtable[stringIndex].id == 0) {
                    stringIndex = -1;
                }
            }

            if (stringIndex) {
                // empty
            }

            if (stringIndex < 0) {
                stringIndex = 0;

                while (memallocstringtable[stringIndex].id) {
                    if (memallocstringtable[stringIndex].id == g_StageNum) {
                        break;
                    }

                    stringIndex++;
                }
            }

            tokenSetString(memallocstringtable[stringIndex].string);
        }

        mempResetBank(MEMPOOL_STAGE);
        obBlankResourcesLoadedInBank(MEMPOOL_STAGE);
        if (tokenFind(1, "-ma")) {
            g_CurentMaMallocValue = (s32) (strtol(tokenFind(1, "-ma"), NULL, 0) * 1024);
        }

        memaReset(mempAllocBytesInBank(g_CurentMaMallocValue, MEMPOOL_STAGE), g_CurentMaMallocValue);
        reset_play_data_ptrs();

        localSelectedNumPlayers = 0;
        if (g_StageNum != LEVELID_TITLE) {
            localSelectedNumPlayers = 1;
            if (get_selected_num_players() >= 2) {
                localSelectedNumPlayers = get_selected_num_players();
            }
        }

        init_player_data_ptrs_construct_viewports(localSelectedNumPlayers);
        dynInitMemory();
        joyCheckStatusThreadSafe();
        lvlStageLoad(g_StageNum);
        viInitBuffers();
        debmenuInit();
        waitForNextFrame();
        speedgraphMarkerCommit();

        if (1)
            ; // regalloc
        if (1)
            ;

        while (osRecvMesg(&gfxFrameMsgQ, (OSMesg*) &localGfxFrameMsg, OS_MESG_NOBLOCK) == 0) {
            // empty
        }

        if (1)
            ; // regalloc
        if (1)
            ;

        while (g_MainStageNum < 0 || pendingGfx != 0) {
            osRecvMesg(&gfxFrameMsgQ, (OSMesg*) &localGfxFrameMsg, OS_MESG_BLOCK);

            switch (localGfxFrameMsg->gen.type) {
                case (OS_SC_RETRACE_MSG): {
                    { static int rc=0; if ((rc++ % 120)==0) recomp_printf("[trace] retrace#%d stage=%d pending=%d demo=%d sgf=%d\n", rc, g_MainStageNum, pendingGfx, demoMode, speedgraphframes); }
#ifdef DEBUG
                    /* debug logging from simple.c, I think this requires #include <ultralog.h>
                     * //    if (logging)
                     * //        osLogEvent(log, LOG_RETRACE, 1, pendingGfx);
                     */
#endif
                    mainTickElapsed = (u32) (osGetCount_recomp() - copy_of_osgetcount_value_1);
                    // @theboy181 - fps fix - demo mode
                    if ((demoMode == 1 && mainTickElapsed < (MAIN_LOOP_TICK_INTERVAL * (speedgraphframes * 2))) ||
                    (demoMode != 1 && mainTickElapsed < MAIN_LOOP_TICK_INTERVAL)) 
                    {
                        { static int sk=0; if ((sk++ % 120)==0) recomp_printf("[trace] skip#%d elapsed=%d interval=%d\n", sk, mainTickElapsed, MAIN_LOOP_TICK_INTERVAL); }
                        break; // Skip frame
                    } else {
                        if (g_MainStageNum < 0 && pendingGfx < 2U) {
                            if (get_is_ramrom_flag()) {
                                iterate_ramrom_entries_handle_camera_out();
                            } else {
                                waitForNextFrame();
                            }
                            
                            speedgraphRenderGraph();
                            speedgraphMarkerCommit();
                            speedgraphMarkerHandler(0x20000);
                            joyConsumeSamplesWrapper();
                            permit_stderr(0);
                            
                            recomp_printf("[trace] render start\n");
                            gdl = firstGdl = dynGetMasterDisplayList();

                            gEXEnable(gdl++); // @theboy181 - better place for this 
                            gEXSetRefreshRate(gdl++, 60 / speedgraphframes); // @theboy181

                            //recomp_printf("gunbarrel_mode = %d\n", speedgraphframes);


#if 1
                            // ported from pd beta, official way to open debug menu
                            //  If menu is open (?) or player has pressed C down + C up
                            if (g_BossIsDebugMenuOpen ||
                                joyGetButtons(0, U_CBUTTONS | D_CBUTTONS) == (U_CBUTTONS | D_CBUTTONS)) {
                                joyStickXPos = joyGetStickX(0);
                                joyStickYPos = joyGetStickY(0);
                                joyButtons = joyGetButtons(0, ANY_BUTTON);
                                g_BossIsDebugMenuOpen =
                                    debug_menu_processor(joyStickXPos, joyStickYPos, joyButtons,
                                                         joyGetButtonsPressedThisFrame(0, ANY_BUTTON));
                            } else if (joyGetButtons(0, START_BUTTON) == 0) {
                                g_DebugMode = g_DebugHighlightedOption;
                            } else
#endif
#ifndef DEBUGMENU
                                if (g_BossIsDebugMenuOpen)
#endif
                            {
                                joyStickXPos = joyGetStickX(0);
                                joyStickYPos = joyGetStickY(0);
                                joyButtons = joyGetButtons(0, ANY_BUTTON);
                                g_BossIsDebugMenuOpen =
                                    debug_menu_processor(joyStickXPos, joyStickYPos, joyButtons,
                                                         joyGetButtonsPressedThisFrame(0, ANY_BUTTON));
                            }

                            lvlManageMpGame();
                            shuffle_player_ids();

                            if (g_StageNum != LEVELID_TITLE) {
                                for (i = 0; i < getPlayerCount(); i++) {
                                    set_cur_player(get_nth_player_from_shuffled(i));

                                    localPlayer = g_CurrentPlayer;
                                    viSetViewSize(localPlayer->viewx, localPlayer->viewy);

                                    localPlayer = g_CurrentPlayer;
                                    viSetViewPosition(localPlayer->viewleft, localPlayer->viewtop);

                                    lvlViewMoveTick();
                                }
                            }

                            gdl = lvlRender(gdl);

                            // Lets Visualise the Coverage Value used for Scilohete Anti-Ailising (edges)
                            // (done on the VI), also produces a cool looking linemode - providing AA is working.
                            if (get_debug_VisCVG_flag()) {
                                gDPPipeSync(gdl++);                                 // 0xe7000000, 0x00000000
                                gDPSetCycleType(gdl++, G_CYC_1CYCLE);               // 0xba001402, 0x00000000
                                gDPSetBlendColor(gdl++, 0xff, 0xff, 0xff, 0xff);    // 0xf9000000, 0xffffffff
                                gDPSetPrimDepth(gdl++, 0xffff, 0xffff);             // 0xee000000, 0xffffffff
                                gDPSetDepthSource(gdl++, G_ZS_PRIM);                // 0xb9000201, 0x00000004
                                gDPSetRenderMode(gdl++, G_RM_VISCVG, G_RM_VISCVG2); // 0xb900031d, 0x0fa54040
                                gDPFillRectangle(gdl++, 0, 0, viGetX() - 1, viGetY() - 1);
                            }

                            gdl = debmenuDraw(gdl);

                            if (0 /* get_memusage_display_flag() */) { // @theboy181 - Enable/Disable onscreen HZ
                                gdl = speedgraphDisplayMetrics(gdl);
                            }

                            if (g_BossIsDebugMenuOpen) {
                                debugmenuUpdate();
                                gdl = debugmenuRender(gdl);
                            }

                            gDPFullSync(gdl++);       // 0xe9000000, 0x00000000
                            gSPEndDisplayList(gdl++); // 0xb8000000, 0x00000000

                            if (g_ShowMemUseFlag) {
                                nulled_mempLoopAllMemBanks();
                                memaDumpPrePostMerge();
                                dynRemovedFunc(gdl);
                                nullsub_41(0);
                                g_ShowMemUseFlag = 0;
                            }

                            if (g_ShowMemBarsFlag) {
                                dynDrawMembars(gdl);
                            }

                            freeGfx = dynGetFreeGfx2(gdl);
                            dynSwapBuffers();
                            video_related_8();

                            if ((get_debug_taskgrab_val()) &&
                                (joyGetButtonsPressedThisFrame(0, (A_BUTTON | B_BUTTON))) &&
                                (joyGetButtons(0, (A_BUTTON | B_BUTTON)) == (A_BUTTON | B_BUTTON))) {
                                static s32 taskgrab_ramdump_num = 1;
                                u8 taskGrabBuffer[28];
                                s32 taskGrabFileSize;

                                while (1) {
                                    s32 unusedSprintf =
                                        sprintf(taskGrabBuffer, "u64.taskgrab.%d.core", taskgrab_ramdump_num);

                                    if (indycommHostCheckFileExists(taskGrabBuffer, &taskGrabFileSize) != NULL) {
                                        taskgrab_ramdump_num++;
                                        continue;
                                    }

                                    break;
                                }

                                indycommHostSendDump(taskGrabBuffer, (u8*) 0x80000000, 0x400000);
                            }

                            rspReplyMsg = (s32) (&localGfxDoneMsg);
                            recomp_printf("[trace] submitting gfx task %p..%p\n", firstGdl, gdl);
                            rspGfxTaskStart(firstGdl, gdl, 0, (s32*) rspReplyMsg);

                            pendingGfx++;
                            memaSingleDefragPass();
#ifdef VERSION_EU
                            eu_sub_7f0c00a4();
#endif
                            toggleFlag ^= 1;

                            speedgraphMarkerHandler(0x10000);
                            if (1)
                                ;
                        }
                    }
                } break;

                case (OS_SC_DONE_MSG):
                    pendingGfx--;
                    break;

                case OS_SC_PRE_NMI_MSG:
                    pendingGfx = 4U;
                    break;
            }
        }

        lvlUnloadStageTextData();
        stop_demo_playback();
        mempNullNextEntryInBank(MEMPOOL_STAGE);
        obBlankResourcesLoadedInBank(MEMPOOL_STAGE);

#if defined(VERSION_EU)
        if (1)
            ;
#endif

        g_StageNum = g_MainStageNum;
        g_MainStageNum = LEVELID_NONE;
    }

    // if(gdl) also works here.
    if (!gdl)
        // removed ... or ido?
        ;

    sub_GAME_7F0D1A7C();


}
#endif

#if 1
RECOMP_PATCH void interface_menu00_legalscreen(void)
{
#ifdef REFRESH_PAL
#define MENU_LEGALSCREEN_MENU_TIMER_MAX (50 * 4 + 1)
#else
#define MENU_LEGALSCREEN_MENU_TIMER_MAX (60 * 4 + 1)
#endif

    viSetFovY(FOV_Y_F);
    viSetAspect(ASPECT_RATIO_SD);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);
#if defined(ISGOLDFINGER) || defined(USEEXPANSION)
    if (osGetMemSize != 0x00800000)
    {
        frontChangeMenu(MENU_NO_CONTROLLERS, TRUE); //double up as No expansion pak window
        return;
    }
#endif
    g_MenuTimer += g_ClockTimer;
    demoMode = 0;

    if (g_MenuTimer >= MENU_LEGALSCREEN_MENU_TIMER_MAX)
    {
        if ((joyGetControllerCount() < 1) && (is_first_time_on_legal_screen))
        {
            frontChangeMenu(MENU_NO_CONTROLLERS, TRUE);
        }
        else
        {
            frontChangeMenu(MENU_NINTENDO_LOGO, TRUE);
        }
    }
    else if (joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON) && (!is_first_time_on_legal_screen))
    {
        if (!is_first_time_on_main_menu)
        {
            frontChangeMenu(MENU_FILE_SELECT, TRUE);
        }
        else
        {
            frontChangeMenu(MENU_NINTENDO_LOGO, TRUE);
        }
    }
    

}
#endif

#if 0
RECOMP_PATCH void interface_menu05_fileselect(void)
{
    s32 i1;
    s32 i2;
    Mtxf spC8;
    Mtxf sp88;
    struct coord3d *sp54;

    demoMode = 0;

    if (((((joyGetButtonsPressedThisFrame(PLAYER_1, ANY_BUTTON) != 0) || (joyGetStickX(0) < -5)) || (joyGetStickX(0) >= 6)) || (joyGetStickY(0) < -5)) || (joyGetStickY(0) >= 6))
    {
        g_MenuTimer = 0;
    }
    else
    {
        g_MenuTimer += g_ClockTimer;
    }

    viSetFovY(60.0f);
    viSetAspect(1.3333334f);
    viSetZRange(100.0f, 10000.0f);
    viSetUseZBuf(0);

    for (i1 = 0; i1 < 4; i1++)
    {
        sp54 = &D_8002AB94[i1];

        matrix_4x4_7F059694(&spC8, 0.0f, 0.0f, 4000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        matrix_4x4_set_identity_and_position(sp54, &sp88);
        matrix_scalar_multiply(0.37f, (f32*)&sp88);
        matrix_4x4_multiply_in_place(&spC8, &sp88);

        walletinst[i1]->render_pos = dynAllocate(walletinst[i1]->obj->numMatrices << 6);
        matrix_4x4_copy(&sp88, &walletinst[i1]->render_pos->pos);

        dword_CODE_bss_80069620[i1].f[0] = walletinst[i1]->render_pos->pos.m[3][0];
        dword_CODE_bss_80069620[i1].f[1] = walletinst[i1]->render_pos->pos.m[3][1];
        dword_CODE_bss_80069620[i1].f[2] = walletinst[i1]->render_pos->pos.m[3][2];

        disable_all_switches(walletinst[i1]);

        select_load_bond_picture(walletinst[i1], fileGetBondForFolder(i1));
        set_item_visibility_in_objinstance(walletinst[i1], 0xE, 1);
        set_item_visibility_in_objinstance(walletinst[i1], 0xD, 1);
    }

    if (selected_folder_num >= 0)
    {
        if (selected_folder_num == 0x64)
        {
            selected_folder_num = selected_folder_num_copy;
        }

        if ((selected_folder_num < 0) || (selected_folder_num >= 4))
        {
            selected_folder_num = 0;
        }

        toggle_deletion_menu_for_folder(selected_folder_num);
        selected_folder_num = -1;
    }

    if (folder_selected_for_deletion >= 0)
    {
        if ((joyGetButtonsPressedThisFrame(0, 0x222U) != 0) && (folder_selected_for_deletion_choice == 0))
        {
            folder_selected_for_deletion_choice = 1;
            sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        else if ((joyGetButtonsPressedThisFrame(0, 0x111U) != 0) && (folder_selected_for_deletion_choice != 0))
        {
            folder_selected_for_deletion_choice = 0;
            sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        if ((joyGetStickX(0) < -0x2D) && (folder_selected_for_deletion_choice == 0))
        {
            folder_selected_for_deletion_choice = 1;
            sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        else if ((joyGetStickX(0) >= 0x2E) && (folder_selected_for_deletion_choice != 0))
        {
            folder_selected_for_deletion_choice = 0;
            sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
        }
        if (joyGetButtonsPressedThisFrame(0, A_BUTTON | Z_TRIG | START_BUTTON) != 0)
        {
            if (folder_selected_for_deletion_choice == 0)
            {
                fileDeleteSaveForFolder(folder_selected_for_deletion);
                sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
            }
            else
            {
                sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
            }
            toggle_deletion_menu_for_folder(folder_selected_for_deletion);
            folder_selected_for_deletion = -1;
        }
        else if (joyGetButtonsPressedThisFrame(0, B_BUTTON) != 0)
        {
            toggle_deletion_menu_for_folder(folder_selected_for_deletion);
            folder_selected_for_deletion = -1;
            sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
        }
    }
    else
    {
        for (i2 = 0; i2 < 4; i2++)
        {
            f32 sp80;
            f32 sp7C;
            f32 sp78;
            f32 sp74;

            struct rectbbox sp64;

            LEVEL_SOLO_SEQUENCE sp60;
            DIFFICULTY sp5C;

            sub_GAME_7F03F90C(walletinst[i2], &sp80, &sp7C, &sp78, &sp74);
            sub_GAME_7F03F948(&dword_CODE_bss_80069620[i2], &sp7C, &sp74, &sp64.right, &sp64.left);

            if ((sp64.right <= cursor_h_pos)
                && (cursor_h_pos <= sp64.left)
                && (sp64.down <= cursor_v_pos)
                && (cursor_v_pos <= sp64.up))
            {
                fileGetHighestStageDifficultyCompletedForFolder(i2, &sp60, &sp5C);

                if (joyGetButtonsPressedThisFrame(0, A_BUTTON | Z_TRIG | START_BUTTON) != 0)
                {
                    if (folder_selection_screen_option_icon == 0)
                    {
                        selected_folder_num = i2;
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, PAPER_TURN_SFX, NULL);
                    }
                    else if (folder_selection_screen_option_icon == 1)
                    {
                        fileCopyFolderToFirstFree(i2);
                        folder_selection_screen_option_icon = 0;
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, COPY_FILE_SFX, NULL);
                    }
                    else if (folder_selection_screen_option_icon == 2)
                    {
                        if ((sp60 >= SP_LEVEL_DAM) && (sp5C >= DIFFICULTY_AGENT))
                        {
                            folder_selected_for_deletion = i2;
                            folder_selected_for_deletion_choice = 1;
                        }

                        folder_selection_screen_option_icon = 0;
                        sndPlaySfx((struct ALBankAlt_s *) g_musicSfxBufferPtr, OPTION_CLICK2_SFX, NULL);
                    }

                    break;
                }
            }
        }

        if (joyGetButtonsPressedThisFrame(0, A_BUTTON | Z_TRIG | START_BUTTON) != 0)
        {
            if ((folder_option_COPY_bound.left <= cursor_h_pos)
                && (cursor_h_pos <= folder_option_COPY_bound.right)
                && (folder_option_COPY_bound.up <= cursor_v_pos)
                && (cursor_v_pos <= folder_option_COPY_bound.down))
            {
                folder_selection_screen_option_icon = 1;
                sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, DOOR_LOCK_SFX, NULL);
            }
            else if ((folder_option_ERASE_bound.left <= cursor_h_pos)
                && (cursor_h_pos <= folder_option_ERASE_bound.right)
                && (folder_option_ERASE_bound.up <= cursor_v_pos)
                && (cursor_v_pos <= folder_option_ERASE_bound.down))
            {
                folder_selection_screen_option_icon = 2;
                sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, DOOR_LOCK_SFX, NULL);
            }
            else if (folder_selection_screen_option_icon != 0)
            {
                folder_selection_screen_option_icon = 0;
                sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
            }
        }
        else if ((joyGetButtonsPressedThisFrame(0, B_BUTTON) != 0)
            && (folder_selection_screen_option_icon != 0))
        {
            folder_selection_screen_option_icon = 0;
            sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, GUN_M60AMMGUN_3_SFX, NULL);
        }

        frontUpdateControlStickPosition();
    }

    if (selected_folder_num >= 0)
    {
        frontChangeMenu(MENU_MODE_SELECT, FALSE);
        setCursorPOSforMode(0);

        return;
    }

#if defined(VERSION_EU)
    if (g_MenuTimer >= 1501) // PAL (50fps): 30 seconds + 1 frame
#else
    if (g_MenuTimer >= 1801) // NTSC (60fps): 30 seconds + 1 frame
#endif
    {
        frontChangeMenu(MENU_LEGAL_SCREEN, TRUE);
    }
    
}
#endif

#if 0
RECOMP_PATCH void select_ramrom_to_play(void)
{
    s32 i;
    s32 temp_v0;

    temp_v0 = fileGetHighestStageUnlockedAnyFolder();

    for (i = 0; ramrom_table[i].fdata != NULL && temp_v0 >= ramrom_table[i].locked; i++)
    {}

    replay_recorded_ramrom_at_address(ramrom_table[randomGetNext() % i].fdata);
}
#endif

#if 0
RECOMP_PATCH void menu_init(void) {
    s32 var_v0;

    if (current_menu == MENU_SWITCH_SCREENS) {
        if (spectrum_related_flag != 0) {
            if (viGetFrameBuf2() == (cfb_16[1])) {
                screen_size = SCREEN_SIZE_440x330;
                spectrum_related_flag = 0;
            }
        } else if ((is_emulating_spectrum != 0) && (viGetFrameBuf2() == cfb_16[0])) {
            screen_size = SCREEN_SIZE_320x240;
            is_emulating_spectrum = 0;
        }
    }

#if defined(VERSION_EU)
#define MENU_INIT_ASPECT_440 ASPECT_RATIO_EU_400_330
#define MENU_INIT_ASPECT_320 ASPECT_RATIO_EU_320_272
#else
#define MENU_INIT_ASPECT_440 ASPECT_RATIO
#define MENU_INIT_ASPECT_320 ASPECT_RATIO
#endif

    if (screen_size != SCREEN_SIZE_320x240) {
        if (viGetFrameBuf2() == (cfb_16[1])) {
            viSetFrameBuf2((u8*) ptr_menu_videobuffer);
        }

        viSetAspect(MENU_INIT_ASPECT_440);
        viSetXY(440, 330);
        viSetBuf(440, 330);
        set_cur_player_screen_size(440, 330);
        viSetViewSize(440, 330);
        set_cur_player_viewport_size(0, 0);
        viSetViewPosition(0, 0);
    } else {
        viSetAspect(MENU_INIT_ASPECT_320);
        viSetXY(320, SCREEN_HEIGHT);
        viSetBuf(320, SCREEN_HEIGHT);
        set_cur_player_screen_size(320, SCREEN_HEIGHT);
        viSetViewSize(320, SCREEN_HEIGHT);
        set_cur_player_viewport_size(0, 0);
        viSetViewPosition(0, 0);
    }

#undef MENU_INIT_ASPECT_440
#undef MENU_INIT_ASPECT_320

    if (((menu_update > MENU_INVALID) || (maybe_prev_menu > MENU_INVALID)) && (current_menu != MENU_SWITCH_SCREENS)) {
        switch (current_menu) {
            case MENU_LEGAL_SCREEN:
                update_menu00_legalscreen();
                break;
            case MENU_NINTENDO_LOGO:
                update_menu01_nintendo();
                break;
            case MENU_RAREWARE_LOGO:
                update_menu02_rareware();
                break;
            case MENU_EYE_INTRO:
                update_menu_03_eye();
                break;
            case MENU_GOLDENEYE_LOGO:
                update_menu04_goldeneye();
                break;
            case MENU_FILE_SELECT:
                update_menu05_filesel();
                break;
            case MENU_MODE_SELECT:
                update_menu06_modesel();
                break;
            case MENU_MISSION_SELECT:
                update_menu07_missionsel();
                break;
            case MENU_DIFFICULTY:
                update_menu08_difficulty();
                break;
            case MENU_007_OPTIONS:
                update_menu09_007options();
                break;
            case MENU_BRIEFING:
                update_menu0A_briefing();
                break;
            case MENU_MISSION_FAILED:
                update_menu0C_missionfailed();
                break;
            case MENU_MISSION_COMPLETE:
                update_menu0D_missioncomplete();
                break;
            case MENU_MP_OPTIONS:
                update_menu0E_mpoptions();
                break;
            case MENU_MP_SCENARIO_SELECT:
                update_menu13_mpscenario();
                break;
            case MENU_MP_CHAR_SELECT:
                update_menu0F_mpcharsel();
                break;
            case MENU_MP_TEAMS:
                update_menu14_mpteams();
                break;
            case MENU_MP_HANDICAP:
                update_menu10_mphandicap();
                break;
            case MENU_MP_CONTROL_STYLE:
                update_menu11_mpcontrols();
                break;
            case MENU_MP_STAGE_SELECT:
                update_menu12_mpstage();
                break;
            case MENU_CHEAT:
                update_menu15_cheat();
                break;
            case MENU_NO_CONTROLLERS:
                update_menu16_nocontrollers();
                break;
            case MENU_DISPLAY_CAST:
                update_menu18_displaycast();
                break;
            case MENU_SPECTRUM_EMU:
                update_menu19_spectrum();
                break;
        }

        if (menu_update > MENU_INVALID) {
            current_menu = MENU_SWITCH_SCREENS;
            reset_menutimer();
        }
    }

    if (maybe_prev_menu > MENU_INVALID) {
        current_menu = maybe_prev_menu;
        maybe_prev_menu = MENU_INVALID;

        switch (current_menu) {
            case MENU_LEGAL_SCREEN:
                init_menu00_legalscreen();
                break;
            case MENU_NINTENDO_LOGO:
                init_menu01_nintendo();
                break;
            case MENU_RAREWARE_LOGO:
                init_menu02_rarelogo();
                break;
            case MENU_EYE_INTRO:
                init_menu03_eyeintro();
                break;
            case MENU_GOLDENEYE_LOGO:
                init_menu04_goldeneyelogo();
                break;
            case MENU_FILE_SELECT:
                init_menu05_fileselect();
                break;
            case MENU_MODE_SELECT:
                init_menu06_modeselect();
                break;
            case MENU_MISSION_SELECT:
                init_menu07_missionselect();
                break;
            case MENU_DIFFICULTY:
                init_menu08_difficultyselect();
                break;
            case MENU_007_OPTIONS:
                init_menu09_007difficultyselect();
                break;
            case MENU_BRIEFING:
                init_menu0A_briefing();
                break;
            case MENU_RUN_STAGE:
                init_menu0B_runstage();
                break;
            case MENU_MISSION_FAILED:
                init_menu0C_missionfailed();
                break;
            case MENU_MISSION_COMPLETE:
                init_menu0D_missioncomplete();
                break;
            case MENU_MP_OPTIONS:
                init_menu0E_mpoptions();
                break;
            case MENU_MP_SCENARIO_SELECT:
                init_menu13_mpscenariosel();
                break;
            case MENU_MP_CHAR_SELECT:
                init_menu0f_mpcharsel();
                break;
            case MENU_MP_TEAMS:
                init_menu14_mpteamsel();
                break;
            case MENU_MP_HANDICAP:
                init_menu10_mphandicap();
                break;
            case MENU_MP_CONTROL_STYLE:
                init_menu11_mpcontrol();
                break;
            case MENU_MP_STAGE_SELECT:
                init_menu12_mpstage();
                break;
            case MENU_CHEAT:
                init_menu15_cheat();
                break;
            case MENU_NO_CONTROLLERS:
                init_menu16_nocontroller();
                break;
            case MENU_DISPLAY_CAST:
                init_menu18_displaycast();
                break;
            case MENU_SPECTRUM_EMU:
                init_menu19_spectrum();
                break;
        }
    }

    switch (current_menu) {
        case MENU_LEGAL_SCREEN:
            interface_menu00_legalscreen();
            break;
        case MENU_SWITCH_SCREENS:
            interface_menu17_switchscreens();
            break;
        case MENU_NINTENDO_LOGO:
            interface_menu01_nintendo();
            break;
        case MENU_RAREWARE_LOGO:
            interface_menu02_rareware();
            break;
        case MENU_EYE_INTRO:
            interface_menu03_eye();
            break;
        case MENU_GOLDENEYE_LOGO:
            interface_menu04_goldeneyelogo();
            break;
        case MENU_FILE_SELECT:
            interface_menu05_fileselect();
            break;
        case MENU_MODE_SELECT:
            interface_menu06_modesel();
            break;
        case MENU_MISSION_SELECT:
            interface_menu07_missionsel();
            break;
        case MENU_DIFFICULTY:
            interface_menu08_difficulty();
            break;
        case MENU_007_OPTIONS:
            interface_menu09_007options();
            break;
        case MENU_BRIEFING:
            interface_menu0A_briefing();
            break;
        case MENU_MISSION_FAILED:
            interface_menu0C_missionfailed();
            break;
        case MENU_MISSION_COMPLETE:
            interface_menu0D_missioncomplete();
            break;
        case MENU_MP_OPTIONS:
            interface_menu0E_mpoptions();
            break;
        case MENU_MP_SCENARIO_SELECT:
            interface_menu13_mpscenario();
            break;
        case MENU_MP_CHAR_SELECT:
            interface_menu0F_mpcharsel();
            break;
        case MENU_MP_TEAMS:
            interface_menu14_mpteams();
            break;
        case MENU_MP_HANDICAP:
            interface_menu10_mphandicap();
            break;
        case MENU_MP_CONTROL_STYLE:
            interface_menu11_mpcontrols();
            break;
        case MENU_MP_STAGE_SELECT:
            interface_menu12_mpstage();
            break;
        case MENU_CHEAT:
            interface_menu15_cheat();
            break;
        case MENU_NO_CONTROLLERS:
            interface_menu16_nocontrollers();
            break;
        case MENU_DISPLAY_CAST:
            interface_menu18_displaycast();
            break;
        case MENU_SPECTRUM_EMU:
            interface_menu19_spectrum();
            break;
        case MENU_RUN_STAGE:
            if (interface_menu0B_runstage()) {
                frontChangeMenu(MENU_LEGAL_SCREEN, 1);
            } else if (gamemode == GAMEMODE_MULTI) {
                frontChangeMenu(MENU_MP_OPTIONS, 1);
            } else if (selected_stage == LEVELID_CUBA) {
                do_extended_cast_display(1);
                frontChangeMenu(MENU_DISPLAY_CAST, 1);
            } else {
                frontChangeMenu(MENU_MISSION_FAILED, 1);
            }
    }
}
#endif

#if 0
RECOMP_PATCH Gfx *speedgraphDisplayMetrics(Gfx *gdl)
{
#if defined(LEFTOVERDEBUG)
    u32 *counters;
    u32 localCountAccumulator = g_speedGraphCountAccumulator;
    s32 *pmaxSeenCount = &g_speedGraphMaxSeenCount;
    char buffer[12];
    volatile u32 *pcountAccumulator = &g_speedGraphCountAccumulator;
    
    localCountAccumulator += speedgraphframes;

    if (*pmaxSeenCount < speedgraphframes)
    {
        *pmaxSeenCount = speedgraphframes;
    }

    *pcountAccumulator = localCountAccumulator;
    
    if (localCountAccumulator > COUNT_REQUIRED_FOR_OUTPUT)
    {
        *pcountAccumulator = localCountAccumulator;

        if (localCountAccumulator > COUNT_REQUIRED_FOR_OUTPUT)
        {
            do
            {
                localCountAccumulator -= COUNT_REQUIRED_FOR_OUTPUT;
            }
            while (localCountAccumulator > COUNT_REQUIRED_FOR_OUTPUT);

            *pcountAccumulator = localCountAccumulator;
        }
        
        counters = get_counters();

        debmenuSetFgColour(255, 255, 255, 255);
        debmenuSetEnvColor(0, 0, 0, 255);

        // utz %
        debmenuSetPos(8, 5);
        sprintf_recomp(buffer, "X %f\n", (float)joyGetStickX(get_cur_playernum()));
        debmenuPrintString(buffer);

        // rsp %
        debmenuSetPos(8, 6);
        sprintf_recomp(buffer, "Y %f\n", (float)joyGetStickY(get_cur_playernum()));
        debmenuPrintString(buffer);

        // tex %
        debmenuSetPos(8, 7);
        sprintf_recomp(buffer, "tex %2.0f%%", ((counters[3] * 100.0f) / counters[0]));
        debmenuPrintString(buffer);

        // hz (60 / framerate)
        // -- or 50 for PAL
        debmenuSetPos(28, 5);
        sprintf_recomp(buffer, "%2d hz", ((speedgraphframes == 0) ? 0 : (60 / speedgraphframes)));
        debmenuPrintString(buffer);

        // framerate
        debmenuSetPos(28, 6);
        sprintf_recomp(buffer, "%2d frames", speedgraphframes);
        debmenuPrintString(buffer);

        // (continues framerate output)
        if (speedgraphframes != g_speedGraphMaxSeenCount)
        {
            sprintf_recomp(buffer, " [%2d]", *pmaxSeenCount);
        }
        else
        {
            sprintf_recomp(buffer, "     ");
        }

        debmenuPrintString(buffer);
        
        g_speedGraphMaxSeenCount = 0;
    }

    gSPDisplayList(gdl++, gSpeedGraphDisplayLists[gSpeedGraphDisplayListIndex ^ 1]);
#endif
    return gdl;
}
#endif

#if 1
RECOMP_PATCH void select_ramrom_to_play(void)
{
    s32 i;
    s32 temp_v0;
    demoMode = 1;

    temp_v0 = fileGetHighestStageUnlockedAnyFolder();

    for (i = 0; ramrom_table[i].fdata != NULL && temp_v0 >= ramrom_table[i].locked; i++)
    {}

    replay_recorded_ramrom_at_address(ramrom_table[randomGetNext() % i].fdata);
}
#endif
/* ==== recomp debug: scheduler message trace ==== */
extern void __scHandleRetrace(void *sc);
extern void __scHandleRSP(void *sc);
extern void __scHandleRDP(void *sc);

RECOMP_PATCH void __scMain(void *arg) {
    OSMesg msg = NULL;
    u8 *sc = (u8 *) arg;
    s32 done = 0;

    while (!done) {
        osRecvMesg((OSMesgQueue *) (sc + 0x40), &msg, OS_MESG_BLOCK);
        {
            static int n = 0;
            u32 rsp = *(u32 *) (sc + 0xC8);
            u32 rdp = *(u32 *) (sc + 0xCC);
            u32 gfxhead = *(u32 *) (sc + 0xBC);
            n++;
            if ((s32) msg != 666 || (n % 240) == 0) {
                recomp_printf("[sc] msg=%d rsp=%08X rdp=%08X gfxq=%08X\n", (s32) msg, rsp, rdp, gfxhead);
            }
        }
        switch ((s32) msg) {
            case 666: __scHandleRetrace(arg); break;
            case 667: __scHandleRSP(arg); break;
            case 668: __scHandleRDP(arg); break;
            case 669: done = 1; break; /* PRE_NMI */
            default: break;
        }
    }
}
