#include "patches.h"

#define MAIN_LOOP_TICK_INTERVAL 0x5eb61U

#if 0
RECOMP_PATCH s32 get_debug_testingmanpos_flag(void) {
    return 1;
}

RECOMP_PATCH s8* bgDebPrintROOMID(s32 roomId) {
    s8* buffer = "01234567890123456789";

    sprintf_recomp(buffer, "printVar %d", printVar); // format and insert the value

    return buffer;
}
#endif

#if 0
RECOMP_PATCH void waitForNextFrame(void) __attribute__((optnone)) // maybe WaitForTick
{
    u32 nextFrameTime; // next frame time?

    do {
        nextFrameTime = ((osGetCount_recomp() - copy_of_osgetcount_value_1) + MAIN_LOOP_TICK_INTERVAL) /
                        ((MAIN_LOOP_TICK_INTERVAL * 2) + 1); // current time + 1/5
    } while (nextFrameTime < frameDelay);

    frameDelay = 1;
    updateFrameCounters(nextFrameTime);
}
#endif

#if 1
void waitForNextFrame2(void) // @theboy181 - Improtant for proper fps
{
    u32 nextFrameTime;

    do {
        nextFrameTime = ((osGetCount_recomp() - copy_of_osgetcount_value_1) + MAIN_LOOP_TICK_INTERVAL) /
                        ((MAIN_LOOP_TICK_INTERVAL * 2) + 1);
    } while (nextFrameTime < frameDelay);

    frameDelay = speedgraphframes;
    updateFrameCounters(nextFrameTime);
}
#endif
