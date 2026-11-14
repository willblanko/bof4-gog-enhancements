/* Fade speed for the GOG PC build of BOF4 (BOF4.exe, 1.0 GOG, image base 0x400000).
 *
 * Screen fades (to black and back) run in one loop, sub_5202D0, which calls the
 * per-frame step sub_520360 once a frame with the fade speed as a 16-bit argument.
 * Only that one call site is redirected (0x5202FD, E8 rel32 to 0x520360):
 *
 *   normal fades   the speed is limited to +/- FS_MAX (0x800), so a fade that the
 *                  game runs in 4 frames takes 16 frames: the screen darkens and
 *                  clears more gently;
 *   door fades     while the leader goes through a door (sub_6368A0 sets the
 *                  flag byte 0xBAB428 to 4), the speed is raised to at least
 *                  +/- FS_DOOR (0x2000): the fade is the fastest the game has.
 *
 * The map is still loaded from disc in the middle, so a door is faster, not
 * instant. F5 turns the whole feature off (the game's own speeds come back).
 *
 * Premises checked on this EXE (static, IDA, BOF4_copy.exe):
 *   - 0x5202FD holds E8 5E 00 00 00 (call 0x520360), inside sub_5202D0;
 *   - 0x520360 starts with push esi (0x56);
 *   - the speed is the second cdecl argument, at [esp+8] inside the stub, and
 *     the game only reads its low word;
 *   - 0xBAB428 is the transition submode; 4 is set by the door code (0x676CE6)
 *     and is still 4 during the fade in (the reset to 0 comes after it).
 * Nothing is written unless all of them match.
 */
#ifndef FADE_SPEED_H
#define FADE_SPEED_H

#define FS_MODULE_BASE   0x400000u
#define FS_SITE          0x5202FDu   /* call sub_520360 in the fade loop sub_5202D0 */
#define FS_STEP          0x520360u   /* sub_520360: one fade step, returns 1 when done */
#define FS_SUBMODE       0xBAB428u   /* transition submode byte; 4 = door transition */
#define FS_MAX           0x800       /* normal fades: largest speed magnitude */
#define FS_DOOR          0x2000      /* door fades: smallest speed magnitude */

/* 1 = fades limited and door fades quickened (the default); 0 = the game's own. */
static volatile int g_fs_enabled = 1;
static int g_fs_installed = 0;

/* Naked stub, entered by the patched call: [esp] is the return address, [esp+4]
 * the counter pointer, [esp+8] the speed (low word), [esp+0xC] the colour and
 * [esp+0x10] the layer. It changes the speed in place, then jumps to the step
 * function, so the step returns to the loop and the loop cleans up the stack. */
__declspec(naked) static void fs_stub(void)
{
    __asm {
        cmp  g_fs_enabled, 0
        je   fs_go                  /* feature off: the game's own speed */
        movsx eax, word ptr [esp+8]
        mov  edx, FS_SUBMODE
        cmp  byte ptr [edx], 4
        je   fs_door
        cmp  eax, FS_MAX
        jg   fs_hi
        cmp  eax, -FS_MAX
        jl   fs_lo
        jmp  fs_go                  /* within the cap: keep it */
    fs_hi:
        mov  eax, FS_MAX
        jmp  fs_store
    fs_lo:
        mov  eax, -FS_MAX
        jmp  fs_store
    fs_door:
        test eax, eax
        jg   fs_dpos
        jz   fs_go                  /* zero speed: leave it */
        cmp  eax, -FS_DOOR
        jle  fs_go                  /* already at least as fast */
        mov  eax, -FS_DOOR
        jmp  fs_store
    fs_dpos:
        cmp  eax, FS_DOOR
        jge  fs_go
        mov  eax, FS_DOOR
    fs_store:
        mov  word ptr [esp+8], ax
    fs_go:
        mov  eax, FS_STEP
        jmp  eax                    /* tail jump: the caller still pops the arguments */
    }
}

/* True when the site is the game's own "call sub_520360" and the step starts
 * with push esi. */
static int fs_site_is_game_call(unsigned int site)
{
    const unsigned char *p = (const unsigned char *)(UINT_PTR)site;
    const unsigned char *q = (const unsigned char *)(UINT_PTR)FS_STEP;
    unsigned int target;
    if (IsBadReadPtr(p, 5) || IsBadReadPtr(q, 1)) return 0;
    if (p[0] != 0xE8) return 0;
    target = site + 5 + (unsigned int)p[1] + ((unsigned int)p[2] << 8)
             + ((unsigned int)p[3] << 16) + ((unsigned int)p[4] << 24);
    return target == FS_STEP && q[0] == 0x56;
}

/* Install once. Safe to call repeatedly. Checks the module base and the call
 * site before writing the five bytes. */
static void fs_install(void (*log)(const char *))
{
    unsigned char *p = (unsigned char *)(UINT_PTR)FS_SITE;
    unsigned int rel_target = (unsigned int)(UINT_PTR)fs_stub - (FS_SITE + 5);
    DWORD old;
    int i;
    char s[160];
    unsigned char want[5];

    if (g_fs_installed) return;
    g_fs_installed = 1;
    if ((unsigned int)(UINT_PTR)GetModuleHandleA(NULL) != FS_MODULE_BASE) {
        wsprintfA(s, "fade speed: module base is not %08X; not installed", FS_MODULE_BASE);
        if (log) log(s);
        return;
    }
    if (!fs_site_is_game_call(FS_SITE)) {
        if (log) log("fade speed: expected call to sub_520360 not found; not installed");
        return;
    }
    want[0] = 0xE8;
    for (i = 0; i < 4; i++) want[1 + i] = (unsigned char)(rel_target >> (8 * i));
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    for (i = 0; i < 5; i++) p[i] = want[i];
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    wsprintfA(s, "fade speed: site %08X redirected to fs_stub", FS_SITE);
    if (log) log(s);
}

#endif
