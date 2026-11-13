/* Walk with the left analog stick at any angle, GOG PC build of BOF4 (BOF4.exe, 1.0 GOG).
 *
 * The field walks on eight directions. The game turns the held d-pad nibble into a screen
 * direction 0..7 (sub_6397E0), then adds the camera yaw rounded to 45 degrees (sub_66F1C0).
 * The recomp instead takes the stick's exact angle and the exact yaw, and picks the nearest
 * world direction. This header does the same on the PC:
 *   world = round(stick_angle/45 + yaw/0x200) & 7,   screen = (world - ((yaw+0x100)>>9)) & 7
 *
 * Patched call sites (IDA, checked at run time before writing; E8 rel32 only):
 *   0x63333A  call sub_6397E0   in sub_633230 (the leader's first step)  -> wk_stub_screen
 *   0x633A51  call sub_6397E0   in sub_633970 (following steps)          -> wk_stub_screen
 *   0x635B56  call sub_635E70   in sub_635AF0 (copies the pad to obj+0xF6) -> wk_stub_pad
 *
 * wk_stub_pad runs the original copy, then, for the leader only, if the stick is tilted and
 * no d-pad nibble is held, it sets the nibble of the screen direction in obj+0xF6, so the
 * state machine sees a press. wk_stub_screen returns the computed screen direction for the
 * leader; for anyone else, or when the stick is idle, it tail-jumps to the original.
 * The direction table (screen 0..7 -> pad nibble) is the one from sub_639C80 / sub_6397E0.
 *
 * F6 toggles the feature (default on). Only the field (game mode 2) is affected.
 */
#ifndef WALK_STICK_H
#define WALK_STICK_H

#define WK_MODULE_BASE   0x400000u
#define WK_SCREEN_DIR    0x6397E0u   /* sub_6397E0(bits, obj) -> screen direction 0..7 */
#define WK_PAD_COPY      0x635E70u   /* sub_635E70(obj): copies the pad to obj+0xF6 */
#define WK_SITE_A        0x63333Au
#define WK_SITE_B        0x633A51u
#define WK_SITE_C        0x635B56u
#define WK_LEADER_ID     0xBB6598u   /* u8: id of the controlled object */
#define WK_YAW           0xBAD7C6u   /* u16: camera yaw, 0x1000 = full turn */
#define WK_OBJ_ID        0x01        /* u8 in the field object */
#define WK_OBJ_PAD       0xF6        /* u16 held pad of the field object */
#define WK_OBJ_SUB       0x05        /* u8 substate: 0 standing, 1 turning, 2 stepping */
#define WK_OBJ_TURN      0x08        /* u8 game frames left before the first step */

static volatile int g_wk_on = 1;      /* F6 toggles */
static volatile int g_wk_active = 0;  /* the stick is tilted this frame (field only) */
static volatile int g_wk_s = 0;       /* chosen screen direction 0..7 */
static int g_wk_installed = 0;
static int g_wk_log_n = 0;

/* Pad nibble of each screen direction 0..7 (up, up-right, right, ...). */
static const unsigned char WK_BITS[8] = { 0x10, 0x30, 0x20, 0x60, 0x40, 0xC0, 0x80, 0x90 };

/* Per frame, from the game's own pad state and the stick. Uses g_xi_get (from stick_frame). */
static void walk_frame(void)
{
    XI_STATE st;
    double x, y, theta, v;
    unsigned short yaw;
    int w, yr, s, n;
    g_wk_active = 0;
    if (!g_wk_on) return;
    if ((UINT_PTR)GetModuleHandleA(NULL) != WK_MODULE_BASE) return;
    if (!g_xi_get || g_xi_get(0, &st) != 0) return;
    if (*(const unsigned short *)MM_GAME_MODE != 2) return;
    x = (double)st.gp.sLX;   /* right is +x */
    y = (double)st.gp.sLY;   /* up is +y */
    if (x * x + y * y < (double)XI_DEAD * (double)XI_DEAD) return;
    /* clockwise from up, in degrees: atan2(x, y) */
    theta = fl_atan2(x, y) * 180.0 / 3.14159265358979;
    yaw = *(const unsigned short *)WK_YAW;
    v = theta / 45.0 + (double)yaw / 512.0;
    n = (int)(v + 0.5 + 64.0) - 64;            /* round, valid for negative values */
    w = n & 7;                                  /* world direction */
    yr = (int)(((unsigned int)yaw + 0x100u) >> 9);
    s = (w - yr) & 7;                           /* what sub_66F1C0 will add back */
    g_wk_s = s;
    g_wk_active = 1;
    if ((g_wk_log_n++ % 60) == 0) {
        char b[96];
        wsprintfA(b, "walk: stick %.0f deg, yaw %04X, world %d, screen %d", theta, (unsigned)yaw, w, s);
        logln(b);
    }
}

