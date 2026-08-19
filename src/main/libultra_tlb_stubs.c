// No-op implementations for TLB-related libultra functions that N64Recomp's
// built-in ignore list drops but GoldenEye's (dead, TLBFREE-bypassed) TLB
// machinery still references. librecomp provides no shims for these since
// no TLB exists in the recompiled environment.
#include <stdint.h>
typedef struct recomp_context recomp_context;
void osUnmapTLB_recomp(uint8_t* rdram, recomp_context* ctx) { (void)rdram; (void)ctx; }
void __osGetTLBHi_recomp(uint8_t* rdram, recomp_context* ctx) { (void)rdram; (void)ctx; }
