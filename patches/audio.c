#include "patches.h"

#if 1
RECOMP_PATCH void musicTrack1Play(s32 track) __attribute__((optnone)) {
    u32 trackSizeBytes;
    struct music_struct_b thing;
    u8* temp_a0;
    void* romAddress;
    u32 t3;
    struct huft hlist;

    if (g_sndBootswitchSound) {
        return;
    }

    if (g_musicXTrack1CurrentTrackNum) {
        musicTrack1Stop();
    }

    g_musicXTrack1CurrentTrackNum = track;

    while (alCSPGetState(g_musicXTrack1SeqPlayer)) {
        // @recomp: Yield the thread so it doesn't stall
        yield_self_1ms();
    }

    romAddress = g_musicDataTable->seqArray[g_musicXTrack1CurrentTrackNum].address;

    if (romAddress < (void*) ROM_MUSIC_START_OFFSET) {
        // Note: recursive call
        musicTrack1Play(M_SHORT_SOLO_DEATH);

        return;
    }

    t3 = ALIGN16_a(g_musicTrackLength[g_musicXTrack1CurrentTrackNum]) + ALIGN16_a(NUM_MUSIC_TRACKS);
    trackSizeBytes = ALIGN16_a(g_musicTrackCompressedLength[g_musicXTrack1CurrentTrackNum]);
    thing.seqData = g_musicXTrack1SeqData;
    temp_a0 = (u8*) ((t3 + (s32) thing.seqData) - trackSizeBytes);

    romCopy(temp_a0, romAddress, trackSizeBytes);
    decompressdata(temp_a0, thing.seqData, &hlist);

    alCSeqNew(&g_musicXTrack1Seq, g_musicXTrack1SeqData);
    alCSPSetSeq(g_musicXTrack1SeqPlayer, &g_musicXTrack1Seq);
    musicTrack1ApplySeqpVol(musicTrack1GetVolume());
    alCSPPlay(g_musicXTrack1SeqPlayer);
}
#endif

#if 1
RECOMP_PATCH void musicTrack2Play(s32 track) __attribute__((optnone)) {
    u32 trackSizeBytes;
    struct music_struct_b thing;
    u8* temp_a0;
    void* romAddress;
    u32 t3;
    struct huft hlist;

    if (g_sndBootswitchSound) {
        return;
    }

    if (g_musicXTrack2CurrentTrackNum) {
        musicTrack2Stop();
    }

    g_musicXTrack2CurrentTrackNum = track;

    while (alCSPGetState(g_musicXTrack2SeqPlayer)) {
        // @recomp: Yield the thread so it doesn't stall
        yield_self_1ms();
    }

    romAddress = g_musicDataTable->seqArray[g_musicXTrack2CurrentTrackNum].address;

    if (romAddress < (void*) ROM_MUSIC_START_OFFSET) {
        // Note: recursive call
        musicTrack2Play(M_SHORT_SOLO_DEATH);

        return;
    }

    t3 = ALIGN16_a(g_musicTrackLength[g_musicXTrack2CurrentTrackNum]) + ALIGN16_a(NUM_MUSIC_TRACKS);
    trackSizeBytes = ALIGN16_a(g_musicTrackCompressedLength[g_musicXTrack2CurrentTrackNum]);
    thing.seqData = g_musicXTrack2SeqData;
    temp_a0 = (u8*) ((t3 + (s32) thing.seqData) - trackSizeBytes);

    romCopy(temp_a0, romAddress, trackSizeBytes);
    decompressdata(temp_a0, thing.seqData, &hlist);

    alCSeqNew(&g_musicXTrack2Seq, g_musicXTrack2SeqData);
    alCSPSetSeq(g_musicXTrack2SeqPlayer, &g_musicXTrack2Seq);
    musicTrack2ApplySeqpVol(musicTrack2GetVolume());
    alCSPPlay(g_musicXTrack2SeqPlayer);
}
#endif

#if 1
RECOMP_PATCH void musicTrack3Play(s32 track) __attribute__((optnone)) {
    u32 trackSizeBytes;
    struct music_struct_b thing;
    u8* temp_a0;
    void* romAddress;
    u32 t3;
    struct huft hlist;

    if (g_sndBootswitchSound) {
        return;
    }

    if (g_musicXTrack3CurrentTrackNum) {
        musicTrack3Stop();
    }

    g_musicXTrack3CurrentTrackNum = track;

    while (alCSPGetState(g_musicXTrack3SeqPlayer)) {
        // @recomp: Yield the thread so it doesn't stall
        yield_self_1ms();
    }

    romAddress = g_musicDataTable->seqArray[g_musicXTrack3CurrentTrackNum].address;

    if (romAddress < (void*) ROM_MUSIC_START_OFFSET) {
        // Note: recursive call
        musicTrack3Play(M_SHORT_SOLO_DEATH);

        return;
    }

    t3 = ALIGN16_a(g_musicTrackLength[g_musicXTrack3CurrentTrackNum]) + ALIGN16_a(NUM_MUSIC_TRACKS);
    trackSizeBytes = ALIGN16_a(g_musicTrackCompressedLength[g_musicXTrack3CurrentTrackNum]);
    thing.seqData = g_musicXTrack3SeqData;
    temp_a0 = (u8*) ((t3 + (s32) thing.seqData) - trackSizeBytes);

    romCopy(temp_a0, romAddress, trackSizeBytes);
    decompressdata(temp_a0, thing.seqData, &hlist);

    alCSeqNew(&g_musicXTrack3Seq, g_musicXTrack3SeqData);
    alCSPSetSeq(g_musicXTrack3SeqPlayer, &g_musicXTrack3Seq);
    musicTrack3ApplySeqpVol(musicTrack3GetVolume());
    alCSPPlay(g_musicXTrack3SeqPlayer);
}
#endif
