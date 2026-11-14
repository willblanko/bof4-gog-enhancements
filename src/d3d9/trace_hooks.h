/* Log-only trace of the candidate Setting screen routines (GOG BOF4.exe, image base
 * 0x400000). It changes no behaviour: each patched call runs the original routine
 * with the same stack and registers, and only writes a log line first.
 *
 * Traced routines (static candidates, see docs/reference/memory-map.md):
 *   0x677480  UI task of the Setting screen (called from the dispatcher 0x68E733)
 *   0x523D20  reads and writes the option dwords (save/restore?)
 *   0x527FA0  Setting screen tick, jump table (called from 0x527B63)
 *   0x527A50  option row helper (called from 0x52798C and 0x5283EE)
 *   0x501E10  writes the option dwords (called from 0x50201A)
 *
 * Each log line gives the routine name, the return address (the caller), and the
 * current text speed (0xB55C28) and vibration (0xB55C2E) bytes. Each routine logs
 * at most TR_MAX lines, so a loop cannot flood the log.
 *
 * Patch: each site is a call (E8 rel32) to the routine; it is replaced by a call
 * to the trace stub for that routine. The install checks each site first and
 * writes nothing if any site differs.
 */
#ifndef TRACE_HOOKS_H
#define TRACE_HOOKS_H

#define TR_MODULE_BASE 0x400000u
#define TR_MAX         300

static void (*g_tr_log)(const char *) = 0;
static int g_tr_installed = 0;
static int g_tr_count[5];   /* per routine, index 0..4 */

static void tr_log_line(int id, unsigned int ret)
{
    static const char *names[5] = { "677480", "523D20", "527FA0", "527A50", "501E10" };
    static const unsigned int sites[5] = { 0x677480u, 0x523D20u, 0x527FA0u, 0x527A50u, 0x501E10u };
    const unsigned char *txt = (const unsigned char *)(UINT_PTR)0xB55C28u;
    const unsigned char *vib = (const unsigned char *)(UINT_PTR)0xB55C2Eu;
    char s[200];
    if (!g_tr_log || g_tr_count[id] >= TR_MAX) return;
    g_tr_count[id]++;
    (void)sites;
    wsprintfA(s, "trace %s from %08X: text=%u vib=%u (#%d)",
              names[id], ret, (unsigned int)txt[0], (unsigned int)vib[0], g_tr_count[id]);
    g_tr_log(s);
}

/* One stub per routine. Entered by the patched call: [esp] is the return address.
 * Saves the registers, logs, restores them, then jumps to the original routine
 * with the stack unchanged (push imm; ret), so the routine sees its own arguments
 * and returns straight to the caller. */
__declspec(naked) static void tr_stub_677480(void)
{
    __asm {
        pushad
        push dword ptr [esp+32]
        push 0
        call tr_log_line
        add  esp, 8
        popad
        push 0x677480
        ret
    }
}

__declspec(naked) static void tr_stub_523D20(void)
{
    __asm {
        pushad
        push dword ptr [esp+32]
        push 1
        call tr_log_line
        add  esp, 8
        popad
        push 0x523D20
        ret
    }
}

__declspec(naked) static void tr_stub_527FA0(void)
{
    __asm {
        pushad
        push dword ptr [esp+32]
        push 2
        call tr_log_line
        add  esp, 8
        popad
        push 0x527FA0
        ret
    }
}

__declspec(naked) static void tr_stub_527A50(void)
{
    __asm {
        pushad
        push dword ptr [esp+32]
        push 3
        call tr_log_line
        add  esp, 8
        popad
        push 0x527A50
        ret
    }
}

__declspec(naked) static void tr_stub_501E10(void)
{
    __asm {
        pushad
        push dword ptr [esp+32]
        push 4
        call tr_log_line
        add  esp, 8
        popad
        push 0x501E10
        ret
    }
}

/* A patched site: where it is, the routine it calls, and its trace stub. */
typedef struct { unsigned int site; unsigned int target; void (*stub)(void); int id; } TrSite;

static TrSite g_tr_sites[] = {
    { 0x68E733u, 0x677480u, tr_stub_677480, 0 },
    { 0x4C79E3u, 0x523D20u, tr_stub_523D20, 1 },
    { 0x60BB44u, 0x523D20u, tr_stub_523D20, 1 },
    { 0x654367u, 0x523D20u, tr_stub_523D20, 1 },
    { 0x6551CCu, 0x523D20u, tr_stub_523D20, 1 },
    { 0x65539Du, 0x523D20u, tr_stub_523D20, 1 },
    { 0x527B63u, 0x527FA0u, tr_stub_527FA0, 2 },
    { 0x52798Cu, 0x527A50u, tr_stub_527A50, 3 },
    { 0x5283EEu, 0x527A50u, tr_stub_527A50, 3 },
    { 0x50201Au, 0x501E10u, tr_stub_501E10, 4 },
};

/* True when the site is a call (E8) to the expected routine. */
static int tr_site_ok(const TrSite *t)
{
    const unsigned char *p = (const unsigned char *)(UINT_PTR)t->site;
    unsigned int target;
    if (IsBadReadPtr(p, 5)) return 0;
    if (p[0] != 0xE8) return 0;
    target = t->site + 5 + (unsigned int)p[1] + ((unsigned int)p[2] << 8)
             + ((unsigned int)p[3] << 16) + ((unsigned int)p[4] << 24);
    return target == t->target;
}

static void tr_patch_site(const TrSite *t)
{
    unsigned char *p = (unsigned char *)(UINT_PTR)t->site;
    unsigned int rel = (unsigned int)(UINT_PTR)t->stub - (t->site + 5);
    unsigned char want[5];
    DWORD old;
    int i;
    want[0] = 0xE8;
    for (i = 0; i < 4; i++) want[1 + i] = (unsigned char)(rel >> (8 * i));
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    for (i = 0; i < 5; i++) p[i] = want[i];
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}

/* Install once. Checks every site first; writes nothing if one differs. */
static void tr_install(void (*log)(const char *))
{
    int i, n = (int)(sizeof(g_tr_sites) / sizeof(g_tr_sites[0]));
    char s[160];
    if (g_tr_installed) return;
    g_tr_installed = 1;
    g_tr_log = log;
    if ((unsigned int)(UINT_PTR)GetModuleHandleA(NULL) != TR_MODULE_BASE) {
        wsprintfA(s, "trace: module base is not %08X; not installed", TR_MODULE_BASE);
        if (log) log(s);
        return;
    }
    for (i = 0; i < n; i++) {
        if (!tr_site_ok(&g_tr_sites[i])) {
            wsprintfA(s, "trace: site %08X does not hold the expected call; not installed",
                      g_tr_sites[i].site);
            if (log) log(s);
            return;
        }
    }
    for (i = 0; i < n; i++) tr_patch_site(&g_tr_sites[i]);
    if (log) log("trace: 10 call sites redirected (Setting screen candidates)");
}

#endif
