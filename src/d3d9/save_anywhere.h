/* Save anywhere for the GOG PC build of BOF4 (BOF4.exe, 1.0 GOG, SHA256 F925075D...).
 *
 * Only the two call sites of the camp menu's AREA_KIND check are redirected:
 *   0x675A2D  call sub_639300   (confirm of camp command 6 "Save"), ret 0x675A32
 *   0x67D07A  call sub_639300   (draw of camp command 6), ret 0x67D07F
 * Each E8 rel32 is replaced by a call to sa_stub, which calls the original
 * sub_639300 with the same argument and returns 1 (Save enabled) when the
 * original result is not 1. All other callers of sub_639300 keep their original
 * call, so the rest of the game is untouched.
 *
 * Premises (checked statically on this EXE, not at run time):
 *   - the module base is 0x400000 (PE ImageBase);
 *   - both sites contain E8 rel32 pointing to 0x639300, followed by
 *     "add esp,4" (cdecl, one 16-bit argument on the stack) and "cmp eax,1";
 *   - sub_639300 takes its argument at [esp+4] and returns in EAX.
 * The install checks these bytes before writing anything; if they differ, nothing
 * is patched.
 *
 * Save path after the choice (static, confirmed in IDA on BOF4_analise.i64):
 *   sub_675B30 case 6 -> sub_520290(2) -> sub_691D24(2, sub_520DA0) -> screen 2
 *   -> sub_52A5E0 -> sub_52A760 -> sub_52A800 (builds the 0x1880-byte .DAT image)
 *   -> writes "SAV\\%02d.DAT" with index (a1+22)+1.
 * Nothing in that path is changed here.
 */
#ifndef SAVE_ANYWHERE_H
#define SAVE_ANYWHERE_H

#define SA_MODULE_BASE      0x400000u
#define SA_AREA_KIND        0x639300u   /* sub_639300: returns 1 (world map), 2 or 0 */
#define SA_SITE_CONFIRM     0x675A2Du   /* call in the confirm handler, return 0x675A32 */
#define SA_SITE_DRAW        0x67D07Au   /* call in the draw handler, return 0x67D07F */

/* 1 = enable the redirect (the default); 0 = leave the game as it is. */
static volatile int g_sa_enabled = 1;
static int g_sa_installed = 0;

/* Naked stub: entered by the patched call, so [esp] is the return address and
 * [esp+4] is the 16-bit map argument. Returns to the caller, which cleans up. */
__declspec(naked) static void sa_stub(void)
{
    __asm {
        push dword ptr [esp+4]       /* copy the map argument for sub_639300 */
        mov  eax, SA_AREA_KIND
        call eax
        add  esp, 4
        cmp  eax, 1
        je   sa_done                 /* original answer 1: keep it */
        cmp  g_sa_enabled, 0
        je   sa_done                 /* feature off: return the original answer */
        mov  eax, 1                  /* Save enabled */
    sa_done:
        ret                          /* no immediate: cdecl caller pops the argument */
    }
}

/* True when the site is the game's own "call sub_639300; add esp,4; cmp eax,1". */
static int sa_site_is_game_call(unsigned int site)
{
    const unsigned char *p = (const unsigned char *)(UINT_PTR)site;
    unsigned int target;
    if (IsBadReadPtr(p, 11)) return 0;
    if (p[0] != 0xE8) return 0;
    target = site + 5 + (unsigned int)p[1] + ((unsigned int)p[2] << 8)
             + ((unsigned int)p[3] << 16) + ((unsigned int)p[4] << 24);
    return target == SA_AREA_KIND
        && p[5] == 0x83 && p[6] == 0xC4 && p[7] == 0x04   /* add esp,4 */
        && p[8] == 0x83 && p[9] == 0xF8 && p[10] == 0x01; /* cmp eax,1 */
}

/* Patch one site, only when it is still the game's own call (checked here too).
 * Writes the 5 bytes of the call and nothing else. */
static int sa_patch_site(unsigned int site, void (*log)(const char *))
{
    unsigned char *p = (unsigned char *)(UINT_PTR)site;
    unsigned char want[5];
    unsigned int rel_target = (unsigned int)(UINT_PTR)sa_stub - (site + 5);
    DWORD old;
    int i;
    char s[160];

    if (!sa_site_is_game_call(site)) {
        wsprintfA(s, "save anywhere: site %08X does not hold the expected call", site);
        if (log) log(s);
        return 0;
    }

    want[0] = 0xE8;
    for (i = 0; i < 4; i++) want[1 + i] = (unsigned char)(rel_target >> (8 * i));
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    for (i = 0; i < 5; i++) p[i] = want[i];
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    wsprintfA(s, "save anywhere: site %08X redirected to sa_stub", site);
    if (log) log(s);
    return 1;
}

/* Install once. Safe to call repeatedly. Only runs if the EXE is the expected
 * module at 0x400000 and the two sites hold the expected calls. */
static void sa_install(void (*log)(const char *))
{
    char s[160];
    if (g_sa_installed) return;
    g_sa_installed = 1;
    if ((unsigned int)(UINT_PTR)GetModuleHandleA(NULL) != SA_MODULE_BASE) {
        wsprintfA(s, "save anywhere: module base is not %08X; not installed", SA_MODULE_BASE);
        if (log) log(s);
        return;
    }
    /* Both sites are checked before either is written. */
    if (!sa_site_is_game_call(SA_SITE_CONFIRM) || !sa_site_is_game_call(SA_SITE_DRAW)) {
        if (log) log("save anywhere: expected sub_639300 calls not found; not installed");
        return;
    }
    sa_patch_site(SA_SITE_CONFIRM, log);
    sa_patch_site(SA_SITE_DRAW, log);
}

#endif
