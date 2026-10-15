#include "patches.h"
#include "gbi_extension.h"

#if 1
RECOMP_PATCH Gfx* skyRenderTri(Gfx* gdl, SkyRelated38* v1, SkyRelated38* v2, SkyRelated38* v3, f32 scale,
                                    bool textured) {
    u32 width = viGetX();
    u32 height = viGetY();

    // Fetch the current environment (this is where fog and sky color are stored)
    struct CurrentEnvironmentRecord* env = fogGetCurrentEnvironmentp();

    // Get the fog color components (assumes fog color is stored in RGBA format)
    u8 red = env->Red;
    u8 green = env->Green;
    u8 blue = env->Blue;

    // Set the fog color as the fill color for the skybox
    u32 fill_color = GPACK_RGBA5551(red, green, blue, 1); // Fog color instead of hardcoded light blue

    // Fill the skybox with the fog color
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, osViGetCurrentFramebuffer());
    gDPSetFillColor(gdl++, (fill_color << 16) | fill_color);
    gDPFillRectangle(gdl++, 0, 0, (width - 1), (height - 1));
    gDPPipeSync(gdl++);

    return gdl;
}

RECOMP_PATCH Gfx* skyRenderFull(Gfx* gdl, SkyRelated38* arg1, SkyRelated38* arg2, SkyRelated38* arg3,
                                    SkyRelated38* arg4, f32 arg5) {
    u32 width = viGetX();
    u32 height = viGetY();

    // Fetch the current environment to get fog color
    struct CurrentEnvironmentRecord* env = fogGetCurrentEnvironmentp();

    // Get the fog color components
    u8 red = env->Red;
    u8 green = env->Green;
    u8 blue = env->Blue;

    // Set the fog color as the fill color for the skybox
    u32 fill_color = GPACK_RGBA5551(red, green, blue, 1); // Use fog color for the background

    // Fill the skybox with the fog color
    gDPPipeSync(gdl++);
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, osViGetCurrentFramebuffer());
    gDPSetFillColor(gdl++, (fill_color << 16) | fill_color);
    gDPFillRectangle(gdl++, 0, 0, (width - 1), (height - 1));
    gDPPipeSync(gdl++);

    return gdl;
}

#endif

#define PORTSKY 1

/*
 * Sky and water for RT64.
 *
 * On console skyRender draws the sky and water as raw RDP triangles (skyRenderTri/skyRenderFull),
 * which RT64 doesn't implement. With PORTSKY they are drawn as regular vertex triangles instead, the
 * same approach as the Perfect Dark PC port. Differences from a normal draw:
 *  - The polygon is drawn camera-relative with a rotation-only modelview, so the camera position never
 *    goes through the s15.16 matrix translation.
 *  - Vtx positions are s16, so they are scaled per polygon to fit and the scale is undone in the matrix.
 *  - The raw triangles carried their own shading and texturing; here that state is set explicitly and
 *    restored afterwards, so nothing leaks into the level rendering that follows.
 *  - The game only builds the polygon for its native 4:3 view; skyRender widens the view it builds for
 *    to cover the widescreen area (skyWidenView).
 */
#define SKY_VTX_RANGE 32000.0f
#define SKY_TC_PERIOD (256.0f * 32.0f)
// Water texture coordinates are stored divided by this and restored with tile shift 12 (x16).
#define SKY_WATER_TC_DIV 16.0f
#define SKY_WATER_TILE_SHIFT 12

static f32 skyAbs(f32 v) {
    return v < 0.0f ? -v : v;
}

enum SkyKind { SKY_CLOUDS, SKY_WATER, SKY_GROUND };

