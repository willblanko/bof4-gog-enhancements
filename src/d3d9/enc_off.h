/* No random encounters on the field, GOG PC build of BOF4 (BOF4.exe, 1.0 GOG).
 *
 * The field's per-step encounter check is sub_637A20 (PC 0x637A20). It is cdecl with one
 * argument (the field object, pushed by the caller) and is called from exactly three
 * places (IDA xrefs, checked on BOF4_analise / investigacao_combos):
 *   0x6341DB  call sub_637A20   in sub_634000   (add esp,4 after)
 *   0x64201B  call sub_637A20   in sub_641AE0   (add esp,4 after)
 *   0x43E6AC  call sub_637A20   in sub_43E210   (add esp,4 after)
 * Each E8 rel32 is replaced by a call to enc_stub. When the option is on, the stub returns 0
 * at once (eax = 0, no stack change, no register change), so sub_637A20 never runs: the step
 * budget (0xBB65A0) is not decremented and no encounter flag (0xBAB54E/0xBAB54F) is set.
 * When the option is off, the stub jumps to the original sub_637A20 with the same stack, so
 * the game behaves as before.
 *
 * Not touched: the battle transition (0xBAB408 = 5, sub_521680) and the script battle
 * commands (sub_4D6EE0 / sub_4D6FA0), so scripted and boss battles keep working.
 * Premises (checked at run time before writing, nothing is written if they differ):
 *   - module base 0x400000 (PE ImageBase);
 *   - each of the three sites holds E8 whose target is 0x637A20.
 */
#ifndef ENC_OFF_H
#define ENC_OFF_H

#define ENC_MODULE_BASE   0x400000u
#define ENC_FN            0x637A20u   /* sub_637A20: per-step encounter check */
#define ENC_SITE_A        0x6341DBu   /* call in sub_634000 */
#define ENC_SITE_B        0x64201Bu   /* call in sub_641AE0 */
#define ENC_SITE_C        0x43E6ACu   /* call in sub_43E210 */

/* 1 = no random encounters; 0 = the game's own encounters (the default). F8 toggles it. */
static volatile int g_enc_off = 0;
static int g_enc_installed = 0;

/* Naked stub: entered by the patched call, [esp] is the return address and [esp+4] is the
 * field object. It touches only eax, and tail-jumps to the original so every other register
 * and the stack are exactly as the original call left them. */
__declspec(naked) static void enc_stub(void)
{
    __asm {
        cmp  g_enc_off, 0
        je   enc_orig                /* option off: run the original check */
        xor  eax, eax                /* option on: no encounter, return 0 */
        ret                          /* cdecl caller pops the argument */
    enc_orig:
        mov  eax, ENC_FN
        jmp  eax
    }
}

/* True when the site is still the game's own "call sub_637A20" (E8 with the right target). */
static int enc_site_is_game_call(unsigned int site)
{
    const unsigned char *p = (const unsigned char *)(UINT_PTR)site;
    unsigned int target;
    if (IsBadReadPtr(p, 5)) return 0;
    if (p[0] != 0xE8) return 0;
    target = site + 5 + (unsigned int)p[1] + ((unsigned int)p[2] << 8)
             + ((unsigned int)p[3] << 16) + ((unsigned int)p[4] << 24);
    return target == ENC_FN;
}

static int enc_patch_site(unsigned int site, void (*log)(const char *))
{
    unsigned char *p = (unsigned char *)(UINT_PTR)site;
    unsigned int rel = (unsigned int)(UINT_PTR)enc_stub - (site + 5);
    DWORD old;
    int i;
    char s[160];
    unsigned char want[5];

    want[0] = 0xE8;
    for (i = 0; i < 4; i++) want[1 + i] = (unsigned char)(rel >> (8 * i));
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    for (i = 0; i < 5; i++) p[i] = want[i];
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    wsprintfA(s, "no encounters: site %08X redirected to enc_stub", site);
    if (log) log(s);
    return 1;
}

/* Install once. All three sites are checked before any is written. */
static void enc_install(void (*log)(const char *))
{
    char s[160];
    if (g_enc_installed) return;
    g_enc_installed = 1;
    if ((unsigned int)(UINT_PTR)GetModuleHandleA(NULL) != ENC_MODULE_BASE) {
        wsprintfA(s, "no encounters: module base is not %08X; not installed", ENC_MODULE_BASE);
        if (log) log(s);
        return;
    }
    if (!enc_site_is_game_call(ENC_SITE_A) || !enc_site_is_game_call(ENC_SITE_B)
        || !enc_site_is_game_call(ENC_SITE_C)) {
        if (log) log("no encounters: expected sub_637A20 calls not found; not installed");
        return;
    }
    enc_patch_site(ENC_SITE_A, log);
    enc_patch_site(ENC_SITE_B, log);
    enc_patch_site(ENC_SITE_C, log);
}

#endif
