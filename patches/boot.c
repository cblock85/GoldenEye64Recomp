#include "patches.h"

int dummy;
int dummy3 = 1;

RECOMP_PATCH void init(void) {
    s32 inflate_code_size;
    s32* stack_pointer;

    // TODO: Validate if this is even needed.
#if 1
    recomp_load_overlays((u32) 0x00001050, (void*) 0x80000450, 0x20740); // our .code size

    // recomp_printf("recomp_load_overlays: %x, vAddr: %x, size: %x\n", (u32) 0x00101000, 0x80100400,
    //               inflate_code_size - (0x101000 - 0x1050));
    inflate_code_size = (s32) ((u32) 0x0005DCE0 - (u32) 0x00001050); // our _inflateSegmentRomStart
    if (inflate_code_size > (0x101000 - 0x1050)) {
        boot_osPiRawStartDma(OS_READ, (u32) 0x00101000, (void*) 0x80100400, inflate_code_size - (0x101000 - 0x1050));
    }
#endif

    osInitialize();

    /**
     * Needed for the runtime as the function cache is insuficient,
     * so we treat the whole section as an overlay
     */
    recomp_load_overlays((u32) 0x0005F280, (void*) 0x80400000, ((u32) 0xE2D50)); // our _gameSegmentRomStart

    boot_osPiRawStartDma(OS_READ, (u32) 0x0005F280, (void*) 0x80400000, ((u32) 0xE2D50));

    stack_pointer = setSPToEnd(sp_main, sizeof(sp_main));
    osCreateThread(&mainThread, (OSId) 3, &mainproc, NULL, stack_pointer, (OSPri) 10);
    osStartThread(&mainThread);

    // @recomp: ModelDistance always disabled
    g_ModelDistanceDisabled = 1;

    // memusage_display_flag = 1;
}

/**
 * Stubbing this since it causes the runtime to crash while trying to resolve rmonMain
 * It isn't needed for the game anyway
 */
#if 1
RECOMP_PATCH void rmonCreateThread(void) {
    return;
}
#endif