/* Is obj the leader, and does the stick decide this step? Returns the screen direction or -1. */
int __cdecl wk_s2(unsigned int obj)
{
    if (!g_wk_active) return -1;
    if (*(const unsigned char *)(UINT_PTR)(obj + WK_OBJ_ID) != *(const unsigned char *)WK_LEADER_ID) return -1;
    return g_wk_s;
}

/* After the pad copy: put the stick direction in the leader's held nibble if no d-pad is held. */
void __cdecl wk_pad_inject(unsigned int obj)
{
    unsigned char *p;
    unsigned int pad;
    if (!g_wk_active) return;
    if (*(const unsigned char *)(UINT_PTR)(obj + WK_OBJ_ID) != *(const unsigned char *)WK_LEADER_ID) return;
    /* Option A: the leader does not wait the three turn frames when the stick decides.
     * Same as movement.odin's instant start: turning (obj+5 == 1) with counter obj+8 != 0 -> 0. */
    if (*(unsigned char *)(UINT_PTR)(obj + WK_OBJ_SUB) == 1 && *(unsigned char *)(UINT_PTR)(obj + WK_OBJ_TURN) != 0)
        *(unsigned char *)(UINT_PTR)(obj + WK_OBJ_TURN) = 0;
    p = (unsigned char *)(UINT_PTR)(obj + WK_OBJ_PAD);
    pad = (unsigned int)p[0] | ((unsigned int)p[1] << 8);
    if (pad & 0xF0u) return;                    /* a d-pad press wins */
    pad |= WK_BITS[g_wk_s & 7];
    p[0] = (unsigned char)(pad & 0xFF);
    p[1] = (unsigned char)((pad >> 8) & 0xFF);
}

/* Stack on entry: [esp] = return address, [esp+4] = bits, [esp+8] = object (cdecl, the caller
 * pops both after the call). Returns the screen direction for the leader with the stick. */
__declspec(naked) static void wk_stub_screen(void)
{
    __asm {
        push ecx
        push edx
        push dword ptr [esp+16]      /* the object argument */
        call wk_s2
        add  esp, 4
        pop  edx
        pop  ecx
        cmp  eax, -1
        je   wk_orig
        ret                          /* eax = screen direction; caller pops the arguments */
    wk_orig:
        mov  eax, WK_SCREEN_DIR
        jmp  eax                     /* original, same stack */
    }
}

/* Stack on entry: [esp] = return address, [esp+4] = object (cdecl, the caller pops it). */
__declspec(naked) static void wk_stub_pad(void)
{
    __asm {
        mov  eax, [esp+4]            /* object */
        push eax
        mov  eax, WK_PAD_COPY
        call eax                     /* original copy, cdecl */
        add  esp, 4
        push ecx
        push edx
        push dword ptr [esp+12]      /* the object argument */
        call wk_pad_inject
        add  esp, 4
        pop  edx
        pop  ecx
        ret                          /* the caller pops the object */
    }
}

/* True when the site is the game's own call to target (E8 rel32). */
static int wk_site_is(unsigned int site, unsigned int target)
{
    const unsigned char *p = (const unsigned char *)(UINT_PTR)site;
    unsigned int t;
    if (IsBadReadPtr(p, 5)) return 0;
    if (p[0] != 0xE8) return 0;
    t = site + 5 + (unsigned int)p[1] + ((unsigned int)p[2] << 8)
        + ((unsigned int)p[3] << 16) + ((unsigned int)p[4] << 24);
    return t == target;
}

static void wk_patch_site(unsigned int site, void *stub, void (*log)(const char *))
{
    unsigned char *p = (unsigned char *)(UINT_PTR)site;
    unsigned int rel = (unsigned int)(UINT_PTR)stub - (site + 5);
    DWORD old;
    int i;
    char s[120];
    unsigned char want[5];
    want[0] = 0xE8;
    for (i = 0; i < 4; i++) want[1 + i] = (unsigned char)(rel >> (8 * i));
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    for (i = 0; i < 5; i++) p[i] = want[i];
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    wsprintfA(s, "walk stick: site %08X redirected", site);
    if (log) log(s);
}

/* Install once. All three sites are checked before any is written. */
static void walk_install(void (*log)(const char *))
{
    if (g_wk_installed) return;
    g_wk_installed = 1;
    if ((UINT_PTR)GetModuleHandleA(NULL) != WK_MODULE_BASE) {
        if (log) log("walk stick: module base is not 00400000; not installed");
        return;
    }
    if (!wk_site_is(WK_SITE_A, WK_SCREEN_DIR) || !wk_site_is(WK_SITE_B, WK_SCREEN_DIR)
        || !wk_site_is(WK_SITE_C, WK_PAD_COPY)) {
        if (log) log("walk stick: expected calls not found; not installed");
        return;
    }
    wk_patch_site(WK_SITE_A, (void *)wk_stub_screen, log);
    wk_patch_site(WK_SITE_B, (void *)wk_stub_screen, log);
    wk_patch_site(WK_SITE_C, (void *)wk_stub_pad, log);
}

#endif