static Gfx* skyDrawPolygon(Gfx* gdl, SkyRelated18* src, s32 count, f32 scale, enum SkyKind kind) {
    bool water = kind == SKY_WATER;
    struct CurrentEnvironmentRecord* env = fogGetCurrentEnvironmentp();
    f32 shear;
    Vtx* verts = dynAllocateVertices(count);
    Mtx* mv_render = dynAllocateMatrix();
    Mtxf mv;
    Mtxf* w2s = camGetWorldToScreenMtxf();
    f32 inv[3][3];
    f32 det;
    f32 eye[3];
    f32 rel[5][3];
    f32 zrange[2];
    f32 depth;
    f32 maxdepth = 0.0f;
    f32 mindepth = 1e9f;
    f32 pull;
    f32 maxabs = 1.0f;
    f32 fit;
    f32 tc_s[5];
    f32 tc_t[5];
    f32 mean_s = 0.0f;
    f32 mean_t = 0.0f;
    f32 shift_s;
    f32 shift_t;
    s32 i;
    s32 j;

    // Draw camera-relative: the world-to-camera matrix carries the camera position as its translation,
    // which can exceed the s15.16 range of an RSP matrix in large levels. Recover the eye position
    // (row-vector convention: view = world * R + t, so eye = -t * R^-1), subtract it from the vertices
    // and hand the RSP the rotation only.
    det = w2s->m[0][0] * (w2s->m[1][1] * w2s->m[2][2] - w2s->m[1][2] * w2s->m[2][1]) -
          w2s->m[0][1] * (w2s->m[1][0] * w2s->m[2][2] - w2s->m[1][2] * w2s->m[2][0]) +
          w2s->m[0][2] * (w2s->m[1][0] * w2s->m[2][1] - w2s->m[1][1] * w2s->m[2][0]);
    inv[0][0] = (w2s->m[1][1] * w2s->m[2][2] - w2s->m[1][2] * w2s->m[2][1]) / det;
    inv[0][1] = (w2s->m[0][2] * w2s->m[2][1] - w2s->m[0][1] * w2s->m[2][2]) / det;
    inv[0][2] = (w2s->m[0][1] * w2s->m[1][2] - w2s->m[0][2] * w2s->m[1][1]) / det;
    inv[1][0] = (w2s->m[1][2] * w2s->m[2][0] - w2s->m[1][0] * w2s->m[2][2]) / det;
    inv[1][1] = (w2s->m[0][0] * w2s->m[2][2] - w2s->m[0][2] * w2s->m[2][0]) / det;
    inv[1][2] = (w2s->m[0][2] * w2s->m[1][0] - w2s->m[0][0] * w2s->m[1][2]) / det;
    inv[2][0] = (w2s->m[1][0] * w2s->m[2][1] - w2s->m[1][1] * w2s->m[2][0]) / det;
    inv[2][1] = (w2s->m[0][1] * w2s->m[2][0] - w2s->m[0][0] * w2s->m[2][1]) / det;
    inv[2][2] = (w2s->m[0][0] * w2s->m[1][1] - w2s->m[0][1] * w2s->m[1][0]) / det;
    for (j = 0; j < 3; j++) {
        eye[j] = -(w2s->m[3][0] * inv[0][j] + w2s->m[3][1] * inv[1][j] + w2s->m[3][2] * inv[2][j]);
    }

    // src positions are world * scale.
    for (i = 0; i < count; i++) {
        rel[i][0] = src[i].unk00 / scale - eye[0];
        rel[i][1] = src[i].unk04 / scale - eye[1];
        rel[i][2] = src[i].unk08 / scale - eye[2];
    }

    // The plane lies up to 300000 units away, often past the far plane (night levels have short draw
    // distances), and RT64 clips there. Scaling the polygon towards the eye leaves its screen position and
    // perspective-correct texturing unchanged, and brings its depth inside the frustum. Near the camera
    // (water below the player) the depth range can exceed what fits, and near clipping is the visible one,
    // so the nearest vertex is kept at least 2x the near plane away.
    viGetZRange(zrange);
    for (i = 0; i < count; i++) {
        depth = -(rel[i][0] * w2s->m[0][2] + rel[i][1] * w2s->m[1][2] + rel[i][2] * w2s->m[2][2]);
        maxdepth = MAX(maxdepth, depth);
        mindepth = MIN(mindepth, depth);
    }
    if (maxdepth > zrange[1] * 0.9f) {
        pull = (zrange[1] * 0.9f) / maxdepth;
        if (mindepth > 0.0f) {
            pull = MIN(1.0f, MAX(pull, 2.0f * zrange[0] / mindepth));
        }
        for (i = 0; i < count; i++) {
            for (j = 0; j < 3; j++) {
                rel[i][j] *= pull;
            }
        }
    }

    for (i = 0; i < count; i++) {
        for (j = 0; j < 3; j++) {
            maxabs = MAX(maxabs, skyAbs(rel[i][j]));
        }
    }

    // Vertices get rel * fit to use the s16 range; the rotation-only matrix undoes the fit.
    // WaterConcavity: the game aims the corner rays that many pixels lower and then draws everything that
    // many pixels higher. The second half is a view-space shear, y += C * c_scaley * -z (row vectors).
    fit = SKY_VTX_RANGE / maxabs;
    shear = -env->WaterConcavity * g_CurrentPlayer->c_scaley;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            mv.m[i][j] = (i < 3 && j < 3) ? w2s->m[i][j] / fit : (i == j ? 1.0f : 0.0f);
        }
        if (i < 3) {
            mv.m[i][1] += mv.m[i][2] * shear;
        }
    }
    matrix_4x4_f32_to_s32(&mv, (Mtxf*) mv_render);

    for (i = 0; i < count; i++) {
        verts[i].v.ob[0] = rel[i][0] * fit;
        verts[i].v.ob[1] = rel[i][1] * fit;
        verts[i].v.ob[2] = rel[i][2] * fit;
        verts[i].v.flag = 0;

        if (water) {
            // Same texture coordinates as the RDP path (unk0c/unk10 are s10.5 there), stored /16 to fit s16
            // across a whole plane; the tiles below scale them back up.
            tc_s[i] = src[i].unk0c / SKY_WATER_TC_DIV;
            tc_t[i] = src[i].unk10 / SKY_WATER_TC_DIV;
        } else {
            tc_s[i] = src[i].unk0c;
            tc_t[i] = src[i].unk10;
        }
        mean_s += tc_s[i] / count;
        mean_t += tc_t[i] / count;

        verts[i].v.cn[0] = src[i].r;
        verts[i].v.cn[1] = src[i].g;
        verts[i].v.cn[2] = src[i].b;
        verts[i].v.cn[3] = src[i].a;
    }

    // Texture coordinates are world position based and reach far past the s16 range near the horizon,
    // where clamping smears the texture into streaks. The texture repeats, so shift the polygon's
    // coordinates towards zero by a whole number of repeats first. SKY_TC_PERIOD is 256 texels in
    // s10.5, a multiple of the repeat length of any sky/water texture up to 256 wide.
    shift_s = (s32) (mean_s / SKY_TC_PERIOD) * SKY_TC_PERIOD;
    shift_t = (s32) (mean_t / SKY_TC_PERIOD) * SKY_TC_PERIOD;
    for (i = 0; i < count; i++) {
        verts[i].v.tc[0] = skyClamp(tc_s[i] - shift_s, -32768.0f, 32767.0f);
        verts[i].v.tc[1] = skyClamp(tc_t[i] - shift_t, -32768.0f, 32767.0f);
    }

    gEXPushGeometryMode(gdl++);
    gSPClearGeometryMode(gdl++, G_CULL_BOTH | G_LIGHTING | G_FOG | G_ZBUFFER | G_TEXTURE_GEN);
    gSPSetGeometryMode(gdl++, G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(gdl++, 0xFFFF, 0xFFFF, water ? 1 : 0, G_TX_RENDERTILE, G_ON);

    // Nothing in skyRender sets a triangle-capable cycle type for the sky when the whole screen is sky: the
    // last state is usually fill mode from the Z buffer clear, which draws untextured. Water is set up by
    // sub_GAME_7F09343C as a two-tile blend, which needs 2-cycle mode.
    gEXPushOtherMode(gdl++);
    gDPPipeSync(gdl++);
    if (water) {
        gDPSetCycleType(gdl++, G_CYC_2CYCLE);
        gDPSetRenderMode(gdl++, G_RM_PASS, G_RM_OPA_SURF2);
        // As sub_GAME_7F09343C sets them up, plus the shift that undoes SKY_WATER_TC_DIV.
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, 0, 5, SKY_WATER_TILE_SHIFT, 0, 5,
                   SKY_WATER_TILE_SHIFT);
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 1, 0, 0, 5, SKY_WATER_TILE_SHIFT, 0, 5,
                   SKY_WATER_TILE_SHIFT);
    } else {
        gDPSetCycleType(gdl++, G_CYC_1CYCLE);
        gDPSetRenderMode(gdl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    }
    gDPSetTexturePersp(gdl++, G_TP_PERSP);
    if (kind == SKY_GROUND) {
        // Below the horizon without water the game fills with the environment colour.
        gDPSetCombineMode(gdl++, G_CC_SHADE, G_CC_SHADE);
        for (i = 0; i < count; i++) {
            verts[i].v.cn[0] = env->Red;
            verts[i].v.cn[1] = env->Green;
            verts[i].v.cn[2] = env->Blue;
            verts[i].v.cn[3] = 0xFF;
        }
    }

    // OS_K0_TO_PHYSICAL rather than osVirtualToPhysical: that one is replaced by the runtime, and calling the
    // game's copy from a patch returns garbage.
    gSPMatrix(gdl++, OS_K0_TO_PHYSICAL(mv_render), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
    gSPVertex(gdl++, OS_K0_TO_PHYSICAL(verts), count, 0);

    // Same triangulation as the original skyRenderTri calls.
    if (count == 4) {
        gSP4Triangles(gdl++, 0, 1, 3, 3, 2, 0, 0, 0, 0, 0, 0, 0);
    } else if (count == 5) {
        gSP4Triangles(gdl++, 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 0, 0);
    } else if (count == 3) {
        gSP4Triangles(gdl++, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    gSPPopMatrix(gdl++, G_MTX_MODELVIEW);
    gDPPipeSync(gdl++);
    if (water) {
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, 0, 5, 0, 0, 5, 0);
        gDPSetTile(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 1, 0, 0, 5, 0, 0, 5, 0);
    }
    gEXPopOtherMode(gdl++);
    gEXPopGeometryMode(gdl++);

    return gdl;
}

#if 1
static Gfx* skyRenderInner(Gfx* gdl);

// The game builds the sky polygon from the corners of its native view, but RT64 renders widescreen and past
// the top/bottom of the viewport. Build it for a larger view with the same centre and pixel scale; the extra
// area is simply off-screen when it isn't needed. 2x width covers aspect ratios up to 8:3.
#define SKY_WIDEN_X 2.0f
#define SKY_WIDEN_Y 1.25f

RECOMP_PATCH Gfx* skyRender(Gfx* gdl) {
    struct player* p = g_CurrentPlayer;
    f32 saved[6];

    saved[0] = p->c_screenwidth;
    saved[1] = p->c_screenheight;
    saved[2] = p->c_screenleft;
    saved[3] = p->c_screentop;
    saved[4] = p->c_halfwidth;
    saved[5] = p->c_halfheight;

    p->c_screenleft -= p->c_screenwidth * (SKY_WIDEN_X - 1.0f) * 0.5f;
    p->c_screentop -= p->c_screenheight * (SKY_WIDEN_Y - 1.0f) * 0.5f;
    p->c_screenwidth *= SKY_WIDEN_X;
    p->c_screenheight *= SKY_WIDEN_Y;
    p->c_halfwidth *= SKY_WIDEN_X;
    p->c_halfheight *= SKY_WIDEN_Y;

    gdl = skyRenderInner(gdl);

    p->c_screenwidth = saved[0];
    p->c_screenheight = saved[1];
    p->c_screenleft = saved[2];
    p->c_screentop = saved[3];
    p->c_halfwidth = saved[4];
    p->c_halfheight = saved[5];

    return gdl;
}

static Gfx* skyRenderInner(Gfx* gdl) __attribute__((optnone)) {
    coord3d sp6a4;
    coord3d sp698;
    coord3d sp68c;
    coord3d sp680;
    coord3d sp674;
    coord3d sp668;
    coord3d sp65c;
    coord3d sp650;
    coord3d sp644;
    coord3d sp638;
    coord3d sp62c;
    coord3d sp620;
    coord3d sp614;
    coord3d sp608;
    coord3d sp5fc;
    coord3d sp5f0;
    coord3d sp5e4;
    coord3d sp5d8;
    coord3d sp5cc;
    coord3d sp5c0;
    coord3d sp5b4;
    coord3d sp5a8;
    coord3d sp59c;
    coord3d sp590;
    f32 sp58c;
    f32 sp588;
    f32 sp584;
    f32 sp580;
    f32 sp57c;
    f32 sp578;
    f32 sp574;
    f32 sp570;
    f32 sp56c;
    f32 sp568;
    f32 sp564;
    f32 sp560;
    f32 sp55c;
    f32 sp558;
    f32 sp554;
    f32 sp550;
    f32 sp54c;
    f32 sp548;
    s32 s1;
    s32 j;
    s32 k;
    s32 sp538;
    s32 sp534;
    s32 sp530;
    s32 sp52c;
    SkyRelated18 sp4b4[5];
    SkyRelated18 sp43c[5];
    f32 tmp;
    f32 scale;
    bool sp430;
    struct CurrentEnvironmentRecord* env;

    scale = get_room_data_float1() / 30.0f;
    sp430 = FALSE;
    env = fogGetCurrentEnvironmentp();

    if (!fogGetCurrentEnvironmentp()->Clouds) {
        if (getPlayerCount() == 1) {
            gDPSetCycleType(gdl++, G_CYC_FILL);

            gdl = viSetFillColor(gdl, env->Red, env->Green, env->Blue);

            gDPFillRectangle(gdl++, viGetViewLeft(), viGetViewTop(), viGetViewLeft() + viGetViewWidth() - 1,
                             viGetViewTop() + viGetViewHeight() - 1);

            gDPPipeSync(gdl++);
            return gdl;
        }

        gDPPipeSync(gdl++);
        gDPSetCycleType(gdl++, G_CYC_FILL);

        gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);

        gDPFillRectangle(gdl++, g_CurrentPlayer->viewleft, g_CurrentPlayer->viewtop,
                         (g_CurrentPlayer->viewleft + g_CurrentPlayer->viewx) - 1,
                         (g_CurrentPlayer->viewtop + g_CurrentPlayer->viewy) - 1);

        gDPPipeSync(gdl++);
        return gdl;
    }

    gdl = viSetFillColor(gdl, env->Red, env->Green, env->Blue);

    if (&sp6a4)
        ;

    skyGetWorldPosFromScreenPos(0.0f, 0.0f, &sp6a4);
    skyGetWorldPosFromScreenPos(getPlayer_c_screenwidth() - 0.1f, 0.0f, &sp698);
    skyGetWorldPosFromScreenPos(0.0f, getPlayer_c_screenheight() - 0.1f, &sp68c);
    skyGetWorldPosFromScreenPos(getPlayer_c_screenwidth() - 0.1f, getPlayer_c_screenheight() - 0.1f, &sp680);

    sp538 = skyIsScreenCornerInSky(&sp6a4, &sp644, &sp58c);
    sp534 = skyIsScreenCornerInSky(&sp698, &sp638, &sp588);
    sp530 = skyIsScreenCornerInSky(&sp68c, &sp62c, &sp584);
    sp52c = skyIsScreenCornerInSky(&sp680, &sp620, &sp580);

    skyIsCornerInWater(&sp6a4, &sp5e4, &sp56c);
    skyIsCornerInWater(&sp698, &sp5d8, &sp568);
    skyIsCornerInWater(&sp68c, &sp5cc, &sp564);
    skyIsCornerInWater(&sp680, &sp5c0, &sp560);

    if (sp538 != sp530) {
        sp54c = getPlayer_c_screentop() + getPlayer_c_screenheight() * (sp6a4.f[1] / (sp6a4.f[1] - sp68c.f[1]));

        skyGetWorldPosFromScreenPos(0.0f, sp54c, &sp65c);
        skyCalculateEdgeVertex(&sp6a4, &sp68c, &sp65c);
        skyIsScreenCornerInSky(&sp65c, &sp5fc, &sp574);
        skyIsCornerInWater(&sp65c, &sp59c, &sp554);
    } else {
        sp54c = 0.0f;
    }

    if (sp534 != sp52c) {
        sp548 = getPlayer_c_screentop() + getPlayer_c_screenheight() * (sp698.f[1] / (sp698.f[1] - sp680.f[1]));

        skyGetWorldPosFromScreenPos(getPlayer_c_screenwidth() - 0.1f, sp548, &sp650);
        skyCalculateEdgeVertex(&sp698, &sp680, &sp650);
        skyIsScreenCornerInSky(&sp650, &sp5f0, &sp570);
        skyIsCornerInWater(&sp650, &sp590, &sp550);
    } else {
        sp548 = 0.0f;
    }

    if (sp538 != sp534) {
        skyGetWorldPosFromScreenPos(getPlayer_c_screenleft() +
                              getPlayer_c_screenwidth() * (sp6a4.f[1] / (sp6a4.f[1] - sp698.f[1])),
                          0.0f, &sp674);
        skyCalculateEdgeVertex(&sp6a4, &sp698, &sp674);
        skyIsScreenCornerInSky(&sp674, &sp614, &sp57c);
        skyIsCornerInWater(&sp674, &sp5b4, &sp55c);
    }

    if (sp530 != sp52c) {
        tmp = getPlayer_c_screenleft() + getPlayer_c_screenwidth() * (sp68c.f[1] / (sp68c.f[1] - sp680.f[1]));

        skyGetWorldPosFromScreenPos(tmp, getPlayer_c_screenheight() - 0.1f, &sp668);
        skyCalculateEdgeVertex(&sp68c, &sp680, &sp668);
        skyIsScreenCornerInSky(&sp668, &sp608, &sp578);
        skyIsCornerInWater(&sp668, &sp5a8, &sp558);
    }

    switch ((sp538 << 3) | (sp534 << 2) | (sp530 << 1) | sp52c) {
        case 15:
            s1 = 0;
            break;

        case 0:
            s1 = 4;
            sp43c[0].unk00 = sp5e4.f[0] * scale;
            sp43c[0].unk04 = sp5e4.f[1] * scale;
            sp43c[0].unk08 = sp5e4.f[2] * scale;
            sp43c[1].unk00 = sp5d8.f[0] * scale;
            sp43c[1].unk04 = sp5d8.f[1] * scale;
            sp43c[1].unk08 = sp5d8.f[2] * scale;
            sp43c[2].unk00 = sp5cc.f[0] * scale;
            sp43c[2].unk04 = sp5cc.f[1] * scale;
            sp43c[2].unk08 = sp5cc.f[2] * scale;
            sp43c[3].unk00 = sp5c0.f[0] * scale;
            sp43c[3].unk04 = sp5c0.f[1] * scale;
            sp43c[3].unk08 = sp5c0.f[2] * scale;
            sp43c[0].unk0c = sp5e4.f[0];
            sp43c[0].unk10 = sp5e4.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5d8.f[0];
            sp43c[1].unk10 = sp5d8.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5cc.f[0];
            sp43c[2].unk10 = sp5cc.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp5c0.f[0];
            sp43c[3].unk10 = sp5c0.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp56c);
            skyChooseWaterVtxColour(&sp43c[1], sp568);
            skyChooseWaterVtxColour(&sp43c[2], sp564);
            skyChooseWaterVtxColour(&sp43c[3], sp560);
            break;

        case 3:
            s1 = 4;
            sp43c[0].unk00 = sp5e4.f[0] * scale;
            sp43c[0].unk04 = sp5e4.f[1] * scale;
            sp43c[0].unk08 = sp5e4.f[2] * scale;
            sp43c[1].unk00 = sp5d8.f[0] * scale;
            sp43c[1].unk04 = sp5d8.f[1] * scale;
            sp43c[1].unk08 = sp5d8.f[2] * scale;
            sp43c[2].unk00 = sp59c.f[0] * scale;
            sp43c[2].unk04 = sp59c.f[1] * scale;
            sp43c[2].unk08 = sp59c.f[2] * scale;
            sp43c[3].unk00 = sp590.f[0] * scale;
            sp43c[3].unk04 = sp590.f[1] * scale;
            sp43c[3].unk08 = sp590.f[2] * scale;
            sp43c[0].unk0c = sp5e4.f[0];
            sp43c[0].unk10 = sp5e4.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5d8.f[0];
            sp43c[1].unk10 = sp5d8.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp59c.f[0];
            sp43c[2].unk10 = sp59c.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp590.f[0];
            sp43c[3].unk10 = sp590.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp56c);
            skyChooseWaterVtxColour(&sp43c[1], sp568);
            skyChooseWaterVtxColour(&sp43c[2], sp554);
            skyChooseWaterVtxColour(&sp43c[3], sp550);
            break;

        case 12:
            s1 = 4;
            sp430 = TRUE;
            sp43c[0].unk00 = sp5c0.f[0] * scale;
            sp43c[0].unk04 = sp5c0.f[1] * scale;
            sp43c[0].unk08 = sp5c0.f[2] * scale;
            sp43c[1].unk00 = sp5cc.f[0] * scale;
            sp43c[1].unk04 = sp5cc.f[1] * scale;
            sp43c[1].unk08 = sp5cc.f[2] * scale;
            sp43c[2].unk00 = sp590.f[0] * scale;
            sp43c[2].unk04 = sp590.f[1] * scale;
            sp43c[2].unk08 = sp590.f[2] * scale;
            sp43c[3].unk00 = sp59c.f[0] * scale;
            sp43c[3].unk04 = sp59c.f[1] * scale;
            sp43c[3].unk08 = sp59c.f[2] * scale;
            sp43c[0].unk0c = sp5c0.f[0];
            sp43c[0].unk10 = sp5c0.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5cc.f[0];
            sp43c[1].unk10 = sp5cc.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp590.f[0];
            sp43c[2].unk10 = sp590.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp59c.f[0];
            sp43c[3].unk10 = sp59c.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp560);
            skyChooseWaterVtxColour(&sp43c[1], sp564);
            skyChooseWaterVtxColour(&sp43c[2], sp550);
            skyChooseWaterVtxColour(&sp43c[3], sp554);
            break;

        case 10:
            s1 = 4;
            sp43c[0].unk00 = sp5d8.f[0] * scale;
            sp43c[0].unk04 = sp5d8.f[1] * scale;
            sp43c[0].unk08 = sp5d8.f[2] * scale;
            sp43c[1].unk00 = sp5c0.f[0] * scale;
            sp43c[1].unk04 = sp5c0.f[1] * scale;
            sp43c[1].unk08 = sp5c0.f[2] * scale;
            sp43c[2].unk00 = sp5b4.f[0] * scale;
            sp43c[2].unk04 = sp5b4.f[1] * scale;
            sp43c[2].unk08 = sp5b4.f[2] * scale;
            sp43c[3].unk00 = sp5a8.f[0] * scale;
            sp43c[3].unk04 = sp5a8.f[1] * scale;
            sp43c[3].unk08 = sp5a8.f[2] * scale;
            sp43c[0].unk0c = sp5d8.f[0];
            sp43c[0].unk10 = sp5d8.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5c0.f[0];
            sp43c[1].unk10 = sp5c0.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5b4.f[0];
            sp43c[2].unk10 = sp5b4.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp5a8.f[0];
            sp43c[3].unk10 = sp5a8.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp568);
            skyChooseWaterVtxColour(&sp43c[1], sp560);
            skyChooseWaterVtxColour(&sp43c[2], sp55c);
            skyChooseWaterVtxColour(&sp43c[3], sp558);
            break;

        case 5:
            s1 = 4;
            sp43c[0].unk00 = sp5cc.f[0] * scale;
            sp43c[0].unk04 = sp5cc.f[1] * scale;
            sp43c[0].unk08 = sp5cc.f[2] * scale;
            sp43c[1].unk00 = sp5e4.f[0] * scale;
            sp43c[1].unk04 = sp5e4.f[1] * scale;
            sp43c[1].unk08 = sp5e4.f[2] * scale;
            sp43c[2].unk00 = sp5a8.f[0] * scale;
            sp43c[2].unk04 = sp5a8.f[1] * scale;
            sp43c[2].unk08 = sp5a8.f[2] * scale;
            sp43c[3].unk00 = sp5b4.f[0] * scale;
            sp43c[3].unk04 = sp5b4.f[1] * scale;
            sp43c[3].unk08 = sp5b4.f[2] * scale;
            sp43c[0].unk0c = sp5cc.f[0];
            sp43c[0].unk10 = sp5cc.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5e4.f[0];
            sp43c[1].unk10 = sp5e4.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5a8.f[0];
            sp43c[2].unk10 = sp5a8.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp5b4.f[0];
            sp43c[3].unk10 = sp5b4.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp564);
            skyChooseWaterVtxColour(&sp43c[1], sp56c);
            skyChooseWaterVtxColour(&sp43c[2], sp558);
            skyChooseWaterVtxColour(&sp43c[3], sp55c);
            break;

        case 14:
            s1 = 3;
            sp43c[0].unk00 = sp5c0.f[0] * scale;
            sp43c[0].unk04 = sp5c0.f[1] * scale;
            sp43c[0].unk08 = sp5c0.f[2] * scale;
            sp43c[1].unk00 = sp5a8.f[0] * scale;
            sp43c[1].unk04 = sp5a8.f[1] * scale;
            sp43c[1].unk08 = sp5a8.f[2] * scale;
            sp43c[2].unk00 = sp590.f[0] * scale;
            sp43c[2].unk04 = sp590.f[1] * scale;
            sp43c[2].unk08 = sp590.f[2] * scale;
            sp43c[0].unk0c = sp5c0.f[0];
            sp43c[0].unk10 = sp5c0.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5a8.f[0];
            sp43c[1].unk10 = sp5a8.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp590.f[0];
            sp43c[2].unk10 = sp590.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp560);
            skyChooseWaterVtxColour(&sp43c[1], sp558);
            skyChooseWaterVtxColour(&sp43c[2], sp550);
            break;

        case 13:
            s1 = 3;
            sp43c[0].unk00 = sp5cc.f[0] * scale;
            sp43c[0].unk04 = sp5cc.f[1] * scale;
            sp43c[0].unk08 = sp5cc.f[2] * scale;
            sp43c[1].unk00 = sp59c.f[0] * scale;
            sp43c[1].unk04 = sp59c.f[1] * scale;
            sp43c[1].unk08 = sp59c.f[2] * scale;
            sp43c[2].unk00 = sp5a8.f[0] * scale;
            sp43c[2].unk04 = sp5a8.f[1] * scale;
            sp43c[2].unk08 = sp5a8.f[2] * scale;
            sp43c[0].unk0c = sp5cc.f[0];
            sp43c[0].unk10 = sp5cc.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp59c.f[0];
            sp43c[1].unk10 = sp59c.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5a8.f[0];
            sp43c[2].unk10 = sp5a8.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp564);
            skyChooseWaterVtxColour(&sp43c[1], sp554);
            skyChooseWaterVtxColour(&sp43c[2], sp558);
            break;

        case 11:
            s1 = 3;
            sp43c[0].unk00 = sp5d8.f[0] * scale;
            sp43c[0].unk04 = sp5d8.f[1] * scale;
            sp43c[0].unk08 = sp5d8.f[2] * scale;
            sp43c[1].unk00 = sp590.f[0] * scale;
            sp43c[1].unk04 = sp590.f[1] * scale;
            sp43c[1].unk08 = sp590.f[2] * scale;
            sp43c[2].unk00 = sp5b4.f[0] * scale;
            sp43c[2].unk04 = sp5b4.f[1] * scale;
            sp43c[2].unk08 = sp5b4.f[2] * scale;
            sp43c[0].unk0c = sp5d8.f[0];
            sp43c[0].unk10 = sp5d8.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp590.f[0];
            sp43c[1].unk10 = sp590.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5b4.f[0];
            sp43c[2].unk10 = sp5b4.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp568);
            skyChooseWaterVtxColour(&sp43c[1], sp550);
            skyChooseWaterVtxColour(&sp43c[2], sp55c);
            break;

        case 7:
            s1 = 3;
            sp43c[0].unk00 = sp5e4.f[0] * scale;
            sp43c[0].unk04 = sp5e4.f[1] * scale;
            sp43c[0].unk08 = sp5e4.f[2] * scale;
            sp43c[1].unk00 = sp5b4.f[0] * scale;
            sp43c[1].unk04 = sp5b4.f[1] * scale;
            sp43c[1].unk08 = sp5b4.f[2] * scale;
            sp43c[2].unk00 = sp59c.f[0] * scale;
            sp43c[2].unk04 = sp59c.f[1] * scale;
            sp43c[2].unk08 = sp59c.f[2] * scale;
            sp43c[0].unk0c = sp5e4.f[0];
            sp43c[0].unk10 = sp5e4.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5b4.f[0];
            sp43c[1].unk10 = sp5b4.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp59c.f[0];
            sp43c[2].unk10 = sp59c.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp56c);
            skyChooseWaterVtxColour(&sp43c[1], sp55c);
            skyChooseWaterVtxColour(&sp43c[2], sp554);
            break;

        case 1:
            s1 = 5;
            sp43c[0].unk00 = sp5cc.f[0] * scale;
            sp43c[0].unk04 = sp5cc.f[1] * scale;
            sp43c[0].unk08 = sp5cc.f[2] * scale;
            sp43c[1].unk00 = sp5e4.f[0] * scale;
            sp43c[1].unk04 = sp5e4.f[1] * scale;
            sp43c[1].unk08 = sp5e4.f[2] * scale;
            sp43c[2].unk00 = sp5d8.f[0] * scale;
            sp43c[2].unk04 = sp5d8.f[1] * scale;
            sp43c[2].unk08 = sp5d8.f[2] * scale;
            sp43c[3].unk00 = sp590.f[0] * scale;
            sp43c[3].unk04 = sp590.f[1] * scale;
            sp43c[3].unk08 = sp590.f[2] * scale;
            sp43c[4].unk00 = sp5a8.f[0] * scale;
            sp43c[4].unk04 = sp5a8.f[1] * scale;
            sp43c[4].unk08 = sp5a8.f[2] * scale;
            sp43c[0].unk0c = sp5cc.f[0];
            sp43c[0].unk10 = sp5cc.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5e4.f[0];
            sp43c[1].unk10 = sp5e4.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5d8.f[0];
            sp43c[2].unk10 = sp5d8.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp590.f[0];
            sp43c[3].unk10 = sp590.f[2] + g_SkyCloudOffset;
            sp43c[4].unk0c = sp5a8.f[0];
            sp43c[4].unk10 = sp5a8.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp564);
            skyChooseWaterVtxColour(&sp43c[1], sp56c);
            skyChooseWaterVtxColour(&sp43c[2], sp568);
            skyChooseWaterVtxColour(&sp43c[3], sp550);
            skyChooseWaterVtxColour(&sp43c[4], sp558);
            break;

        case 2:
            s1 = 5;
            sp43c[0].unk00 = sp5e4.f[0] * scale;
            sp43c[0].unk04 = sp5e4.f[1] * scale;
            sp43c[0].unk08 = sp5e4.f[2] * scale;
            sp43c[1].unk00 = sp5d8.f[0] * scale;
            sp43c[1].unk04 = sp5d8.f[1] * scale;
            sp43c[1].unk08 = sp5d8.f[2] * scale;
            sp43c[2].unk00 = sp5c0.f[0] * scale;
            sp43c[2].unk04 = sp5c0.f[1] * scale;
            sp43c[2].unk08 = sp5c0.f[2] * scale;
            sp43c[3].unk00 = sp5a8.f[0] * scale;
            sp43c[3].unk04 = sp5a8.f[1] * scale;
            sp43c[3].unk08 = sp5a8.f[2] * scale;
            sp43c[4].unk00 = sp59c.f[0] * scale;
            sp43c[4].unk04 = sp59c.f[1] * scale;
            sp43c[4].unk08 = sp59c.f[2] * scale;
            sp43c[0].unk0c = sp5e4.f[0];
            sp43c[0].unk10 = sp5e4.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5d8.f[0];
            sp43c[1].unk10 = sp5d8.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5c0.f[0];
            sp43c[2].unk10 = sp5c0.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp5a8.f[0];
            sp43c[3].unk10 = sp5a8.f[2] + g_SkyCloudOffset;
            sp43c[4].unk0c = sp59c.f[0];
            sp43c[4].unk10 = sp59c.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp56c);
            skyChooseWaterVtxColour(&sp43c[1], sp568);
            skyChooseWaterVtxColour(&sp43c[2], sp560);
            skyChooseWaterVtxColour(&sp43c[3], sp558);
            skyChooseWaterVtxColour(&sp43c[4], sp554);
            break;

        case 4:
            s1 = 5;
            sp43c[0].unk00 = sp5c0.f[0] * scale;
            sp43c[0].unk04 = sp5c0.f[1] * scale;
            sp43c[0].unk08 = sp5c0.f[2] * scale;
            sp43c[1].unk00 = sp5cc.f[0] * scale;
            sp43c[1].unk04 = sp5cc.f[1] * scale;
            sp43c[1].unk08 = sp5cc.f[2] * scale;
            sp43c[2].unk00 = sp5e4.f[0] * scale;
            sp43c[2].unk04 = sp5e4.f[1] * scale;
            sp43c[2].unk08 = sp5e4.f[2] * scale;
            sp43c[3].unk00 = sp5b4.f[0] * scale;
            sp43c[3].unk04 = sp5b4.f[1] * scale;
            sp43c[3].unk08 = sp5b4.f[2] * scale;
            sp43c[4].unk00 = sp590.f[0] * scale;
            sp43c[4].unk04 = sp590.f[1] * scale;
            sp43c[4].unk08 = sp590.f[2] * scale;
            sp43c[0].unk0c = sp5c0.f[0];
            sp43c[0].unk10 = sp5c0.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5cc.f[0];
            sp43c[1].unk10 = sp5cc.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5e4.f[0];
            sp43c[2].unk10 = sp5e4.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp5b4.f[0];
            sp43c[3].unk10 = sp5b4.f[2] + g_SkyCloudOffset;
            sp43c[4].unk0c = sp590.f[0];
            sp43c[4].unk10 = sp590.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp560);
            skyChooseWaterVtxColour(&sp43c[1], sp564);
            skyChooseWaterVtxColour(&sp43c[2], sp56c);
            skyChooseWaterVtxColour(&sp43c[3], sp55c);
            skyChooseWaterVtxColour(&sp43c[4], sp550);
            break;

        case 8:
            s1 = 5;
            sp43c[0].unk00 = sp5d8.f[0] * scale;
            sp43c[0].unk04 = sp5d8.f[1] * scale;
            sp43c[0].unk08 = sp5d8.f[2] * scale;
            sp43c[1].unk00 = sp5c0.f[0] * scale;
            sp43c[1].unk04 = sp5c0.f[1] * scale;
            sp43c[1].unk08 = sp5c0.f[2] * scale;
            sp43c[2].unk00 = sp5cc.f[0] * scale;
            sp43c[2].unk04 = sp5cc.f[1] * scale;
            sp43c[2].unk08 = sp5cc.f[2] * scale;
            sp43c[3].unk00 = sp59c.f[0] * scale;
            sp43c[3].unk04 = sp59c.f[1] * scale;
            sp43c[3].unk08 = sp59c.f[2] * scale;
            sp43c[4].unk00 = sp5b4.f[0] * scale;
            sp43c[4].unk04 = sp5b4.f[1] * scale;
            sp43c[4].unk08 = sp5b4.f[2] * scale;
            sp43c[0].unk0c = sp5d8.f[0];
            sp43c[0].unk10 = sp5d8.f[2] + g_SkyCloudOffset;
            sp43c[1].unk0c = sp5c0.f[0];
            sp43c[1].unk10 = sp5c0.f[2] + g_SkyCloudOffset;
            sp43c[2].unk0c = sp5cc.f[0];
            sp43c[2].unk10 = sp5cc.f[2] + g_SkyCloudOffset;
            sp43c[3].unk0c = sp59c.f[0];
            sp43c[3].unk10 = sp59c.f[2] + g_SkyCloudOffset;
            sp43c[4].unk0c = sp5b4.f[0];
            sp43c[4].unk10 = sp5b4.f[2] + g_SkyCloudOffset;

            skyChooseWaterVtxColour(&sp43c[0], sp568);
            skyChooseWaterVtxColour(&sp43c[1], sp560);
            skyChooseWaterVtxColour(&sp43c[2], sp564);
            skyChooseWaterVtxColour(&sp43c[3], sp554);
            skyChooseWaterVtxColour(&sp43c[4], sp55c);
            break;

        default:
            return gdl;
    }

    if (s1 > 0) {
        Mtxf sp3cc;
        Mtxf sp38c;
        SkyRelated38 sp274[5];
        s32 i;
        s32 unused[3];

        matrix_4x4_multiply(currentPlayerGetProjectionMatrixF(), camGetWorldToScreenMtxf(), &sp3cc);
        guScaleF(dword_CODE_bss_80079E98.m, 1.0f / scale, 1.0f / scale, 1.0f / scale);
        matrix_4x4_multiply(&sp3cc, &dword_CODE_bss_80079E98, &sp38c);

        for (i = 0; i < s1; i++) {
            sub_GAME_7F097388(&sp43c[i], &sp38c, 130, 65535.0f, 65535.0f, &sp274[i]);

            sp274[i].unk28 = skyClamp(sp274[i].unk28, getPlayer_c_screenleft() * 4.0f,
                                      (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f);
            sp274[i].unk2c = skyClamp(sp274[i].unk2c, getPlayer_c_screentop() * 4.0f,
                                      (getPlayer_c_screentop() + getPlayer_c_screenheight()) * 4.0f - 1.0f);

            if (sp274[i].unk2c > getPlayer_c_screentop() * 4.0f + 4.0f &&
                sp274[i].unk2c < (getPlayer_c_screentop() + getPlayer_c_screenheight()) * 4.0f - 4.0f) {
                sp274[i].unk2c -= 4.0f;
            }
        }

        if (!fogGetCurrentEnvironmentp()->IsWater) {
#if PORTSKY
            gdl = skyDrawPolygon(gdl, sp43c, s1, scale, SKY_GROUND);
#else
            f32 f14 = 1279.0f;
            f32 f2 = 0.0f;
            f32 f16 = 959.0f;
            f32 f12 = 0.0f;

            for (j = 0; j < s1; j++) {
                if (sp274[j].unk28 < f14) {
                    f14 = sp274[j].unk28;
                }
                if (sp274[j].unk28 > f2) {
                    f2 = sp274[j].unk28;
                }

                if (sp274[j].unk2c < f16) {
                    f16 = sp274[j].unk2c;
                }
                if (sp274[j].unk2c > f12) {
                    f12 = sp274[j].unk2c;
                }
            }

            gDPPipeSync(gdl++);
            gDPSetCycleType(gdl++, G_CYC_FILL);
            gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetTexturePersp(gdl++, G_TP_NONE);
            gDPFillRectangle(gdl++, (s32) (f14 * 0.25f), (s32) (f16 * 0.25f), (s32) (f2 * 0.25f), (s32) (f12 * 0.25f));
            gDPPipeSync(gdl++);
            gDPSetTexturePersp(gdl++, G_TP_PERSP);
#endif
        } else {
            gDPPipeSync(gdl++);

            texSelect(&gdl, &skywaterimages[fogGetCurrentEnvironmentp()->WaterImageId], 1, 0, 2);
            gdl = sub_GAME_7F09343C(gdl, 0); // ???
            gDPSetRenderMode(gdl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

#if PORTSKY == 0
            if (s1 == 4) {
                gdl = skyRenderTri(gdl, &sp274[0], &sp274[1], &sp274[3], 130.0f, TRUE);

                if (sp430) {
                    sp274[0].unk2c++;
                    sp274[1].unk2c++;
                    sp274[2].unk2c++;
                    sp274[3].unk2c++;
                }

                gdl = skyRenderTri(gdl, &sp274[3], &sp274[2], &sp274[0], 130.0f, TRUE);
            } else if (s1 == 5) {
                gdl = skyRenderTri(gdl, &sp274[0], &sp274[1], &sp274[2], 130.0f, TRUE);
                gdl = skyRenderTri(gdl, &sp274[0], &sp274[2], &sp274[3], 130.0f, TRUE);
                gdl = skyRenderTri(gdl, &sp274[0], &sp274[3], &sp274[4], 130.0f, TRUE);
            } else if (s1 == 3) {
                gdl = skyRenderTri(gdl, &sp274[0], &sp274[1], &sp274[2], 130.0f, TRUE);
            }
#else
            gdl = skyDrawPolygon(gdl, sp43c, s1, scale, SKY_WATER);
#endif
        }
    }

    switch ((sp538 << 3) | (sp534 << 2) | (sp530 << 1) | sp52c) {
        case 0:
            return gdl;

        case 15:
            s1 = 4;
            sp4b4[0].unk00 = sp644.f[0] * scale;
            sp4b4[0].unk04 = sp644.f[1] * scale;
            sp4b4[0].unk08 = sp644.f[2] * scale;
            sp4b4[1].unk00 = sp638.f[0] * scale;
            sp4b4[1].unk04 = sp638.f[1] * scale;
            sp4b4[1].unk08 = sp638.f[2] * scale;
            sp4b4[2].unk00 = sp62c.f[0] * scale;
            sp4b4[2].unk04 = sp62c.f[1] * scale;
            sp4b4[2].unk08 = sp62c.f[2] * scale;
            sp4b4[3].unk00 = sp620.f[0] * scale;
            sp4b4[3].unk04 = sp620.f[1] * scale;
            sp4b4[3].unk08 = sp620.f[2] * scale;
            sp4b4[0].unk0c = sp644.f[0] * 0.1f;
            sp4b4[0].unk10 = sp644.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp638.f[0] * 0.1f;
            sp4b4[1].unk10 = sp638.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp62c.f[0] * 0.1f;
            sp4b4[2].unk10 = sp62c.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp620.f[0] * 0.1f;
            sp4b4[3].unk10 = sp620.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp58c);
            skyChooseCloudVtxColour(&sp4b4[1], sp588);
            skyChooseCloudVtxColour(&sp4b4[2], sp584);
            skyChooseCloudVtxColour(&sp4b4[3], sp580);
            break;

        case 12:
            s1 = 4;
            sp4b4[0].unk00 = sp644.f[0] * scale;
            sp4b4[0].unk04 = sp644.f[1] * scale;
            sp4b4[0].unk08 = sp644.f[2] * scale;
            sp4b4[1].unk00 = sp638.f[0] * scale;
            sp4b4[1].unk04 = sp638.f[1] * scale;
            sp4b4[1].unk08 = sp638.f[2] * scale;
            sp4b4[2].unk00 = sp5fc.f[0] * scale;
            sp4b4[2].unk04 = sp5fc.f[1] * scale;
            sp4b4[2].unk08 = sp5fc.f[2] * scale;
            sp4b4[3].unk00 = sp5f0.f[0] * scale;
            sp4b4[3].unk04 = sp5f0.f[1] * scale;
            sp4b4[3].unk08 = sp5f0.f[2] * scale;
            sp4b4[0].unk0c = sp644.f[0] * 0.1f;
            sp4b4[0].unk10 = sp644.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp638.f[0] * 0.1f;
            sp4b4[1].unk10 = sp638.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp5fc.f[0] * 0.1f;
            sp4b4[2].unk10 = sp5fc.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp5f0.f[0] * 0.1f;
            sp4b4[3].unk10 = sp5f0.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp58c);
            skyChooseCloudVtxColour(&sp4b4[1], sp588);
            skyChooseCloudVtxColour(&sp4b4[2], sp574);
            skyChooseCloudVtxColour(&sp4b4[3], sp570);
            break;

        case 3:
            s1 = 4;
            sp4b4[0].unk00 = sp620.f[0] * scale;
            sp4b4[0].unk04 = sp620.f[1] * scale;
            sp4b4[0].unk08 = sp620.f[2] * scale;
            sp4b4[1].unk00 = sp62c.f[0] * scale;
            sp4b4[1].unk04 = sp62c.f[1] * scale;
            sp4b4[1].unk08 = sp62c.f[2] * scale;
            sp4b4[2].unk00 = sp5f0.f[0] * scale;
            sp4b4[2].unk04 = sp5f0.f[1] * scale;
            sp4b4[2].unk08 = sp5f0.f[2] * scale;
            sp4b4[3].unk00 = sp5fc.f[0] * scale;
            sp4b4[3].unk04 = sp5fc.f[1] * scale;
            sp4b4[3].unk08 = sp5fc.f[2] * scale;
            sp4b4[0].unk0c = sp620.f[0] * 0.1f;
            sp4b4[0].unk10 = sp620.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp62c.f[0] * 0.1f;
            sp4b4[1].unk10 = sp62c.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp5f0.f[0] * 0.1f;
            sp4b4[2].unk10 = sp5f0.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp5fc.f[0] * 0.1f;
            sp4b4[3].unk10 = sp5fc.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp580);
            skyChooseCloudVtxColour(&sp4b4[1], sp584);
            skyChooseCloudVtxColour(&sp4b4[2], sp570);
            skyChooseCloudVtxColour(&sp4b4[3], sp574);
            break;

        case 5:
            s1 = 4;
            sp4b4[0].unk00 = sp638.f[0] * scale;
            sp4b4[0].unk04 = sp638.f[1] * scale;
            sp4b4[0].unk08 = sp638.f[2] * scale;
            sp4b4[1].unk00 = sp620.f[0] * scale;
            sp4b4[1].unk04 = sp620.f[1] * scale;
            sp4b4[1].unk08 = sp620.f[2] * scale;
            sp4b4[2].unk00 = sp614.f[0] * scale;
            sp4b4[2].unk04 = sp614.f[1] * scale;
            sp4b4[2].unk08 = sp614.f[2] * scale;
            sp4b4[3].unk00 = sp608.f[0] * scale;
            sp4b4[3].unk04 = sp608.f[1] * scale;
            sp4b4[3].unk08 = sp608.f[2] * scale;
            sp4b4[0].unk0c = sp638.f[0] * 0.1f;
            sp4b4[0].unk10 = sp638.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp620.f[0] * 0.1f;
            sp4b4[1].unk10 = sp620.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp614.f[0] * 0.1f;
            sp4b4[2].unk10 = sp614.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp608.f[0] * 0.1f;
            sp4b4[3].unk10 = sp608.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp588);
            skyChooseCloudVtxColour(&sp4b4[1], sp580);
            skyChooseCloudVtxColour(&sp4b4[2], sp57c);
            skyChooseCloudVtxColour(&sp4b4[3], sp578);
            break;

        case 10:
            s1 = 4;
            sp4b4[0].unk00 = sp62c.f[0] * scale;
            sp4b4[0].unk04 = sp62c.f[1] * scale;
            sp4b4[0].unk08 = sp62c.f[2] * scale;
            sp4b4[1].unk00 = sp644.f[0] * scale;
            sp4b4[1].unk04 = sp644.f[1] * scale;
            sp4b4[1].unk08 = sp644.f[2] * scale;
            sp4b4[2].unk00 = sp608.f[0] * scale;
            sp4b4[2].unk04 = sp608.f[1] * scale;
            sp4b4[2].unk08 = sp608.f[2] * scale;
            sp4b4[3].unk00 = sp614.f[0] * scale;
            sp4b4[3].unk04 = sp614.f[1] * scale;
            sp4b4[3].unk08 = sp614.f[2] * scale;
            sp4b4[0].unk0c = sp62c.f[0] * 0.1f;
            sp4b4[0].unk10 = sp62c.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp644.f[0] * 0.1f;
            sp4b4[1].unk10 = sp644.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp608.f[0] * 0.1f;
            sp4b4[2].unk10 = sp608.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp614.f[0] * 0.1f;
            sp4b4[3].unk10 = sp614.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp584);
            skyChooseCloudVtxColour(&sp4b4[1], sp58c);
            skyChooseCloudVtxColour(&sp4b4[2], sp578);
            skyChooseCloudVtxColour(&sp4b4[3], sp57c);
            break;

        case 1:
            s1 = 3;
            sp4b4[0].unk00 = sp620.f[0] * scale;
            sp4b4[0].unk04 = sp620.f[1] * scale;
            sp4b4[0].unk08 = sp620.f[2] * scale;
            sp4b4[1].unk00 = sp608.f[0] * scale;
            sp4b4[1].unk04 = sp608.f[1] * scale;
            sp4b4[1].unk08 = sp608.f[2] * scale;
            sp4b4[2].unk00 = sp5f0.f[0] * scale;
            sp4b4[2].unk04 = sp5f0.f[1] * scale;
            sp4b4[2].unk08 = sp5f0.f[2] * scale;
            sp4b4[0].unk0c = sp620.f[0] * 0.1f;
            sp4b4[0].unk10 = sp620.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp608.f[0] * 0.1f;
            sp4b4[1].unk10 = sp608.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp5f0.f[0] * 0.1f;
            sp4b4[2].unk10 = sp5f0.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp580);
            skyChooseCloudVtxColour(&sp4b4[1], sp578);
            skyChooseCloudVtxColour(&sp4b4[2], sp570);
            break;

        case 2:
            s1 = 3;
            sp4b4[0].unk00 = sp62c.f[0] * scale;
            sp4b4[0].unk04 = sp62c.f[1] * scale;
            sp4b4[0].unk08 = sp62c.f[2] * scale;
            sp4b4[1].unk00 = sp5fc.f[0] * scale;
            sp4b4[1].unk04 = sp5fc.f[1] * scale;
            sp4b4[1].unk08 = sp5fc.f[2] * scale;
            sp4b4[2].unk00 = sp608.f[0] * scale;
            sp4b4[2].unk04 = sp608.f[1] * scale;
            sp4b4[2].unk08 = sp608.f[2] * scale;
            sp4b4[0].unk0c = sp62c.f[0] * 0.1f;
            sp4b4[0].unk10 = sp62c.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp5fc.f[0] * 0.1f;
            sp4b4[1].unk10 = sp5fc.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp608.f[0] * 0.1f;
            sp4b4[2].unk10 = sp608.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp584);
            skyChooseCloudVtxColour(&sp4b4[1], sp574);
            skyChooseCloudVtxColour(&sp4b4[2], sp578);
            break;

        case 4:
            s1 = 3;
            sp4b4[0].unk00 = sp638.f[0] * scale;
            sp4b4[0].unk04 = sp638.f[1] * scale;
            sp4b4[0].unk08 = sp638.f[2] * scale;
            sp4b4[1].unk00 = sp5f0.f[0] * scale;
            sp4b4[1].unk04 = sp5f0.f[1] * scale;
            sp4b4[1].unk08 = sp5f0.f[2] * scale;
            sp4b4[2].unk00 = sp614.f[0] * scale;
            sp4b4[2].unk04 = sp614.f[1] * scale;
            sp4b4[2].unk08 = sp614.f[2] * scale;
            sp4b4[0].unk0c = sp638.f[0] * 0.1f;
            sp4b4[0].unk10 = sp638.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp5f0.f[0] * 0.1f;
            sp4b4[1].unk10 = sp5f0.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp614.f[0] * 0.1f;
            sp4b4[2].unk10 = sp614.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp588);
            skyChooseCloudVtxColour(&sp4b4[1], sp570);
            skyChooseCloudVtxColour(&sp4b4[2], sp57c);
            break;

        case 8:
            s1 = 3;
            sp4b4[0].unk00 = sp644.f[0] * scale;
            sp4b4[0].unk04 = sp644.f[1] * scale;
            sp4b4[0].unk08 = sp644.f[2] * scale;
            sp4b4[1].unk00 = sp614.f[0] * scale;
            sp4b4[1].unk04 = sp614.f[1] * scale;
            sp4b4[1].unk08 = sp614.f[2] * scale;
            sp4b4[2].unk00 = sp5fc.f[0] * scale;
            sp4b4[2].unk04 = sp5fc.f[1] * scale;
            sp4b4[2].unk08 = sp5fc.f[2] * scale;
            sp4b4[0].unk0c = sp644.f[0] * 0.1f;
            sp4b4[0].unk10 = sp644.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp614.f[0] * 0.1f;
            sp4b4[1].unk10 = sp614.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp5fc.f[0] * 0.1f;
            sp4b4[2].unk10 = sp5fc.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp58c);
            skyChooseCloudVtxColour(&sp4b4[1], sp57c);
            skyChooseCloudVtxColour(&sp4b4[2], sp574);
            break;

        case 14:
            s1 = 5;
            sp4b4[0].unk00 = sp62c.f[0] * scale;
            sp4b4[0].unk04 = sp62c.f[1] * scale;
            sp4b4[0].unk08 = sp62c.f[2] * scale;
            sp4b4[1].unk00 = sp644.f[0] * scale;
            sp4b4[1].unk04 = sp644.f[1] * scale;
            sp4b4[1].unk08 = sp644.f[2] * scale;
            sp4b4[2].unk00 = sp638.f[0] * scale;
            sp4b4[2].unk04 = sp638.f[1] * scale;
            sp4b4[2].unk08 = sp638.f[2] * scale;
            sp4b4[3].unk00 = sp5f0.f[0] * scale;
            sp4b4[3].unk04 = sp5f0.f[1] * scale;
            sp4b4[3].unk08 = sp5f0.f[2] * scale;
            sp4b4[4].unk00 = sp608.f[0] * scale;
            sp4b4[4].unk04 = sp608.f[1] * scale;
            sp4b4[4].unk08 = sp608.f[2] * scale;
            sp4b4[0].unk0c = sp62c.f[0] * 0.1f;
            sp4b4[0].unk10 = sp62c.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp644.f[0] * 0.1f;
            sp4b4[1].unk10 = sp644.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp638.f[0] * 0.1f;
            sp4b4[2].unk10 = sp638.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp5f0.f[0] * 0.1f;
            sp4b4[3].unk10 = sp5f0.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[4].unk0c = sp608.f[0] * 0.1f;
            sp4b4[4].unk10 = sp608.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp584);
            skyChooseCloudVtxColour(&sp4b4[1], sp58c);
            skyChooseCloudVtxColour(&sp4b4[2], sp588);
            skyChooseCloudVtxColour(&sp4b4[3], sp570);
            skyChooseCloudVtxColour(&sp4b4[4], sp578);
            break;

        case 13:
            s1 = 5;
            sp4b4[0].unk00 = sp644.f[0] * scale;
            sp4b4[0].unk04 = sp644.f[1] * scale;
            sp4b4[0].unk08 = sp644.f[2] * scale;
            sp4b4[1].unk00 = sp638.f[0] * scale;
            sp4b4[1].unk04 = sp638.f[1] * scale;
            sp4b4[1].unk08 = sp638.f[2] * scale;
            sp4b4[2].unk00 = sp620.f[0] * scale;
            sp4b4[2].unk04 = sp620.f[1] * scale;
            sp4b4[2].unk08 = sp620.f[2] * scale;
            sp4b4[3].unk00 = sp608.f[0] * scale;
            sp4b4[3].unk04 = sp608.f[1] * scale;
            sp4b4[3].unk08 = sp608.f[2] * scale;
            sp4b4[4].unk00 = sp5fc.f[0] * scale;
            sp4b4[4].unk04 = sp5fc.f[1] * scale;
            sp4b4[4].unk08 = sp5fc.f[2] * scale;
            sp4b4[0].unk0c = sp644.f[0] * 0.1f;
            sp4b4[0].unk10 = sp644.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp638.f[0] * 0.1f;
            sp4b4[1].unk10 = sp638.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp620.f[0] * 0.1f;
            sp4b4[2].unk10 = sp620.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp608.f[0] * 0.1f;
            sp4b4[3].unk10 = sp608.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[4].unk0c = sp5fc.f[0] * 0.1f;
            sp4b4[4].unk10 = sp5fc.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp58c);
            skyChooseCloudVtxColour(&sp4b4[1], sp588);
            skyChooseCloudVtxColour(&sp4b4[2], sp580);
            skyChooseCloudVtxColour(&sp4b4[3], sp578);
            skyChooseCloudVtxColour(&sp4b4[4], sp574);
            break;

        case 11:
            s1 = 5;
            sp4b4[0].unk00 = sp620.f[0] * scale;
            sp4b4[0].unk04 = sp620.f[1] * scale;
            sp4b4[0].unk08 = sp620.f[2] * scale;
            sp4b4[1].unk00 = sp62c.f[0] * scale;
            sp4b4[1].unk04 = sp62c.f[1] * scale;
            sp4b4[1].unk08 = sp62c.f[2] * scale;
            sp4b4[2].unk00 = sp644.f[0] * scale;
            sp4b4[2].unk04 = sp644.f[1] * scale;
            sp4b4[2].unk08 = sp644.f[2] * scale;
            sp4b4[3].unk00 = sp614.f[0] * scale;
            sp4b4[3].unk04 = sp614.f[1] * scale;
            sp4b4[3].unk08 = sp614.f[2] * scale;
            sp4b4[4].unk00 = sp5f0.f[0] * scale;
            sp4b4[4].unk04 = sp5f0.f[1] * scale;
            sp4b4[4].unk08 = sp5f0.f[2] * scale;
            sp4b4[0].unk0c = sp620.f[0] * 0.1f;
            sp4b4[0].unk10 = sp620.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp62c.f[0] * 0.1f;
            sp4b4[1].unk10 = sp62c.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp644.f[0] * 0.1f;
            sp4b4[2].unk10 = sp644.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp614.f[0] * 0.1f;
            sp4b4[3].unk10 = sp614.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[4].unk0c = sp5f0.f[0] * 0.1f;
            sp4b4[4].unk10 = sp5f0.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp580);
            skyChooseCloudVtxColour(&sp4b4[1], sp584);
            skyChooseCloudVtxColour(&sp4b4[2], sp58c);
            skyChooseCloudVtxColour(&sp4b4[3], sp57c);
            skyChooseCloudVtxColour(&sp4b4[4], sp570);
            break;

        case 7:
            s1 = 5;
            sp4b4[0].unk00 = sp638.f[0] * scale;
            sp4b4[0].unk04 = sp638.f[1] * scale;
            sp4b4[0].unk08 = sp638.f[2] * scale;
            sp4b4[1].unk00 = sp620.f[0] * scale;
            sp4b4[1].unk04 = sp620.f[1] * scale;
            sp4b4[1].unk08 = sp620.f[2] * scale;
            sp4b4[2].unk00 = sp62c.f[0] * scale;
            sp4b4[2].unk04 = sp62c.f[1] * scale;
            sp4b4[2].unk08 = sp62c.f[2] * scale;
            sp4b4[3].unk00 = sp5fc.f[0] * scale;
            sp4b4[3].unk04 = sp5fc.f[1] * scale;
            sp4b4[3].unk08 = sp5fc.f[2] * scale;
            sp4b4[4].unk00 = sp614.f[0] * scale;
            sp4b4[4].unk04 = sp614.f[1] * scale;
            sp4b4[4].unk08 = sp614.f[2] * scale;
            sp4b4[0].unk0c = sp638.f[0] * 0.1f;
            sp4b4[0].unk10 = sp638.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[1].unk0c = sp620.f[0] * 0.1f;
            sp4b4[1].unk10 = sp620.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[2].unk0c = sp62c.f[0] * 0.1f;
            sp4b4[2].unk10 = sp62c.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[3].unk0c = sp5fc.f[0] * 0.1f;
            sp4b4[3].unk10 = sp5fc.f[2] * 0.1f + g_SkyCloudOffset;
            sp4b4[4].unk0c = sp614.f[0] * 0.1f;
            sp4b4[4].unk10 = sp614.f[2] * 0.1f + g_SkyCloudOffset;

            skyChooseCloudVtxColour(&sp4b4[0], sp588);
            skyChooseCloudVtxColour(&sp4b4[1], sp580);
            skyChooseCloudVtxColour(&sp4b4[2], sp584);
            skyChooseCloudVtxColour(&sp4b4[3], sp574);
            skyChooseCloudVtxColour(&sp4b4[4], sp57c);
            break;

        default:
            return gdl;
    }

    gDPPipeSync(gdl++);

    texSelect(&gdl, &skywaterimages[fogGetCurrentEnvironmentp()->SkyImageId], 1, 0, 2);

    if (1)
        ;

    gDPSetEnvColor(gdl++, fogGetCurrentEnvironmentp()->Red, fogGetCurrentEnvironmentp()->Green,
                   fogGetCurrentEnvironmentp()->Blue, 0xff);
    gDPSetCombineLERP(gdl++, SHADE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, SHADE, SHADE, ENVIRONMENT, TEXEL0,
                      ENVIRONMENT, 0, 0, 0, SHADE);

    {
        Mtxf sp1ec;
        Mtxf sp1ac;
        SkyRelated38 sp94[5];
        s32 i;
        s32 stack[2];

        matrix_4x4_multiply(currentPlayerGetProjectionMatrixF(), camGetWorldToScreenMtxf(), &sp1ec);
        guScaleF(dword_CODE_bss_80079E98.m, 1.0f / scale, 1.0f / scale, 1.0f / scale);
        matrix_4x4_multiply(&sp1ec, &dword_CODE_bss_80079E98, &sp1ac);

        for (i = 0; i < s1; i++) {
            sub_GAME_7F097388(&sp4b4[i], &sp1ac, 130, 65535.0f, 65535.0f, &sp94[i]);

            sp94[i].unk28 = skyClamp(sp94[i].unk28, getPlayer_c_screenleft() * 4.0f,
                                     (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f);
            sp94[i].unk2c = skyClamp(sp94[i].unk2c, getPlayer_c_screentop() * 4.0f,
                                     (getPlayer_c_screentop() + getPlayer_c_screenheight()) * 4.0f - 1.0f);
        }

#if PORTSKY == 0
        if (s1 == 4) {
            if (((sp538 << 3) | (sp534 << 2) | (sp530 << 1) | sp52c) == 12) {
                if (sp548 < sp54c) {
                    if (sp94[3].unk2c >= sp94[1].unk2c + 4.0f) {
                        sp94[0].unk28 = getPlayer_c_screenleft() * 4.0f;
                        sp94[0].unk2c = getPlayer_c_screentop() * 4.0f;
                        sp94[1].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;
                        sp94[1].unk2c = getPlayer_c_screentop() * 4.0f;
                        sp94[2].unk28 = getPlayer_c_screenleft() * 4.0f;
                        sp94[3].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;

                        gdl = skyRenderFull(gdl, &sp94[0], &sp94[1], &sp94[2], &sp94[3], 130.0f);
                    } else {
                        gdl = skyRenderTri(gdl, &sp94[0], &sp94[1], &sp94[2], 130.0f, TRUE);
                    }
                } else if (sp94[2].unk2c >= sp94[0].unk2c + 4.0f) {
                    sp94[0].unk28 = getPlayer_c_screenleft() * 4.0f;
                    sp94[0].unk2c = getPlayer_c_screentop() * 4.0f;
                    sp94[1].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;
                    sp94[1].unk2c = getPlayer_c_screentop() * 4.0f;
                    sp94[2].unk28 = getPlayer_c_screenleft() * 4.0f;
                    sp94[3].unk28 = (getPlayer_c_screenleft() + getPlayer_c_screenwidth()) * 4.0f - 1.0f;

                    gdl = skyRenderFull(gdl, &sp94[1], &sp94[0], &sp94[3], &sp94[2], 130.0f);
                } else {
                    gdl = skyRenderTri(gdl, &sp94[1], &sp94[0], &sp94[3], 130.0f, TRUE);
                }
            } else {
                gdl = skyRenderTri(gdl, &sp94[0], &sp94[1], &sp94[3], 130.0f, TRUE);
                gdl = skyRenderTri(gdl, &sp94[3], &sp94[2], &sp94[0], 130.0f, TRUE);
            }
        } else if (s1 == 5) {
            gdl = skyRenderTri(gdl, &sp94[0], &sp94[1], &sp94[2], 130.0f, TRUE);
            gdl = skyRenderTri(gdl, &sp94[0], &sp94[2], &sp94[3], 130.0f, TRUE);
            gdl = skyRenderTri(gdl, &sp94[0], &sp94[3], &sp94[4], 130.0f, TRUE);
        } else if (s1 == 3) {
            gdl = skyRenderTri(gdl, &sp94[0], &sp94[1], &sp94[2], 130.0f, TRUE);
        }
    }
#else
        gdl = skyDrawPolygon(gdl, sp4b4, s1, scale, SKY_CLOUDS);
    }
#endif

    return gdl;
}
#endif
