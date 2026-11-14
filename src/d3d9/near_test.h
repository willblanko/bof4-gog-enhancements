/* Near-plane test (F2 toggles). EXPERIMENT: changes the game's camera near plane
 * 0xBAD7C0 to NT_VALUE every field frame while on, so that scenery between the
 * camera and the party is cut less. The question it answers: does the wall that
 * disappears when the camera turns (docs/features/wall-transparency.md) come from
 * the near-plane cut? If the wall stays visible with the test on, the cut is the
 * cause.
 *
 * Off: the proxy stops writing the value. The game sets its own near plane again
 * when the next map loads (until then the last value stays). */
#ifndef NEAR_TEST_H
#define NEAR_TEST_H

#define NT_NEAR   0xBAD7C0u   /* s32 camera near plane (ZP_NEAR) */
#define NT_VALUE  0x100       /* the test value (the game uses 0x400..0xA00) */
#define NT_EVERY  60          /* field frames between log lines while on */

static volatile int g_nt_on = 0;
static int g_nt_frames = 0;

static void near_test_frame(void)
{
    unsigned short mode;
    char s[160];
    if (!g_nt_on) return;
    if ((UINT_PTR)GetModuleHandleA(NULL) != MM_IMAGE_BASE) return;
    mode = *(const unsigned short *)MM_GAME_MODE;
    if (mode != 2) return;                 /* field only */
    *(int *)NT_NEAR = NT_VALUE;
    if (++g_nt_frames % NT_EVERY == 0) {
        wsprintfA(s, "near test: near plane %d (test value %d)", (int)*(const int *)NT_NEAR, NT_VALUE);
        logln(s);
    }
}

#endif
