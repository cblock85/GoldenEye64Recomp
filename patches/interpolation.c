#include "patches.h"

#if 0

extern Mtx roomMatrices[300];
s32 setupRoomTransformationMatrix(s32 room);

RECOMP_PATCH Gfx* applyRoomMatrixToDisplayList(Gfx* gdl, int roomID) {
    s32 roomIndex;

    roomIndex = setupRoomTransformationMatrix(roomID);
    gSPMatrix(gdl++, &roomMatrices[roomIndex], G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

    return gdl;
}
#endif