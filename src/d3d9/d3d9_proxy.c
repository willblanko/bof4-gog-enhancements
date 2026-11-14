/* d3d9.dll proxy for BOF4 (test build, no CRT).
 * Loaded by the GOG ddraw wrapper from the game folder. Forwards Direct3DCreate9
 * to the real System32 d3d9.dll, hooks IDirect3D9::CreateDevice (vtable 16) and
 * IDirect3DSwapChain9::Present (vtable 3).
 * Overlay: a texture drawn over each frame. Shows the "D3D9 PROXY OK" label, or
 * the Masters panel when F12 turns it on (off by default).
 * Masters panel: data read from the live game memory. The character record of
 * Ryu is found in the process heap (name "Ryu", level 1-99); when several copies
 * exist, the one that changes while the game runs is used.
 * Keys: F12 panel on/off, F10 next page, 1-6 choose a master on the page.
 * (F9 is the game's own "Exit Game" key, so it is not used.)
 * Logs to d3d9_proxy.log in the game folder. Win32 API only (no C runtime). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "master_faces_data.h"   /* FACE_PX, FACE_HAS: the 12 master portraits, 40x48 */
#include "save_anywhere.h"       /* camp menu Save enabled everywhere (two call sites) */
#include "enc_off.h"             /* no random encounters on the field (F8 toggles) */
#include "bonus_icons_data.h"   /* level-up bonus icons: HP, AP, Pwr, Def, Agl, Wis */

typedef void *(WINAPI *PFN_Create9)(UINT);
typedef long (__stdcall *PFN_CreateDevice)(void *, UINT, int, HWND, DWORD, void *, void **);
typedef long (__stdcall *PFN_SCPresent)(void *, const void *, const void *, HWND, const void *, DWORD);
typedef long (__stdcall *PFN_GetBackBufferSC)(void *, UINT, int, void **);
typedef long (__stdcall *PFN_Release)(void *);
typedef long (__stdcall *PFN_GetSwapChain)(void *, UINT, void **);

static PFN_Create9 g_real_create9 = NULL;
static PFN_CreateDevice g_real_createdev = NULL;
static PFN_SCPresent g_real_sc_present = NULL;
static volatile LONG g_vt_d3d_patched = 0;
static volatile LONG g_vt_dev_patched = 0;
static volatile LONG g_sc_patched = 0;
static volatile LONG g_sc_frames = 0;

typedef struct { int Pitch; void *pBits; } OV_LOCKED;
typedef struct { float x, y, z, rhw; DWORD c; float u, v; } OV_V;
typedef long (__stdcall *PFN_CreateTex)(void *, UINT, UINT, UINT, DWORD, int, int, void **, void **);
typedef long (__stdcall *PFN_LockRect)(void *, UINT, void *, const RECT *, DWORD);
typedef long (__stdcall *PFN_UnlockRect)(void *, UINT);
typedef long (__stdcall *PFN_CreateSB)(void *, int, void **);
typedef long (__stdcall *PFN_Call0)(void *);
typedef long (__stdcall *PFN_SetFVF)(void *, DWORD);
typedef long (__stdcall *PFN_SetRS)(void *, DWORD, DWORD);
typedef long (__stdcall *PFN_SetTex)(void *, DWORD, void *);
typedef long (__stdcall *PFN_SetTSS)(void *, DWORD, DWORD, DWORD);
typedef long (__stdcall *PFN_DrawUP)(void *, int, UINT, const void *, UINT);
typedef long (__stdcall *PFN_SetRT)(void *, DWORD, void *);
typedef long (__stdcall *PFN_SetVP)(void *, const void *);
typedef struct { DWORD X, Y, Width, Height; float MinZ, MaxZ; } OV_VP;
typedef long (__stdcall *PFN_GetRS)(void *, DWORD, DWORD *);
typedef long (__stdcall *PFN_SetShader)(void *, void *);

static void *g_dev = NULL;      /* IDirect3DDevice9 from CreateDevice */
static UINT g_w = 0, g_h = 0;   /* back buffer size from presentation params */
static void *g_tex = NULL;      /* overlay texture (A8R8G8B8) */
static int g_tex_tried = 0;
static int g_draw_logged = 0;

/* Vignette (Extras, Look page): a dark gradient stretched over the frame. K toggles it. */
#define VIG_N 64
static void *g_vig_tex = NULL;
static int g_vig_on = 0;
static int g_mm_on = 1;        /* minimap: starts on; M toggles it */
static void make_minimap_tex(void);
/* Intro subtitles: L toggles; they follow the game's movie flag (see the subtitle block below). */
#define SUB_IMAGE_BASE 0x400000u
#define SUB_MOVIE_FLAG 0xB5069Cu
static int g_sub_on = 1;
static int g_sub_cur = -1;
static DWORD g_sub_t0 = 0;
static int g_sub_t0_set = 0;
static void make_sub_tex(void);

/* Overlay texture size, drawn at the top left of the back buffer. */
#define TEX_W 512
#define TEX_H 320
#define FVF_OV 0x144            /* XYZRHW | DIFFUSE | TEX1 */

#define VT(obj) (*(void ***)(obj))

/* No CRT: the linker needs this symbol when float code is used. */
int _fltused = 0;

static void logln(const char *s)
{
    HANDLE h = CreateFileA("d3d9_proxy.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD n;
        WriteFile(h, s, lstrlenA(s), &n, NULL);
        WriteFile(h, "\r\n", 2, &n, NULL);
        CloseHandle(h);
    }
}

static int patch_slot(void **vt, int idx, void *newfn, void **oldfn)
{
    DWORD old;
    if (!VirtualProtect(&vt[idx], sizeof(void *), PAGE_READWRITE, &old)) return 0;
    *oldfn = vt[idx];
    vt[idx] = newfn;
    VirtualProtect(&vt[idx], sizeof(void *), old, &old);
    return 1;
}

/* ---- Game data (offsets from qol/game.odin and qol/masters.odin of the port) ----
 * The character records start at SAVE_BASE in the game's RAM. g_save holds a copy
 * of that RAM block at file-style offset SAVE_HDR, so RAM address a is at
 * g_save[a - SAVE_BASE + SAVE_HDR]. */
#define SAVE_SIZE 0x1880u
#define SAVE_HDR 0x180u
#define SAVE_BASE 0x8011a94cu
#define SG(a) ((unsigned int)(a) - SAVE_BASE + SAVE_HDR)
#define RAM_BLOCK 0x1700u       /* bytes from SAVE_BASE used by the panel */
#define MAX_CAND 8
#define SCAN_CHUNK 0x100000u
#define SCAN_RETRY_MS 10000u
#define MASTER_N 12
#define PAGE_ROWS 6
#define MAX_TIER 4

enum { C_COMBO_HITS, C_HOURS, C_ENCOUNTERS, C_SPECIES, C_FAERIES, C_FISH,
       C_INVENTORY, C_COMBO_DMG, C_RYU, C_BALL, C_HIT_DMG };

static const char *COUNTER_NAME[] = {
    "combo hits", "play time", "battles", "species", "fairies",
    "fishing points", "item types", "combo damage", "Ryu level",
    "ball rank", "move damage"
};

typedef struct {
    const char *name;
    int counter;
    int tiers;
    unsigned int need[MAX_TIER];
    const char *reward[MAX_TIER];
    unsigned int skill[MAX_TIER];
} MASTER_DEF;

/* Indexed by master id - 1 (the predicate tables of each master's area overlay). */
static const MASTER_DEF MASTER_TAB[MASTER_N] = {
    {"Rwolf", C_COMBO_HITS, 4, {5, 10, 15, 20}, {"Eddy", "Plateau", "Magic Ball", "Flame Pillar"}, {153, 155, 89, 159}},
    {"Momo", C_HOURS, 4, {25, 30, 40, 50}, {"Spray", "Clip", "Oracle", "Egghead"}, {128, 129, 127, 141}},
    {"Kryrik", C_COMBO_HITS, 4, {30, 40, 50, 70}, {"Megaphone", "SpiritBlast", "Cleave", "Disembowel"}, {92, 130, 131, 87}},
    {"Abbess", C_SPECIES, 3, {70, 85, 100, 0}, {"Concentrate", "Sanctuary", "Celerity", ""}, {105, 113, 110, 0}},
    {"Njomo", C_FAERIES, 4, {8, 12, 16, 20}, {"FaerieAttack", "FaerieBreath", "War Shout", "FaerieCharge"}, {147, 145, 146, 148}},
    {"Gyosil", C_FISH, 3, {4000, 6000, 9500, 0}, {"Ring of Ice", "Spanner", "Master's Rod", ""}, {0, 0, 0, 0}},
    {"Stoll", C_INVENTORY, 2, {80, 120, 0, 0}, {"Steal", "Coward's Way", "", ""}, {120, 139, 0, 0}},
    {"Una", C_COMBO_DMG, 3, {1500, 3000, 10000, 0}, {"Pilfer", "Super Combo", "Blitz", ""}, {121, 88, 90, 0}},
    {"Bunyan", C_HIT_DMG, 4, {3000, 5000, 8000, 12000}, {"Backhand", "Counter", "Shadowwalk", "Final Hope"}, {137, 107, 86, 111}},
    {"Lyta", C_RYU, 4, {20, 25, 30, 35}, {"Supplication", "Holy Strike", "Resist", "Benediction"}, {108, 136, 106, 119}},
    {"Khan", C_ENCOUNTERS, 4, {300, 400, 500, 600}, {"Focus", "Shout", "Tiger Fist", "Flex"}, {104, 112, 157, 158}},
    {"Marlok", C_BALL, 3, {6, 8, 10, 0}, {"Charm", "Monopolize", "Roulette", ""}, {123, 122, 150, 0}},
};

/* Level-up bonus per stat (HP, AP, Pwr, Def, Agl, Wis), by master id - 1. */
static const signed char BONUS_TAB[MASTER_N][6] = {
    {0, 1, -1, 0, 0, 1}, {8, 0, 0, 0, 0, 0}, {25, -2, 3, 2, -2, -2},
    {0, 2, -2, -2, 0, 4}, {-8, 1, 0, 0, 2, 0}, {-8, 2, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 0}, {12, -2, 2, 1, 0, -1}, {-16, -3, 1, 1, 1, 1},
    {0, 1, 0, -1, 0, 1}, {20, -4, 3, 0, 1, -3}, {16, 3, -1, -1, -1, -1},
};
static const char *STAT_NAME[6] = {"HP", "AP", "Pwr", "Def", "Agl", "Wis"};

/* Copy of the game's RAM block, at g_save[SAVE_HDR + k] = RAM SAVE_BASE + k. */
static unsigned char g_save[SAVE_SIZE];
static int g_have_save = 0;
static int g_met[MASTER_N];
static int g_next[MASTER_N];
static unsigned int g_val[MASTER_N];
static char g_pupil[MASTER_N][12];   /* name of the apprentice, "" if none */
static int g_order[MASTER_N] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; /* met first, then the rest */

/* Live-copy tracking. g_cand_addr[i] is the RAM address of a Ryu record copy, and
 * g_snap[i] its last seen block. g_cur is the copy in use (-1 = none). */
static unsigned char *g_cand_addr[MAX_CAND];
static unsigned char g_snap[MAX_CAND][RAM_BLOCK];
static int g_ncand = 0;
static int g_cur = -1;
static int g_scan_tried = 0;
static DWORD g_last_scan = 0;
static unsigned char g_scan_buf[SCAN_CHUNK];
static unsigned char g_tmp[RAM_BLOCK];
static unsigned char g_live[RAM_BLOCK];

/* Masters panel state. */
static int g_panel_on = 0;           /* off by default; F12 */
static int g_journal_on = 0;         /* the Diario page; J */
static int g_page = 0;               /* 0 or 1: masters 1-6 or 7-12 of the order */
static int g_sel = 0;                /* master index shown in the box */
static DWORD g_last_read = 0;

/* Overlay drawing surface (GDI) and the texture uploaded from it. */
static HDC g_mdc = NULL;
static HBITMAP g_dib = NULL;
static void *g_dib_bits = NULL;
static HFONT g_font = NULL;

/* Ready notices (toasts), shown while the panel is closed, as the recomp does. */
#define TOASTS 3
#define TOAST_MS 6000
typedef struct { char text[72]; DWORD expires; } TOAST;
static TOAST g_toast[TOASTS];
static int g_ntoast = 0;
static int g_ready[MASTER_N];     /* reward index ready + 1 per master, 0 = none */
static int g_rebase = 1;          /* no notices on the first read after a scan or a jump */
/* Reads in a row with the current copy unchanged. The live copy changes every
 * second, so a copy that stays still is probably a stale one: scan again. */
static int g_still = 0;
#define STILL_TICKS 10
static unsigned int g_last_hours = 0;

static unsigned int rd8(unsigned int o) { return g_save[o]; }
static unsigned int rd16(unsigned int o) { return (unsigned int)g_save[o] | ((unsigned int)g_save[o + 1] << 8); }
static unsigned int rd32(unsigned int o)
{
    return (unsigned int)g_save[o] | ((unsigned int)g_save[o + 1] << 8) |
           ((unsigned int)g_save[o + 2] << 16) | ((unsigned int)g_save[o + 3] << 24);
}

static unsigned int popcount32(unsigned int v)
{
    unsigned int n = 0;
    while (v) { n += v & 1; v >>= 1; }
    return n;
}

/* ---- Live memory ---- */

static int read_block(const unsigned char *a, unsigned char *out)
{
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)a, out, RAM_BLOCK, &got)) return 0;
    return got == RAM_BLOCK;
}

/* A character record: name "Ryu" (NUL terminated) and a level from 1 to 99. */
static int rec_ok(const unsigned char *b)
{
    unsigned int lv = (unsigned int)b[0xA] | ((unsigned int)b[0xB] << 8);
    return b[0] == 'R' && b[1] == 'y' && b[2] == 'u' && b[3] == 0 && lv >= 1 && lv <= 99;
}

static int same_block(const unsigned char *a, const unsigned char *b)
{
    unsigned int k;
    for (k = 0; k < RAM_BLOCK; k++) if (a[k] != b[k]) return 0;
    return 1;
}

/* volatile stops the compiler from turning the loop into a memcpy call (no CRT). */
static void copy_block(unsigned char *dst, const unsigned char *src)
{
    volatile unsigned char *d = dst;
    unsigned int k;
    for (k = 0; k < RAM_BLOCK; k++) d[k] = src[k];
}

/* Walks the committed memory of the process and records up to MAX_CAND Ryu records. */
static void scan_ram(void)
{
    unsigned char *p = NULL;
    MEMORY_BASIC_INFORMATION mbi;
    g_ncand = 0;
    while ((UINT_PTR)p < (UINT_PTR)0x80000000u) {
        SIZE_T rs;
        unsigned char *base;
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
        rs = mbi.RegionSize;
        base = (unsigned char *)mbi.BaseAddress;
        if (rs == 0) break;
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            SIZE_T off = 0;
            while (off < rs) {
                SIZE_T n = rs - off, got = 0, i;
                if (n > SCAN_CHUNK) n = SCAN_CHUNK;
                if (ReadProcessMemory(GetCurrentProcess(), base + off, g_scan_buf, n, &got)) {
                    for (i = 0; i + 3 < got; i++) {
                        if (g_scan_buf[i] == 'R' && g_scan_buf[i + 1] == 'y' && g_scan_buf[i + 2] == 'u') {
                            unsigned char *a = base + off + i;
                            if (g_ncand < MAX_CAND && read_block(a, g_tmp) && rec_ok(g_tmp)) {
                                g_cand_addr[g_ncand] = a;
                                copy_block(g_snap[g_ncand], g_tmp);
                                g_ncand++;
                            }
                        }
                    }
                }
                /* Overlap of 3 bytes so a name split across two chunks is still found. */
                off += (off + n < rs && n > 3) ? n - 3 : n;
            }
        }
        p = base + rs;
    }
    {
        char s[80];
        wsprintfA(s, "ram scan: %d candidate(s)", g_ncand);
        logln(s);
    }
}

/* Reads the live record, switching to a copy that changed while the current one
 * did not. Fills g_save from the chosen copy. */
static int refresh_ram(void)
{
    DWORD now = GetTickCount();
    int i, cur_changed = 0, switched = -1;

    if (g_cur >= 0 && !(read_block(g_cand_addr[g_cur], g_live) && rec_ok(g_live))) {
        g_cur = -1;   /* the game restarted or the record moved */
    }
    if (g_cur >= 0 && g_still >= STILL_TICKS && (g_panel_on || g_journal_on)) {
        g_cur = -1;   /* stuck on a stale copy: look again */
    }
    if (g_cur < 0) {
        if (!g_panel_on && !g_journal_on) return 0;   /* scans only while a page is on: the scan stalls the game (the minimap reads fixed addresses) */
        if (g_scan_tried && now - g_last_scan < SCAN_RETRY_MS) return 0;
        g_last_scan = now;
        g_scan_tried = 1;
        scan_ram();
        if (!g_ncand) return 0;
        g_cur = 0;
        g_rebase = 1;
        g_still = 0;
    }

    if (read_block(g_cand_addr[g_cur], g_live) && rec_ok(g_live)) {
        cur_changed = !same_block(g_live, g_snap[g_cur]);
        copy_block(g_snap[g_cur], g_live);
    }
    g_still = cur_changed ? 0 : g_still + 1;
    for (i = 0; i < g_ncand; i++) {
        if (i == g_cur) continue;
        if (read_block(g_cand_addr[i], g_live) && rec_ok(g_live)) {
            if (!same_block(g_live, g_snap[i])) {
                if (!cur_changed && switched < 0) switched = i;
                copy_block(g_snap[i], g_live);
            }
        }
    }
    if (switched >= 0) {
        char s[64];
        g_cur = switched;
        wsprintfA(s, "live copy switched to %08X", (unsigned int)(UINT_PTR)g_cand_addr[g_cur]);
        logln(s);
    }
    copy_block(g_save + SAVE_HDR, g_snap[g_cur]);
    return 1;
}

static unsigned int counter_value(int c)
{
    unsigned int v = 0, k, L, s, at;
    switch (c) {
    case C_COMBO_HITS: return rd8(SG(0x8011b694));
    case C_HOURS: return rd32(SG(0x8011ad90));
    case C_ENCOUNTERS: return rd32(SG(0x8011b67c));
    case C_COMBO_DMG: return rd32(SG(0x8011b734));
    case C_HIT_DMG: return rd32(SG(0x8011b760));
    case C_RYU: return rd16(SG(0x8011a956));
    case C_BALL: return rd8(SG(0x8011b6e4));
    case C_FISH:
        for (k = 0; k < 30; k++) v += rd16(SG(0x8011b95c) + k * 2);
        return v;
    case C_SPECIES:
        for (k = 0; k < 8; k++) v += popcount32(rd32(SG(0x8011b61c) + k * 4));
        return v;
    case C_FAERIES:
        for (k = 0; k < 20; k++) v += rd8(SG(0x8011a94cu + 0x10b0u) + k * 0x28) != 0 ? 1u : 0u;
        return v;
    case C_INVENTORY:
        for (L = 0; L < 4; L++) {
            for (s = 0; s < 128; s++) {
                at = SG(0x8011ad98) + L * 0x200 + s * 2;
                if (rd8(at) && rd8(at + 1)) v++;
            }
        }
        return v;
    }
    return 0;
}

/* Whether the party already knows the skill (bit skill - 84 of SKILL_BITS). */
static int skill_known(unsigned int skill)
{
    unsigned int bit;
    if (skill < 84) return 0;
    bit = skill - 84;
    return (g_save[SG(0x8011b5c8) + (bit >> 3)] >> (bit & 7)) & 1;
}

static int master_met(int i)
{
    unsigned int id = (unsigned int)(i + 1);
    if (g_pupil[i][0]) return 1;
    return (g_save[SG(0x8011a94c) + 0xd54 + id / 8] >> (id % 8)) & 1;
}

/* Index of the next reward the party lacks (tiers once all are given). */
static int master_next(int i)
{
    const MASTER_DEF *m = &MASTER_TAB[i];
    unsigned int progress = g_save[SG(0x8011a94c) + 0xd55 + (unsigned int)(i + 1)];
    int next;
    if (progress == 0xff) return m->tiers;
    next = progress < (unsigned int)m->tiers ? (int)progress : m->tiers;
    while (next < m->tiers && m->skill[next] && skill_known(m->skill[next])) next++;
    return next;
}

static void copy_name(char *dst, const unsigned char *src)
{
    int n;
    for (n = 0; n < 8 && src[n]; n++) dst[n] = (char)src[n];
    dst[n] = 0;
}

/* Adds a notice "master has reward for apprentice", dropping the oldest when full. */
static void add_toast(int i, int ready)
{
    TOAST *t;
    if (g_ntoast == TOASTS) {
        int k, c;
        for (k = 1; k < TOASTS; k++) {
            volatile char *d = g_toast[k - 1].text;
            const char *s = g_toast[k].text;
            for (c = 0; c < 72; c++) d[c] = s[c];
            g_toast[k - 1].expires = g_toast[k].expires;
        }
        g_ntoast--;
    }
    t = &g_toast[g_ntoast++];
    wsprintfA(t->text, "%s has %s for %s", MASTER_TAB[i].name, MASTER_TAB[i].reward[ready], g_pupil[i]);
    t->expires = GetTickCount() + TOAST_MS;
}

/* Reads the live game data and fills the panel state. Returns 0 when there is no game data. */
static int refresh_state(void)
{
    int i, slot, n;
    if (!refresh_ram()) {
        g_have_save = 0;
        return 0;
    }
    g_have_save = 1;
    for (i = 0; i < MASTER_N; i++) g_pupil[i][0] = 0;
    for (slot = 0; slot < 6; slot++) {
        unsigned int idx = rd8(SG(0x8011ad88) + slot);
        unsigned int rec, mid;
        if (idx >= 7) continue;
        rec = SG(0x8011a94c) + idx * 0x98;
        mid = rd8(rec + 0x84);
        if (mid >= 1 && mid <= MASTER_N && !g_pupil[mid - 1][0])
            copy_name(g_pupil[mid - 1], &g_save[rec]);
    }
    {
        /* A save load or a new game shows up as the play time jumping: no notices then. */
        unsigned int hours = counter_value(C_HOURS);
        if (hours < g_last_hours || hours > g_last_hours + 1) g_rebase = 1;
        g_last_hours = hours;
    }
    for (i = 0; i < MASTER_N; i++) {
        int ready;
        g_val[i] = counter_value(MASTER_TAB[i].counter);
        g_met[i] = master_met(i);
        g_next[i] = master_next(i);
        ready = (g_next[i] < MASTER_TAB[i].tiers && g_val[i] >= MASTER_TAB[i].need[g_next[i]]) ? g_next[i] : -1;
        if (!g_rebase && ready >= 0 && ready + 1 != g_ready[i] && g_pupil[i][0]) add_toast(i, ready);
        g_ready[i] = ready + 1;
    }
    g_rebase = 0;
    n = 0;
    for (i = 0; i < MASTER_N; i++) if (g_met[i]) g_order[n++] = i;
    for (i = 0; i < MASTER_N; i++) if (!g_met[i]) g_order[n++] = i;
    return 1;
}

/* ---- Painting into the GDI surface ---- */
static void txt(int x, int y, const char *s, COLORREF c)
{
    SetTextColor(g_mdc, c);
    TextOutA(g_mdc, x, y, s, lstrlenA(s));
}

static void fill_all(COLORREF c)
{
    RECT r;
    HBRUSH br;
    r.left = 0; r.top = 0; r.right = TEX_W; r.bottom = TEX_H;
    br = CreateSolidBrush(c);
    FillRect(g_mdc, &r, br);
    DeleteObject(br);
}

/* The debug label at the top left, used while the panel is off. */
static void paint_label(void)
{
    fill_all(RGB(0, 0, 0));
    SetBkMode(g_mdc, TRANSPARENT);
    SelectObject(g_mdc, g_font);
    /* The "D3D9 PROXY OK" label and its red square were removed on request. */
    {
        DWORD now = GetTickCount();
        int i, y = 44;
        for (i = 0; i < g_ntoast; i++) {
            if ((long)(g_toast[i].expires - now) > 0) {
                txt(10, y, g_toast[i].text, RGB(255, 255, 0));
                y += 18;
            }
        }
    }
}

/* Master i's portrait at the top right of the panel. Transparent pixels are skipped. */
static void draw_face(int i)
{
    const int x0 = 456, y0 = 8;
    int x, y;
    if (!g_dib_bits || i < 0 || i >= MASTER_N || !FACE_HAS[i]) return;
    for (y = 0; y < FACEP_H; y++) {
        DWORD *dst = (DWORD *)g_dib_bits + (unsigned int)(y0 + y) * TEX_W + x0;
        for (x = 0; x < FACEP_W; x++) {
            unsigned int v = FACE_PX[i][y * FACEP_W + x];
            if (v) dst[x] = v;
        }
    }
}

/* Level-up bonus icon (HP, AP, Pwr, Def, Agl, Wis): 0x00RRGGBB pixels, copied as they are. */
static const unsigned int *const ICON_PX[6] = { ICON_HP_PX, ICON_AP_PX, ICON_PWR_PX, ICON_DEF_PX, ICON_AGL_PX, ICON_WIS_PX };
static const int ICON_W[6] = { ICON_HP_W, ICON_AP_W, ICON_PWR_W, ICON_DEF_W, ICON_AGL_W, ICON_WIS_W };
static const int ICON_H[6] = { ICON_HP_H, ICON_AP_H, ICON_PWR_H, ICON_DEF_H, ICON_AGL_H, ICON_WIS_H };

static void draw_icon(const unsigned int *px, int w, int h, int x0, int y0)
{
    int x, y;
    if (!g_dib_bits) return;
    for (y = 0; y < h; y++) {
        DWORD *dst = (DWORD *)g_dib_bits + (unsigned int)(y0 + y) * TEX_W + x0;
        for (x = 0; x < w; x++) { unsigned int v = px[y * w + x]; if (v) dst[x] = v; }
    }
}

static void paint_panel(void)
{
    char buf[96];
    int k, idx, i, t, y;
    const MASTER_DEF *m;

    fill_all(RGB(12, 12, 36));
    SetBkMode(g_mdc, TRANSPARENT);
    SelectObject(g_mdc, g_font);

    wsprintfA(buf, "Masters  page %u/2", (unsigned int)(g_page + 1));
    txt(8, 8, buf, RGB(255, 255, 0));
    for (k = 0; k < PAGE_ROWS; k++) {
        idx = g_page * PAGE_ROWS + k;
        if (idx >= MASTER_N) break;
        i = g_order[idx];
        if (!g_have_save || !g_met[i]) {
            wsprintfA(buf, "%u ???", (unsigned int)(k + 1));
            txt(8, 32 + k * 22, buf, RGB(140, 140, 140));
        } else {
            wsprintfA(buf, "%u %s", (unsigned int)(k + 1), MASTER_TAB[i].name);
            txt(8, 32 + k * 22, buf, i == g_sel ? RGB(255, 255, 0) : RGB(255, 255, 255));
        }
    }

    if (!g_have_save) {
        txt(160, 8, "No save loaded", RGB(140, 140, 140));
    } else if (!g_met[g_sel]) {
        txt(160, 8, "???", RGB(255, 255, 0));
        txt(160, 30, "Not known yet.", RGB(140, 140, 140));
    } else {
        m = &MASTER_TAB[g_sel];
        txt(160, 8, m->name, RGB(255, 255, 0));
        txt(160, 30, "Apprentice", RGB(255, 255, 255));
        txt(264, 30, g_pupil[g_sel][0] ? g_pupil[g_sel] : "none",
            g_pupil[g_sel][0] ? RGB(255, 255, 255) : RGB(140, 140, 140));
        txt(160, 52, "Count", RGB(255, 255, 255));
        txt(264, 52, COUNTER_NAME[m->counter], RGB(255, 255, 255));
        txt(160, 74, "Has", RGB(255, 255, 255));
        wsprintfA(buf, "%u", g_val[g_sel]);
        txt(264, 74, buf, RGB(255, 255, 255));

        txt(160, 100, "Needs", RGB(140, 140, 140));
        txt(240, 100, "Reward", RGB(140, 140, 140));
        txt(400, 100, "Status", RGB(140, 140, 140));
        for (t = 0; t < m->tiers; t++) {
            COLORREF col = RGB(255, 255, 255);
            const char *status = "";
            y = 122 + t * 22;
            if (t < g_next[g_sel]) {
                col = RGB(140, 140, 140);
                status = "Earned";
            } else if (g_val[g_sel] >= m->need[t]) {
                col = RGB(120, 230, 120);
                status = t == g_next[g_sel] ? "ready" : "done";
            }
            wsprintfA(buf, "%u", m->need[t]);
            txt(160, y, buf, col);
            txt(240, y, m->reward[t], col);
            txt(400, y, status, col);
        }

        txt(160, 226, "Level-up bonus", RGB(255, 255, 0));
        for (k = 0; k < 6; k++) {
            int b = BONUS_TAB[g_sel][k];
            draw_icon(ICON_PX[k], ICON_W[k], ICON_H[k], 232 + k * 46 + (12 - ICON_W[k]) / 2, 244);
            if (b > 0) {
                wsprintfA(buf, "+%d", b);
                txt(232 + k * 46, 270, buf, RGB(120, 230, 120));
            } else if (b < 0) {
                wsprintfA(buf, "%d", b);
                txt(232 + k * 46, 270, buf, RGB(230, 110, 110));
            } else {
                txt(232 + k * 46, 270, "-", RGB(140, 140, 140));
            }
        }
    }
    if (g_have_save && g_met[g_sel]) draw_face(g_sel);
    if (g_cur >= 0) wsprintfA(buf, "RAM %08X", (unsigned int)(UINT_PTR)g_cand_addr[g_cur]);
    else wsprintfA(buf, "RAM not detected");
    txt(8, 280, buf, RGB(140, 140, 140));
    txt(8, 296, "F12 close  F10 page  1-6", RGB(140, 140, 140));
}

/* ---- Diario: the port's objective journal (qol/tracker.odin) ----
 * Each beat is a bit of its area's progress word (AREA_WORDS + area * 4). The
 * objective shown is the one after the last beat done. */
#define AREA_WORDS 0x8011b7d0u
#define OBJ_N 11
#define JL_MAX 48
#define JL_ROWS 16
#define JL_CHARS 64

typedef struct { unsigned int area, bit; const char *text; } OBJECTIVE;
static const OBJECTIVE OBJECTIVES[OBJ_N] = {
    {2, 1, "Follow the cliffs on the way to Sarai."},
    {2, 3, "Jump over the gaps on the cliff road."},
    {2, 4, "Keep climbing while there is light."},
    {2, 5, "Find a place to camp for the night."},
    {2, 6, "Fou-Lu: Leave the tomb."},
    {2, 10, "Fou-Lu: Go to the capital."},
    {2, 11, "Fou-Lu: Escape the burning forest."},
    {4, 0, "Climb the cliffs to the city of Sarai."},
    {4, 12, "Ask in Sarai about sandflier parts. Try the tavern."},
    {4, 18, "Get a shovel and the dog in the Sandflier Valley, south of the city."},
    {4, 19, "Dig behind the part in the Sandflier Valley."},
};

/* One general line per story area (chapter), for the parts of the game the beats above
 * do not cover. PROVISIONAL: written from which maps use each area's flags and a sample
 * of their text, not from play. bof4_flaglog.txt (below) is how they get refined. */
#define RESUME_AREA 0x8011b7c0u   /* u8: the save's current area */
#define AREA_N 24
static const char *const AREA_TEXT[AREA_N] = {
    NULL,
    "Kyria and the dam: cross the trap village and solve the dam problem.",
    "Cliffs on the way to Sarai (and Fou-Lu's awakening).",
    "Chamba: cross the cursed city and continue the trip with the sandflier.",
    "Sarai and the Sandflier Valley: get the sandflier part.",
    "Synesta: look for Marlok and do what he asks in the city.",
    "Worent and the mist plain: head to Wyndia Castle.",
    "Imperial base and the portal: go through the cargo port and the Gateway.",
    "Ludia Castle and the fairy world: take care of the prisoners and the truce.",
    "Under Wyndia Castle: go down to the altar.",
    "Wind mountains: ride the gondola and talk to the Wind Oracle.",
    "Isolated village and dreams: follow the clues to the next dragon.",
    "The mountain that woke up: climb to the place of powerful aura.",
    "Abandoned village: find the dragon and go down the mountain.",
    "Shikk and the sea: get a ship and go to the Sea God's sanctuary.",
    "Crossing to the Empire: resupply at the port and continue to Chedo, the capital.",
    "Imperial Castle: enter the castle.",
    "Chiqua and Koshka villages: explore the lands reached by sea.",
    "Toward the capital: free the prisoners and pass through the sacred tomb.",
    "Chedo, the capital, after the disaster: advance to the castle.",
    NULL,
    NULL,
    "Imperial Castle, final stretch: climb the floors to the end.",
    NULL,
};

typedef struct { char text[80]; COLORREF colour; int indent; } JLINE;
static JLINE g_jl[JL_MAX];
static int g_jn = 0;
static int g_journal_top = 0;

/* Index of the beat after the last one done (the beats before it count as done). */
static unsigned int objective_next(void)
{
    unsigned int i, next = 0;
    for (i = 0; i < OBJ_N; i++) {
        unsigned int word = rd32(SG(AREA_WORDS + OBJECTIVES[i].area * 4));
        if (word & (1u << OBJECTIVES[i].bit)) next = i + 1;
    }
    return next;
}

/* Adds text as journal rows, wrapped at spaces. Empty text adds a blank row. */
static void jl_add(const char *s, COLORREF c, int indent)
{
    int len = lstrlenA(s), start = 0;
    if (len == 0) {
        if (g_jn < JL_MAX) {
            g_jl[g_jn].text[0] = 0;
            g_jl[g_jn].colour = c;
            g_jl[g_jn].indent = indent;
            g_jn++;
        }
        return;
    }
    while (start < len && g_jn < JL_MAX) {
        int end = start + JL_CHARS, k;
        if (end >= len) {
            end = len;
        } else {
            int cut = end;
            while (cut > start && s[cut] != ' ') cut--;
            if (cut > start) end = cut;
        }
        for (k = 0; k < end - start; k++) g_jl[g_jn].text[k] = s[start + k];
        g_jl[g_jn].text[end - start] = 0;
        g_jl[g_jn].colour = c;
        g_jl[g_jn].indent = indent;
        g_jn++;
        start = end;
        while (start < len && s[start] == ' ') start++;
    }
}

static void paint_journal(void)
{
    unsigned int next, i;
    int row, top, maxtop;
    fill_all(RGB(12, 12, 36));
    SetBkMode(g_mdc, TRANSPARENT);
    SelectObject(g_mdc, g_font);
    txt(8, 8, "Journal", RGB(255, 255, 0));
    if (!g_have_save) {
        txt(8, 34, "No save loaded", RGB(140, 140, 140));
    } else {
        /* The step-by-step beats cover areas 2 and 4 only. Anywhere else (or past their
         * end) the chapter's general line shows, picked by the save's current area. */
        unsigned int area = rd8(SG(RESUME_AREA));
        const char *chapter = area < AREA_N ? AREA_TEXT[area] : NULL;
        g_jn = 0;
        next = objective_next();
        jl_add("Now", RGB(255, 255, 0), 0);
        if ((area == 2 || area == 4) && next < OBJ_N) jl_add(OBJECTIVES[next].text, RGB(255, 255, 255), 8);
        else if (chapter) jl_add(chapter, RGB(255, 255, 255), 8);
        else jl_add("No objective noted yet.", RGB(140, 140, 140), 8);
        if (next > 0) {
            jl_add("", RGB(255, 255, 255), 0);
            jl_add("Done", RGB(255, 255, 0), 0);
            for (i = next; i > 0; i--) jl_add(OBJECTIVES[i - 1].text, RGB(255, 255, 255), 8);
        }
        {
            /* Chapters with any beat done, other than the current one. Numeric order: the
             * story's order of the areas is not known yet. */
            int head = 0;
            for (i = 0; i < AREA_N; i++) {
                if (i == area || i == 2 || i == 4 || !AREA_TEXT[i]) continue;
                if (!rd32(SG(AREA_WORDS + i * 4))) continue;
                if (!head) {
                    jl_add("", RGB(255, 255, 255), 0);
                    jl_add("Areas visited", RGB(255, 255, 0), 0);
                    head = 1;
                }
                jl_add(AREA_TEXT[i], RGB(200, 200, 200), 8);
            }
        }
        maxtop = g_jn - JL_ROWS;
        if (maxtop < 0) maxtop = 0;
        if (g_journal_top > maxtop) g_journal_top = maxtop;
        if (g_journal_top < 0) g_journal_top = 0;
        top = g_journal_top;
        for (row = 0; row < JL_ROWS && top + row < g_jn; row++) {
            JLINE *l = &g_jl[top + row];
            if (l->text[0]) txt(8 + l->indent, 34 + row * 16, l->text, l->colour);
        }
    }
    txt(8, 296, "Q/E scroll  J close", RGB(140, 140, 140));
}

/* Copies the GDI surface into the texture. Black becomes alpha 0. */
static void update_overlay_tex(void)
{
    OV_LOCKED lr;
    long hr;
    int x, y;
    if (!g_tex || !g_dib_bits) return;
    lr.Pitch = 0;
    lr.pBits = NULL;
    hr = ((PFN_LockRect)VT(g_tex)[19])(g_tex, 0, &lr, NULL, 0);
    if (hr < 0 || !lr.pBits) return;
    for (y = 0; y < TEX_H; y++) {
        DWORD *src = (DWORD *)g_dib_bits + (unsigned int)y * TEX_W;
        DWORD *dst = (DWORD *)((unsigned char *)lr.pBits + y * lr.Pitch);
        for (x = 0; x < TEX_W; x++) {
            DWORD c = src[x] & 0x00FFFFFFu;
            dst[x] = (c ? 0xFF000000u : 0u) | c;
        }
    }
    ((PFN_UnlockRect)VT(g_tex)[20])(g_tex, 0);
}

/* Repaints the surface for the current panel state and uploads it. */
static void repaint(void)
{
    if (!g_mdc) return;
    if (g_journal_on) paint_journal();
    else if (g_panel_on) paint_panel();
    else paint_label();
    GdiFlush();
    update_overlay_tex();
}

/* Moves the selection by delta positions in the list, and switches the page to match. */
static void move_sel(int delta)
{
    int pos = 0, i;
    for (i = 0; i < MASTER_N; i++) if (g_order[i] == g_sel) pos = i;
    pos += delta;
    if (pos < 0) pos = 0;
    if (pos >= MASTER_N) pos = MASTER_N - 1;
    g_sel = g_order[pos];
    g_page = pos / PAGE_ROWS;
}

/* ---- Story flag log (the recomp's BOF4_FLAGLOG): every change in the game's flag
 * region is appended to bof4_flaglog.txt with the map and the leader's cell, so story
 * beats can be named later from a play session. Reads the game's globals at their fixed
 * addresses (PS1 address + FL_DELTA); nothing is written to the game. ---- */
#define FL_DELTA (0xB55800u - 0x8011A94Cu)
#define FL_LO 0x8011b5c0u
#define FL_SIZE 0x2d8u            /* up to 0x8011b898 */
#define FL_POS_LO 0x200u          /* 0x8011b7c0..c7: the resume position, not flags */
#define FL_POS_HI 0x208u
#define FL_AREA_LO 0x210u         /* 0x8011b7d0: the per-area progress words */
#define FL_BULK 48                /* more changes than this in one frame is a load, not a beat */
#define FL_MAP 0xBAE300u          /* u16 current map, u8 section at +2 */
static unsigned char g_fl[FL_SIZE];
static int g_fl_have = 0;

/* Bytes of the region that are not story flags: the resume position, the world-map
 * return spot and the running counters next to it (0x8011b718..0x8011b76f: one of them
 * ticks many times a second), and the encounter and combo counters. */
static int fl_skip(unsigned int i)
{
    unsigned int a = FL_LO + i;
    if (i >= FL_POS_LO && i < FL_POS_HI) return 1;
    if (a >= 0x8011b718u && a < 0x8011b770u) return 1;
    if (a >= 0x8011b67cu && a < 0x8011b680u) return 1;
    if (a == 0x8011b694u) return 1;
    return 0;
}

static void fl_write(const char *s)
{
    HANDLE h = CreateFileA("bof4_flaglog.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD n;
        WriteFile(h, s, lstrlenA(s), &n, NULL);
        WriteFile(h, "\r\n", 2, &n, NULL);
        CloseHandle(h);
    }
}

static void flaglog_frame(void)
{
    const unsigned char *live = (const unsigned char *)(UINT_PTR)(FL_LO + FL_DELTA);
    const unsigned char *ryu = (const unsigned char *)(UINT_PTR)0xB55800u;
    const unsigned char *obj = (const unsigned char *)(UINT_PTR)0xB554C0u;
    unsigned int i, nchg = 0, map, sec, area, secs;
    int x, z;
    char s[200];

    if ((UINT_PTR)GetModuleHandleA(NULL) != 0x400000u) return;
    if (!(ryu[0] == 'R' && ryu[1] == 'y' && ryu[2] == 'u' && ryu[3] == 0)) { g_fl_have = 0; return; }   /* no game loaded */
    for (i = 0; i < FL_SIZE; i++)
        if (live[i] != g_fl[i] && !fl_skip(i)) nchg++;
    if (!nchg && g_fl_have) return;

    map = *(const unsigned short *)(UINT_PTR)FL_MAP;
    sec = *(const unsigned char *)(UINT_PTR)(FL_MAP + 2);
    area = live[FL_POS_LO];
    secs = *(const unsigned int *)(UINT_PTR)(0x8011ad90u + FL_DELTA);
    x = *(const int *)(obj + 0xc) >> 16;
    z = *(const int *)(obj + 0x14) >> 16;

    if (!g_fl_have || nchg > FL_BULK) {
        /* First sight of a game, or a save was loaded: the area words as they stand. */
        wsprintfA(s, "--- estado (play=%u map=%04X.%u area=%u, %u bytes mudaram)", secs, map, sec, area, nchg);
        fl_write(s);
        for (i = 0; i < 48; i++) {
            unsigned int w = *(const unsigned int *)(live + FL_AREA_LO + i * 4);
            if (w) { wsprintfA(s, "    area %u = %08X", i, w); fl_write(s); }
        }
    } else {
        for (i = 0; i < FL_SIZE; i++) {
            unsigned int o = g_fl[i], n = live[i], b;
            if (o == n || fl_skip(i)) continue;
            for (b = 0; b < 8; b++) {
                if (((o ^ n) >> b) & 1) {
                    if (i >= FL_AREA_LO)
                        wsprintfA(s, "play=%u map=%04X.%u area=%u pos=%d,%d  %s area %u bit %u  (%08X %02X->%02X)",
                                  secs, map, sec, area, x, z, ((n >> b) & 1) ? "SET" : "CLR",
                                  (i - FL_AREA_LO) / 4, ((i - FL_AREA_LO) % 4) * 8 + b, FL_LO + i, o, n);
                    else
                        wsprintfA(s, "play=%u map=%04X.%u area=%u pos=%d,%d  %s flag %08X bit %u  (%02X->%02X)",
                                  secs, map, sec, area, x, z, ((n >> b) & 1) ? "set" : "clr", FL_LO + i, b, o, n);
                    fl_write(s);
                }
            }
        }
    }
    for (i = 0; i < FL_SIZE; i++) g_fl[i] = live[i];
    g_fl_have = 1;
}

/* Keys are polled every frame so each press is seen once. */
static void masters_frame(void)
{
    int f12 = (GetAsyncKeyState(VK_F12) & 1) != 0;
    int f10 = (GetAsyncKeyState(VK_F10) & 1) != 0;
    int jkey = (GetAsyncKeyState('J') & 1) != 0;
    int kkey = (GetAsyncKeyState('K') & 1) != 0;
    int mkey = (GetAsyncKeyState('M') & 1) != 0;
    int lkey = (GetAsyncKeyState('L') & 1) != 0;
    /* Panel keys, chosen so the game does not use them: Q/E master up/down,
     * R/T page 1/2 (F10 also flips the page). */
    int pgup = (GetAsyncKeyState('R') & 1) != 0;
    int pgdn = (GetAsyncKeyState('T') & 1) != 0;
    int up = (GetAsyncKeyState('Q') & 1) != 0;
    int down = (GetAsyncKeyState('E') & 1) != 0;
    /* Arrow keys only read here (not remapped): they also reach the game, so the
     * character may walk while the panel is open. */
    int aup = (GetAsyncKeyState(VK_UP) & 1) != 0;
    int adown = (GetAsyncKeyState(VK_DOWN) & 1) != 0;
    int aleft = (GetAsyncKeyState(VK_LEFT) & 1) != 0;
    int aright = (GetAsyncKeyState(VK_RIGHT) & 1) != 0;
    int pick[PAGE_ROWS];
    int k, dirty = 0;
    DWORD now;

    for (k = 0; k < PAGE_ROWS; k++) pick[k] = (GetAsyncKeyState('1' + k) & 1) != 0;

    if (f12) {
        g_panel_on = !g_panel_on;
        g_journal_on = 0;
        g_last_read = 0;
        dirty = 1;
    }
    if (jkey) {
        g_journal_on = !g_journal_on;
        if (g_journal_on) g_panel_on = 0;
        g_journal_top = 0;
        dirty = 1;
    }
    if (kkey) g_vig_on = !g_vig_on;
    if (mkey) { g_mm_on = !g_mm_on; dirty = 1; }   /* repaint: the debug label hides under the minimap */
    if (lkey) g_sub_on = !g_sub_on;
    if (g_journal_on) {
        if (up) { g_journal_top--; dirty = 1; }
        if (down) { g_journal_top++; dirty = 1; }
    }
    /* Data is read every second, panel open or not, so notices can show. */
    now = GetTickCount();
    if (now - g_last_read >= 1000) {
        g_last_read = now;
        refresh_state();
        if (g_panel_on && !g_have_save) g_sel = 0;
        dirty = 1;
    }
    if (g_panel_on) {
        if (f10) {
            g_page = 1 - g_page;
            g_sel = g_order[g_page * PAGE_ROWS];
            dirty = 1;
        }
        if (pgup || pgdn) {
            g_page = pgdn ? 1 : 0;
            g_sel = g_order[g_page * PAGE_ROWS];
            dirty = 1;
        }
        if (up || aup) { move_sel(-1); dirty = 1; }
        if (down || adown) { move_sel(1); dirty = 1; }
        if (aleft || aright) {
            g_page = aright ? 1 : 0;
            g_sel = g_order[g_page * PAGE_ROWS];
            dirty = 1;
        }
        for (k = 0; k < PAGE_ROWS; k++) {
            int idx = g_page * PAGE_ROWS + k;
            if (pick[k] && idx < MASTER_N) {
                g_sel = g_order[idx];
                dirty = 1;
            }
        }
    }
    if (dirty) repaint();
}

static void set_v(OV_V *v, float x, float y, float u, float w)
{
    v->x = x; v->y = y; v->z = 0.0f; v->rhw = 1.0f;
    v->c = 0xFFFFFFFFu;
    v->u = u; v->v = w;
}

/* Builds the vignette texture: transparent in the middle, black and up to alpha 200
 * at the corners. Integer maths only (the no-CRT build has no float-to-int helper). */
static void make_vignette(void)
{
    void *tex = NULL;
    OV_LOCKED lr;
    long hr;
    int x, y;
    hr = ((PFN_CreateTex)VT(g_dev)[23])(g_dev, VIG_N, VIG_N, 1, 0, 21, 1, &tex, NULL);
    if (hr < 0 || !tex) return;
    lr.Pitch = 0;
    lr.pBits = NULL;
    if (((PFN_LockRect)VT(tex)[19])(tex, 0, &lr, NULL, 0) < 0 || !lr.pBits) return;
    for (y = 0; y < VIG_N; y++) {
        DWORD *row = (DWORD *)((unsigned char *)lr.pBits + y * lr.Pitch);
        for (x = 0; x < VIG_N; x++) {
            int dx = 2 * x + 1 - VIG_N, dy = 2 * y + 1 - VIG_N;
            int d = dx * dx + dy * dy;          /* 0 at the centre, 2 * VIG_N^2 at the corners */
            int a = 0;
            if (d > (2 * VIG_N * VIG_N) / 5) {  /* starts at 0.4 of the way out */
                a = (d - (2 * VIG_N * VIG_N) / 5) * 200 / ((2 * VIG_N * VIG_N) * 3 / 5);
                if (a > 200) a = 200;
                a = a * a / 200;
            }
            row[x] = (DWORD)a << 24;
        }
    }
    ((PFN_UnlockRect)VT(tex)[20])(tex, 0);
    g_vig_tex = tex;
}

/* Creates the GDI surface and the texture, then paints the first frame. */
static void build_overlay_tex(void)
{
    BITMAPINFO bi;
    BITMAPINFOHEADER *bh = &bi.bmiHeader;
    void *tex = NULL;
    long hr;
    char s[96];

    bh->biSize = sizeof(BITMAPINFOHEADER);
    bh->biWidth = TEX_W;
    bh->biHeight = -TEX_H;
    bh->biPlanes = 1;
    bh->biBitCount = 32;
    bh->biCompression = BI_RGB;
    bh->biSizeImage = 0;
    bh->biXPelsPerMeter = 0;
    bh->biYPelsPerMeter = 0;
    bh->biClrUsed = 0;
    bh->biClrImportant = 0;
    g_mdc = CreateCompatibleDC(NULL);
    g_dib = CreateDIBSection(g_mdc, &bi, DIB_RGB_COLORS, &g_dib_bits, NULL, 0);
    if (!g_dib || !g_dib_bits) { logln("overlay: CreateDIBSection failed"); return; }
    SelectObject(g_mdc, g_dib);
    g_font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, "Arial");

    hr = ((PFN_CreateTex)VT(g_dev)[23])(g_dev, TEX_W, TEX_H, 1, 0, 21, 1, &tex, NULL);
    wsprintfA(s, "overlay CreateTexture hr=%08X", (unsigned int)hr);
    logln(s);
    if (hr >= 0 && tex) g_tex = tex;
    make_vignette();
    make_minimap_tex();
    make_sub_tex();
    repaint();
}

/* ---- Intro subtitles (English). Same timings as the German patch's MOV\ZBOF4.srt.
 * The clock starts when the game starts the movie; L toggles. Times in ms. Text in plain ASCII. ---- */
typedef struct { DWORD start, end; const char *text; } SUB_CUE;
static const SUB_CUE SUBS[] = {
    {35501, 36737, "Nina: I believed in you..."},
    {37286, 38886, "Nina: Right from the start."},
    {43320, 44786, "Ryu: What..."},
    {44810, 47486, "Ryu: What are you trying to do?"},
    {47900, 50299, "Ryu: Where do you think you're going?"},
    {64786, 68461, "Ryu: So you're saying people are so corrupt they can never change?"},
    {68462, 70420, "Ryu: So that's what you believe?!"},
    {71314, 72620, "Ryu: You're wrong!"},
    {73053, 73853, "Ryu: People..."},
    {74420, 76020, "Ryu: People are not so corrupt..."},
    {82268, 84353, "Nina: Who..."},
    {84377, 86653, "Nina: Who... are you?"},
};
#define SUB_COUNT ((int)(sizeof(SUBS) / sizeof(SUBS[0])))
#define SUB_W 1280
#define SUB_H 56
#define SUB_BOTTOM 40
static void *g_sub_tex = NULL;
static HDC g_sdc = NULL;
static HBITMAP g_sdib = NULL;
static void *g_sdib_bits = NULL;
static HFONT g_sfont = NULL;
static int g_sub_shown = 0;

static void make_sub_tex(void)
{
    BITMAPINFO bi;
    BITMAPINFOHEADER *bh = &bi.bmiHeader;
    void *tex = NULL;
    long hr;
    bh->biSize = sizeof(BITMAPINFOHEADER);
    bh->biWidth = SUB_W;
    bh->biHeight = -SUB_H;
    bh->biPlanes = 1;
    bh->biBitCount = 32;
    bh->biCompression = BI_RGB;
    bh->biSizeImage = 0;
    bh->biXPelsPerMeter = 0;
    bh->biYPelsPerMeter = 0;
    bh->biClrUsed = 0;
    bh->biClrImportant = 0;
    g_sdc = CreateCompatibleDC(NULL);
    g_sdib = CreateDIBSection(g_sdc, &bi, DIB_RGB_COLORS, &g_sdib_bits, NULL, 0);
    if (!g_sdib || !g_sdib_bits) return;
    SelectObject(g_sdc, g_sdib);
    g_sfont = CreateFontA(-26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, "Arial");
    hr = ((PFN_CreateTex)VT(g_dev)[23])(g_dev, SUB_W, SUB_H, 1, 0, 21, 1, &tex, NULL);
    if (hr >= 0 && tex) g_sub_tex = tex;
}

/* Paints the text txt into the subtitle surface and uploads it. */
static void text_paint(const char *txt)
{
    RECT rc, bar;
    OV_LOCKED lr;
    int x, y;
    if (!g_sub_tex || !g_sdib_bits) return;
    /* No bar: the background stays black, which uploads as transparent. The text has a
     * near-black shadow (RGB 1,1,1 is not transparent) so it reads over bright scenes. */
    bar.left = 0; bar.top = 0; bar.right = SUB_W; bar.bottom = SUB_H;
    {
        HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(g_sdc, &bar, br);
        DeleteObject(br);
    }
    SetBkMode(g_sdc, TRANSPARENT);
    SelectObject(g_sdc, g_sfont);
    SetTextColor(g_sdc, RGB(1, 1, 1));
    rc = bar;
    rc.left += 3; rc.top += 2; rc.right += 3; rc.bottom += 2;
    DrawTextA(g_sdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(g_sdc, RGB(255, 255, 0));   /* yellow, with the speaker's name before it */
    rc = bar;
    DrawTextA(g_sdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    GdiFlush();
    lr.Pitch = 0;
    lr.pBits = NULL;
    if (((PFN_LockRect)VT(g_sub_tex)[19])(g_sub_tex, 0, &lr, NULL, 0) < 0 || !lr.pBits) return;
    for (y = 0; y < SUB_H; y++) {
        DWORD *src = (DWORD *)g_sdib_bits + (unsigned int)y * SUB_W;
        DWORD *dst = (DWORD *)((unsigned char *)lr.pBits + y * lr.Pitch);
        for (x = 0; x < SUB_W; x++) dst[x] = (src[x] & 0x00FFFFFFu) ? (0xFF000000u | (src[x] & 0x00FFFFFFu)) : 0u;
    }
    ((PFN_UnlockRect)VT(g_sub_tex)[20])(g_sub_tex, 0);
}

static void sub_paint(int idx)
{
    text_paint(SUBS[idx].text);
}

/* ---- Status line: a short notice at the bottom of the screen, on the subtitle surface.
 * F8 uses it to show whether random encounters are on or off. ---- */
#define STATUS_MS 1500
static char g_status_txt[64];
static DWORD g_status_until = 0;
static int g_status_dirty = 0;
static int g_status_active = 0;

static void status_show(const char *s)
{
    int i;
    for (i = 0; i < 63 && s[i]; i++) g_status_txt[i] = s[i];
    g_status_txt[i] = 0;
    g_status_until = GetTickCount() + STATUS_MS;
    g_status_dirty = 1;
}

/* Per frame: after the subtitle frame, so the notice takes the surface while it is shown. */
static void status_frame(void)
{
    if (!g_status_until || !g_sub_tex) return;
    if (GetTickCount() >= g_status_until) {
        if (g_status_active) {
            g_status_active = 0;
            g_sub_cur = -2;          /* the subtitle line repaints next time it is due */
        }
        g_status_until = 0;
        return;
    }
    if (g_status_dirty) {
        text_paint(g_status_txt);
        g_status_dirty = 0;
    }
    g_status_active = 1;
    g_sub_shown = 1;
}

/* Per frame: picks the cue for the time, repaints when it changes. */
static void subtitle_frame(void)
{
    DWORD now = GetTickCount(), el;
    int i, idx = -1, playing;
    g_sub_shown = 0;
    /* The game's own "intro movie is playing" flag (byte_B5069C, set and cleared by the
     * movie state machine at sub_503970): the clock starts with the movie and the lines
     * stop the moment it ends or is skipped. No key starts them. */
    playing = (UINT_PTR)GetModuleHandleA(NULL) == SUB_IMAGE_BASE && *(const unsigned char *)(UINT_PTR)SUB_MOVIE_FLAG != 0;
    if (!playing) {
        g_sub_t0_set = 0;
        g_sub_cur = -1;
        return;
    }
    if (!g_sub_t0_set) {
        g_sub_t0 = now;
        g_sub_t0_set = 1;
        g_sub_cur = -1;
    }
    if (!g_sub_on || !g_sub_tex) return;
    el = now - g_sub_t0;
    for (i = 0; i < SUB_COUNT; i++) {
        if (el >= SUBS[i].start && el < SUBS[i].end) {
            idx = i;
            break;
        }
    }
    if (idx != g_sub_cur) {
        g_sub_cur = idx;
        if (idx >= 0) sub_paint(idx);
    }
    g_sub_shown = idx >= 0;
}

/* ---- Minimap (qol/minimap.odin): the field's collision grid, turned with the camera ----
 * Reads the GOG BOF4.exe globals at their fixed addresses (image base 0x400000, found
 * in IDA: sub_66EDC0 is the game's own attribute lookup). The map block is on the heap,
 * so every pointer taken from it is checked before use. Integer maths only. */
#define MM_SIZE 96
#define MM_R 48
#define MM_RIM_W 2
#define MM_AT_X 16
#define MM_AT_Y 16
#define MM_PX_CELL 3
#define MM_IMAGE_BASE 0x400000u
#define MM_GAME_MODE 0xBAB408u    /* u16: 2 field, 6 camera turn, 5 battle */
#define MM_CAMERA_YAW 0xBAD7C6u   /* u16, 0x1000 = full circle */
#define MM_MAP_SECTION 0xBAE302u  /* u8 added to the object's layer (sub_66EDC0) */
#define MM_MAP_BLOCK 0xBAE310u    /* pointer to the map block */
#define MM_FIELD_OBJ 0xB554C0u    /* three party objects, 0x10C bytes each */
/* The room's NPCs and objects (the save book among them): 32 slots of 0xD8 bytes, up to
 * 0xB52F40. First byte 0 = free slot; position at +0xC / +0x14 like the party objects. */
#define MM_NPCS 0xB51440u
#define MM_NPC_SIZE 0xD8u
#define MM_NPC_COUNT 32u
#define MM_COL_WALL 0xa0181c22u
#define MM_COL_FLOOR 0xc8687480u
#define MM_COL_STAIRS 0xc890a0b0u
#define MM_COL_EXIT 0xd8c09848u
#define MM_COL_RIM 0xe0d8d8d8u
#define MM_COL_NPC 0xffe06048u
#define MM_COL_LEADER 0xffffffffu
#define MM_COL_NORTH 0xffff5040u

static DWORD g_mm[MM_SIZE * MM_SIZE];
static void *g_mm_tex = NULL;
static int g_mm_shown = 0;    /* drawn this frame */

static int mm_okn(unsigned int p, unsigned int n) { return p >= 0x10000u && !IsBadReadPtr((const void *)(UINT_PTR)p, n); }
static int mm_ok(unsigned int p) { return mm_okn(p, 4); }
static unsigned int mm_r8(unsigned int p) { return *(const unsigned char *)(UINT_PTR)p; }
static unsigned int mm_r16(unsigned int p) { return mm_r8(p) | (mm_r8(p + 1) << 8); }
static unsigned int mm_r32(unsigned int p) { return mm_r16(p) | (mm_r16(p + 2) << 16); }

/* 1024 * sin(d), d in 0..90 whole degrees (Bhaskara I's approximation). */
static int sin1024_deg(int d)
{
    int num = 16 * d * (180 - d);
    int den = 5 * 180 * 180 - 4 * d * (180 - d);
    return den ? (num * 1024) / den : 0;
}

/* 1024 * sin and cos of an angle in whole degrees (any value). */
static int mm_sin(int deg)
{
    deg %= 360;
    if (deg < 0) deg += 360;
    if (deg <= 90) return sin1024_deg(deg);
    if (deg <= 180) return sin1024_deg(180 - deg);
    if (deg <= 270) return -sin1024_deg(deg - 180);
    return -sin1024_deg(360 - deg);
}
static int mm_cos(int deg) { return mm_sin(deg + 90); }

/* Grid of the leader's layer: host address of the attributes, size in cells. */
static int mm_grid(unsigned int layer, int *w, int *h, unsigned char **attrs)
{
    unsigned int blk = mm_r32(MM_MAP_BLOCK), desc, header, at;
    if (!mm_okn(blk, 0xc8) || layer >= mm_r8(blk + 3)) return 0;
    if (!mm_ok(blk + 0xc4 + 4 * layer)) return 0;
    desc = blk + mm_r32(blk + 0xc4 + 4 * layer);
    if (!mm_ok(desc)) return 0;
    if (mm_r8(desc) >= mm_r8(blk) || mm_r8(desc + 1) >= mm_r8(blk + 2)) return 0;   /* same bounds the game checks */
    if (!mm_ok(blk + 4 + 4 * mm_r8(desc)) || !mm_ok(blk + 0xa4 + 4 * mm_r8(desc + 1))) return 0;
    header = blk + mm_r32(blk + 4 + 4 * mm_r8(desc));
    at = blk + mm_r32(blk + 0xa4 + 4 * mm_r8(desc + 1));
    if (!mm_ok(header) || !mm_ok(at)) return 0;
    *w = (int)mm_r16(header) * 2;
    *h = (int)mm_r16(header + 2) * 2;
    if (*w <= 0 || *h <= 0 || *w > 512 || *h > 512 || !mm_okn(at, (unsigned int)(*w * *h))) return 0;
    *attrs = (unsigned char *)(UINT_PTR)at;
    return 1;
}

static int mm_attr(unsigned char *attrs, int w, int h, int x, int z)
{
    if (x < 0 || z < 0 || x >= w || z >= h || x > 0xff || z > 0xff) return 0x10;  /* outside reads as a wall */
    return attrs[z * w + x];
}

static DWORD mm_colour(int attr)
{
    switch ((attr >> 4) & 0xf) {
    case 0x0: return MM_COL_FLOOR;
    case 0x1: return MM_COL_WALL;
    case 0xa: return MM_COL_EXIT;
    case 0x7:
    case 0xc: return MM_COL_STAIRS;
    }
    return MM_COL_FLOOR;
}

static void mm_dot(int x, int y, int radius, DWORD c)
{
    int py, px;
    for (py = y - radius; py <= y + radius; py++)
        for (px = x - radius; px <= x + radius; px++)
            if (px >= 0 && py >= 0 && px < MM_SIZE && py < MM_SIZE) g_mm[py * MM_SIZE + px] = c;
}

/* Paints the minimap into g_mm. Returns 0 when the map block does not look loaded. */
static int mm_paint(void)
{
    unsigned int leader = MM_FIELD_OBJ;   /* the first of the three party objects is the one the player walks */
    unsigned int pos = leader + 0xc;
    int lxq, lzq, w = 0, h = 0, ok, yaw, cs, sn, u, v, i, k;
    unsigned char *attrs = NULL;
    int r = MM_R;

    lxq = (int)mm_r32(pos);
    lzq = (int)mm_r32(pos + 8);
    ok = mm_grid(mm_r8(MM_MAP_SECTION) + mm_r8(pos + 12), &w, &h, &attrs);
    yaw = (int)(mm_r16(MM_CAMERA_YAW) * 45 / 512);   /* 4096 = full circle, in degrees */
    cs = mm_cos(yaw);
    sn = mm_sin(yaw);

    for (v = 0; v < MM_SIZE; v++) {
        for (u = 0; u < MM_SIZE; u++) {
            int du = u - r, dv = v - r;
            int d2 = du * du + dv * dv;
            DWORD c = 0;
            if (d2 > r * r) {
                c = 0;
            } else if (d2 > (r - MM_RIM_W) * (r - MM_RIM_W)) {
                c = MM_COL_RIM;
            } else if (!ok) {
                c = MM_COL_WALL;
            } else {
                /* Offsets in 16.16 cells: pixels * 1/1024 (Q10 cos/sin) * 1/3 (cells per pixel). */
                int ox = ((du * cs - dv * sn) * 64) / MM_PX_CELL;
                int oz = ((du * sn + dv * cs) * 64) / MM_PX_CELL;
                int cx = (lxq + ox + 0x8000) >> 16;
                int cz = (lzq + oz + 0x8000) >> 16;
                c = mm_colour(mm_attr(attrs, w, h, cx, cz));
            }
            g_mm[v * MM_SIZE + u] = c;
        }
    }

    for (i = 0; i < MM_NPC_COUNT; i++) {
        unsigned int npc = MM_NPCS + i * MM_NPC_SIZE;
        int nxq, nzq, su, sv;
        if (!mm_ok(npc + 0x14) || mm_r8(npc) == 0) continue;
        nxq = (int)mm_r32(npc + 0xc) - lxq;
        nzq = (int)mm_r32(npc + 0x14) - lzq;
        su = (((nxq >> 6) * cs + (nzq >> 6) * sn) * MM_PX_CELL) / 1048576;
        sv = ((-(nxq >> 6) * sn + (nzq >> 6) * cs) * MM_PX_CELL) / 1048576;
        if (su * su + sv * sv < (r - MM_RIM_W - 3) * (r - MM_RIM_W - 3)) mm_dot(r + su, r + sv, 2, MM_COL_NPC);
    }

    /* Leader's arrow: a filled triangle at the centre pointing along its facing (8
     * directions, 45 degrees each), as the recomp draws it: 7 long, 4.5 half wide.
     * Coordinates in 1/128 pixel (sin and cos in 1/64, lengths doubled). */
    k = (int)(mm_r8(leader + 0x4c) & 7) * 45 - yaw;
    {
        int fx = mm_sin(k) / 16, fy = -mm_cos(k) / 16;
        int tx = fx * 14, ty = fy * 14;
        int ax = -fx * 8 - fy * 9, ay = -fy * 8 + fx * 9;
        int bx = -fx * 8 + fy * 9, by = -fy * 8 - fx * 9;
        int px, py;
        for (py = -8; py <= 8; py++) {
            for (px = -8; px <= 8; px++) {
                int qx = (2 * px + 1) * 64, qy = (2 * py + 1) * 64;
                int s1 = (ax - tx) * (qy - ty) - (ay - ty) * (qx - tx);
                int s2 = (bx - ax) * (qy - ay) - (by - ay) * (qx - ax);
                int s3 = (tx - bx) * (qy - by) - (ty - by) * (qx - bx);
                if ((s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0))
                    g_mm[(r + py) * MM_SIZE + (r + px)] = MM_COL_LEADER;
            }
        }
    }

    /* North: a red N near the rim, where -yaw points. */
    {
        static const unsigned char N_ROWS[7] = { 0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11 };   /* 5x7 */
        int nr = r - MM_RIM_W - 7;
        int nx = r + (mm_sin(-yaw) * nr) / 1024 - 2, ny = r - (mm_cos(-yaw) * nr) / 1024 - 3;
        int gx, gy;
        for (gy = 0; gy < 7; gy++)
            for (gx = 0; gx < 5; gx++)
                if (N_ROWS[gy] & (0x10 >> gx)) mm_dot(nx + gx, ny + gy, 0, MM_COL_NORTH);
    }
    return 1;
}

static void make_minimap_tex(void)
{
    void *tex = NULL;
    long hr = ((PFN_CreateTex)VT(g_dev)[23])(g_dev, MM_SIZE, MM_SIZE, 1, 0, 21, 1, &tex, NULL);
    if (hr >= 0 && tex) g_mm_tex = tex;
}

/* Per frame: decides whether the minimap shows, paints it and uploads it. */
static void minimap_frame(void)
{
    int mode, ok = 0, x, y;
    DWORD *dst;
    OV_LOCKED lr;
    g_mm_shown = 0;
    if (!g_mm_on || !g_mm_tex || g_journal_on) return;
    if ((UINT_PTR)GetModuleHandleA(NULL) != MM_IMAGE_BASE) return;   /* addresses are for this exe at its preferred base */
    mode = (int)mm_r16(MM_GAME_MODE);
    if (mode != 2 && mode != 6) return;       /* field, or the camera turn */
    ok = mm_paint();
    if (!ok) return;
    lr.Pitch = 0;
    lr.pBits = NULL;
    if (((PFN_LockRect)VT(g_mm_tex)[19])(g_mm_tex, 0, &lr, NULL, 0) < 0 || !lr.pBits) return;
    for (y = 0; y < MM_SIZE; y++) {
        dst = (DWORD *)((unsigned char *)lr.pBits + y * lr.Pitch);
        for (x = 0; x < MM_SIZE; x++) dst[x] = g_mm[y * MM_SIZE + x];
    }
    ((PFN_UnlockRect)VT(g_mm_tex)[20])(g_mm_tex, 0);
    g_mm_shown = 1;
}

/* Overlay quad over the frame. State is captured and restored. */
static void draw_overlay_d3d(void *sc)
{
    void *bb = NULL;
    OV_VP vp;
    void *dev = g_dev;
    void *sb = NULL;
    OV_V v[6];
    long hr_sb, hr_draw;
    char s[96];
    float fw = (float)TEX_W, fh = (float)TEX_H;

    hr_sb = ((PFN_CreateSB)VT(dev)[59])(dev, 1, &sb);   /* D3DSBT_ALL */
    if (hr_sb < 0 || !sb) {
        if (!g_draw_logged) { wsprintfA(s, "overlay CreateStateBlock hr=%08X", (unsigned int)hr_sb); logln(s); g_draw_logged = 1; }
        return;
    }
    if (((PFN_GetBackBufferSC)VT(sc)[5])(sc, 0, 0, &bb) < 0 || !bb) {
        ((PFN_Call0)VT(sb)[2])(sb);
        return;
    }
    /* Draw into the back buffer itself, with a viewport covering all of it. */
    ((PFN_SetRT)VT(dev)[37])(dev, 0, bb);
    vp.X = 0; vp.Y = 0; vp.Width = g_w; vp.Height = g_h; vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
    ((PFN_SetVP)VT(dev)[47])(dev, &vp);
    ((PFN_Call0)VT(dev)[41])(dev);                        /* BeginScene */
    /* The game (through the GOG wrapper) may leave shaders and fog on: the quad must
     * use the fixed pipeline, or it can come out white. */
    ((PFN_SetShader)VT(dev)[92])(dev, NULL);              /* SetVertexShader(NULL) */
    ((PFN_SetShader)VT(dev)[107])(dev, NULL);             /* SetPixelShader(NULL) */
    ((PFN_SetRS)VT(dev)[57])(dev, 28, 0);                 /* FOGENABLE = FALSE */
    ((PFN_SetRS)VT(dev)[57])(dev, 29, 0);                 /* SPECULARENABLE = FALSE */
    ((PFN_SetTSS)VT(dev)[67])(dev, 1, 4, 1);              /* stage 1 ALPHAOP = DISABLE */
    ((PFN_SetFVF)VT(dev)[89])(dev, FVF_OV);
    ((PFN_SetTex)VT(dev)[65])(dev, 0, g_tex);
    ((PFN_SetRS)VT(dev)[57])(dev, 27, 1);                 /* ALPHABLENDENABLE */
    ((PFN_SetRS)VT(dev)[57])(dev, 15, 1);                 /* ALPHATESTENABLE: alpha 0 texels are not written */
    ((PFN_SetRS)VT(dev)[57])(dev, 25, 5);                 /* ALPHAFUNC = GREATER */
    ((PFN_SetRS)VT(dev)[57])(dev, 24, 0);                 /* ALPHAREF = 0 */
    ((PFN_SetRS)VT(dev)[57])(dev, 19, 5);                 /* SRCBLEND = SRCALPHA */
    ((PFN_SetRS)VT(dev)[57])(dev, 20, 6);                 /* DESTBLEND = INVSRCALPHA */
    ((PFN_SetRS)VT(dev)[57])(dev, 7, 0);                  /* ZENABLE = FALSE */
    ((PFN_SetRS)VT(dev)[57])(dev, 137, 0);                /* LIGHTING = FALSE */
    ((PFN_SetRS)VT(dev)[57])(dev, 22, 1);                 /* CULLMODE = NONE */
    ((PFN_SetTSS)VT(dev)[67])(dev, 0, 1, 2);              /* COLOROP = SELECTARG1 */
    ((PFN_SetTSS)VT(dev)[67])(dev, 0, 2, 2);              /* COLORARG1 = TEXTURE */
    ((PFN_SetTSS)VT(dev)[67])(dev, 0, 4, 2);              /* ALPHAOP = SELECTARG1 */
    ((PFN_SetTSS)VT(dev)[67])(dev, 0, 5, 2);              /* ALPHAARG1 = TEXTURE */
    ((PFN_SetTSS)VT(dev)[67])(dev, 1, 1, 1);              /* stage 1 off */

    if (g_vig_on && g_vig_tex) {
        OV_V vv[6];
        float W = (float)g_w, H = (float)g_h;
        ((PFN_SetTex)VT(dev)[65])(dev, 0, g_vig_tex);
        set_v(&vv[0], 0.0f, 0.0f, 0.0f, 0.0f);
        set_v(&vv[1], W,    0.0f, 1.0f, 0.0f);
        set_v(&vv[2], 0.0f, H,    0.0f, 1.0f);
        set_v(&vv[3], W,    0.0f, 1.0f, 0.0f);
        set_v(&vv[4], W,    H,    1.0f, 1.0f);
        set_v(&vv[5], 0.0f, H,    0.0f, 1.0f);
        ((PFN_DrawUP)VT(dev)[83])(dev, 4, 2, vv, sizeof(OV_V));
        ((PFN_SetTex)VT(dev)[65])(dev, 0, g_tex);
    }
    if (g_sub_shown && g_sub_tex) {
        OV_V sv[6];
        float sx0 = (float)((int)g_w - SUB_W) / 2.0f;
        float sy0 = (float)((int)g_h - SUB_H - SUB_BOTTOM), sx1, sy1;
        sx1 = sx0 + (float)SUB_W;
        sy1 = sy0 + (float)SUB_H;
        ((PFN_SetTex)VT(dev)[65])(dev, 0, g_sub_tex);
        set_v(&sv[0], sx0, sy0, 0.0f, 0.0f);
        set_v(&sv[1], sx1, sy0, 1.0f, 0.0f);
        set_v(&sv[2], sx0, sy1, 0.0f, 1.0f);
        set_v(&sv[3], sx1, sy0, 1.0f, 0.0f);
        set_v(&sv[4], sx1, sy1, 1.0f, 1.0f);
        set_v(&sv[5], sx0, sy1, 0.0f, 1.0f);
        ((PFN_DrawUP)VT(dev)[83])(dev, 4, 2, sv, sizeof(OV_V));
        ((PFN_SetTex)VT(dev)[65])(dev, 0, g_tex);
    }
    if (g_mm_shown && g_mm_tex) {
        OV_V mv[6];
        /* 22% of the screen's height (238 pixels at 1080), at the top right of the game's
         * 4:3 picture, away from the game's compass at the top left. */
        int side = (int)g_h * 22 / 100, pic_w = (int)g_h * 4 / 3, pic_r, margin = (int)g_h / 54;
        float x0, y0, x1, y1;
        if (side < MM_SIZE) side = MM_SIZE;
        if (pic_w > (int)g_w) pic_w = (int)g_w;
        pic_r = ((int)g_w + pic_w) / 2;
        x1 = (float)(pic_r - margin);
        x0 = x1 - (float)side;
        y0 = (float)margin;
        y1 = y0 + (float)side;
        ((PFN_SetTex)VT(dev)[65])(dev, 0, g_mm_tex);
        set_v(&mv[0], x0, y0, 0.0f, 0.0f);
        set_v(&mv[1], x1, y0, 1.0f, 0.0f);
        set_v(&mv[2], x0, y1, 0.0f, 1.0f);
        set_v(&mv[3], x1, y0, 1.0f, 0.0f);
        set_v(&mv[4], x1, y1, 1.0f, 1.0f);
        set_v(&mv[5], x0, y1, 0.0f, 1.0f);
        ((PFN_DrawUP)VT(dev)[83])(dev, 4, 2, mv, sizeof(OV_V));
        ((PFN_SetTex)VT(dev)[65])(dev, 0, g_tex);
    }
    {
        /* Masters panel (and the notices) at the top left; the journal at the top right. */
        float ox = (g_journal_on && g_w > TEX_W) ? (float)(g_w - TEX_W) : 0.0f;
        set_v(&v[0], ox,      0.0f, 0.0f, 0.0f);
        set_v(&v[1], ox + fw, 0.0f, 1.0f, 0.0f);
        set_v(&v[2], ox,      fh,   0.0f, 1.0f);
        set_v(&v[3], ox + fw, 0.0f, 1.0f, 0.0f);
        set_v(&v[4], ox + fw, fh,   1.0f, 1.0f);
        set_v(&v[5], ox,      fh,   0.0f, 1.0f);
    }
    hr_draw = ((PFN_DrawUP)VT(dev)[83])(dev, 4, 2, v, sizeof(OV_V));   /* TRIANGLELIST */
    if (hr_draw < 0) {
        static int nfail = 0;
        if (nfail < 10) {
            nfail++;
            wsprintfA(s, "overlay DrawUP fail hr=%08X", (unsigned int)hr_draw);
            logln(s);
        }
    }
    ((PFN_Call0)VT(dev)[42])(dev);                        /* EndScene */
    if (!g_draw_logged) {
        DWORD ab = 0, sbv = 0, dbv = 0, zen = 0;
        char s4[160];
        ((PFN_GetRS)VT(dev)[58])(dev, 27, &ab);
        ((PFN_GetRS)VT(dev)[58])(dev, 19, &sbv);
        ((PFN_GetRS)VT(dev)[58])(dev, 20, &dbv);
        ((PFN_GetRS)VT(dev)[58])(dev, 7, &zen);
        wsprintfA(s4, "overlay state: alphablend=%u src=%u dst=%u zenable=%u", ab, sbv, dbv, zen);
        logln(s4);
    }
    ((PFN_Call0)VT(sb)[5])(sb);                           /* StateBlock::Apply */
    ((PFN_Call0)VT(sb)[2])(sb);                           /* Release */
    ((PFN_Release)VT(bb)[2])(bb);                         /* back buffer ref */

    if (!g_draw_logged) {
        wsprintfA(s, "overlay draw hr=%08X sb=%08X", (unsigned int)hr_draw, (unsigned int)hr_sb);
        logln(s);
        g_draw_logged = 1;
    }
}

/* ---- Right analog stick through XInput (Windows' pad API) ----
 * The game's pad setting has no stick, so the DLL reads it itself. Field only: the
 * right stick X turns the camera yaw (0x1000 = a full turn). */
typedef struct { unsigned short wButtons; unsigned char bLT, bRT; short sLX, sLY, sRX, sRY; } XI_GAMEPAD;
typedef struct { unsigned long dwPacket; XI_GAMEPAD gp; } XI_STATE;
typedef unsigned long (__stdcall *PFN_XIGetState)(unsigned long, XI_STATE *);

#define XI_DEAD 7000
#define XI_RATE 32           /* camera units per frame at full stick */
static PFN_XIGetState g_xi_get = NULL;
static int g_xi_tried = 0;
static int g_stick_turn = 0;      /* set by stick_frame: the right stick turned the camera this frame */
static int g_xi_log_n = 0;
#define ZP_XI_LT 0x10000u    /* zoom-in edge from the left trigger */
#define ZP_XI_RT 0x20000u    /* zoom-out edge from the right trigger */
static unsigned int zp_xi_edge = 0;
static int g_lt_prev = 0, g_rt_prev = 0;

static void xi_init(void)
{
    HMODULE m;
    g_xi_tried = 1;
    m = LoadLibraryA("xinput1_4.dll");
    if (!m) m = LoadLibraryA("xinput1_3.dll");
    if (!m) return;
    g_xi_get = (PFN_XIGetState)GetProcAddress(m, "XInputGetState");
}

static void stick_frame(void)
{
    XI_STATE st;
    int rx, ry, delta;
    unsigned short yaw;
    unsigned short mode;
    if ((UINT_PTR)GetModuleHandleA(NULL) != MM_IMAGE_BASE) return;
    if (!g_xi_tried) xi_init();
    if (!g_xi_get) return;
    if (g_xi_get(0, &st) != 0) return;           /* pad 0 not connected */
    {   /* triggers: a press (edge) is a zoom step, read here, not through the game's pad */
        int lt = st.gp.bLT > 128, rt = st.gp.bRT > 128;
        zp_xi_edge = 0;
        if (lt && !g_lt_prev) zp_xi_edge |= ZP_XI_LT;
        if (rt && !g_rt_prev) zp_xi_edge |= ZP_XI_RT;
        g_lt_prev = lt;
        g_rt_prev = rt;
    }
    rx = st.gp.sRX;
    ry = st.gp.sRY;
    mode = *(const unsigned short *)MM_GAME_MODE;
    g_stick_turn = (rx <= -XI_DEAD || rx >= XI_DEAD);   /* a manual turn this frame */
    if (mode != 2) return;
    if (rx > -XI_DEAD && rx < XI_DEAD) return;
    delta = rx * XI_RATE / 32768;
    if (delta == 0) delta = rx > 0 ? 1 : -1;
    yaw = *(const unsigned short *)MM_CAMERA_YAW;
    yaw = (unsigned short)((yaw + (unsigned short)delta) & 0x0FFF);
    *(unsigned short *)MM_CAMERA_YAW = yaw;
    if ((g_xi_log_n++ % 30) == 0) {
        char s[96];
        wsprintfA(s, "stick rx=%d ry=%d yaw=%04X", (int)rx, (int)ry, (unsigned)yaw);
        logln(s);
    }
}

/* ---- Mouse wheel zoom: the game window's messages are read in a wrapper.
 * Wheel up = zoom in, wheel down = zoom out (one step per notch). ---- */
#define WM_MOUSEWHEEL_ 0x020A
static WNDPROC g_wnd_orig = NULL;
static volatile unsigned int g_wheel_edge = 0;

static LRESULT CALLBACK wheel_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_MOUSEWHEEL_) {
        short d = (short)HIWORD(w);
        char s[64];
        wsprintfA(s, "wheel msg delta=%d", (int)d);
        logln(s);
        if (d > 0) g_wheel_edge |= ZP_XI_LT;
        else if (d < 0) g_wheel_edge |= ZP_XI_RT;
    }
    return CallWindowProcA(g_wnd_orig, h, m, w, l);
}

static HWND g_game_hwnd = NULL;   /* the game window, from CreateDevice (Present's own HWND is usually NULL) */

static void wheel_hook(HWND h)
{
    if (g_wnd_orig || !h) return;
    g_wnd_orig = (WNDPROC)SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)wheel_proc);
    {
        char s[80];
        wsprintfA(s, "wheel hook set hwnd=%p orig=%p", (void *)h, (void *)g_wnd_orig);
        logln(s);
    }
}

/* ---- L1/R1 zoom on the field (third person) ----
 * L1 zooms in one step, R1 zooms out one step (edges of the pad word). Zoom in:
 * the camera distance shrinks and the three party sprites grow (flag 2, the
 * stored scale, which the game does not recompute). Step 0 gives the game's own
 * values back (flag 1). Only in field mode (2). */
#define ZP_PAD 0xB5064Au     /* u32 pad word; edges this frame (Xbox pad): 0x0400 = LB, 0x0800 = RB */
#define ZP_L1 0x0400u        /* LB: zoom in */
#define ZP_R1 0x0800u        /* RB: zoom out */
#define ZP_DIST 0xBAD7D4u    /* s32 camera distance */
#define ZP_OBJ 0xB554C0u     /* party objects, 0x10C bytes each */
#define ZP_STRIDE 0x10Cu
#define ZP_FLAG 0x73u        /* sprite scale flag */
#define ZP_SX 0x74u          /* scale X, 16.16 */
#define ZP_SY 0x78u          /* scale Y, 16.16 */
#define ZP_MAX 6             /* steps; each step is 15 percent */

static unsigned int zp_prev_pad = 0;
static int zp_step = 0, zp_on = 0, zp_base_dist = 0, zp_base_near = 0;
#define ZP_NEAR 0xBAD7C0u    /* s32 camera near plane */
#define ZP_NEAR_MIN 256
static unsigned short zp_yaw = 0;
static int zp_yaw_on = 0;

static void zoom_frame(void)
{
    unsigned int pad, edge;
    unsigned short mode;
    int i, pct, dist;
    if ((UINT_PTR)GetModuleHandleA(NULL) != MM_IMAGE_BASE) return;
    mode = *(const unsigned short *)MM_GAME_MODE;
    pad = *(const unsigned int *)ZP_PAD;
    /* zoom only from the triggers (LT in, RT out); LB/RB no longer zoom */
    edge = zp_xi_edge | g_wheel_edge;
    g_wheel_edge = 0;
    zp_xi_edge = 0;
    zp_prev_pad = pad;
    if (edge & (ZP_L1 | ZP_R1)) {        /* debug: each zoom button edge, with the mode seen */
        char s[80];
        wsprintfA(s, "zoom edge pad=%08X mode=%u step=%d", pad, (unsigned)mode, zp_step);
        logln(s);
    }
    /* field (2) and camera turn (6, which LB/RB start); battle and menus untouched.
     * The game's mode is not changed here: forcing it back to the field left the
     * game ignoring its buttons. */
    if (mode != 2 && mode != 6) return;
    if ((edge & (ZP_L1 | ZP_XI_LT)) && zp_step < ZP_MAX) zp_step++;
    if ((edge & (ZP_R1 | ZP_XI_RT)) && zp_step > 0) zp_step--;
    if (zp_step == 0 && !zp_on) return;
    if (!zp_on) {
        zp_base_dist = *(const int *)ZP_DIST;
        zp_base_near = *(const int *)ZP_NEAR;
        zp_on = 1;
    }
    pct = 100 + 15 * zp_step;            /* 100 = the game's own view */
    dist = zp_base_dist * 100 / pct;
    *(int *)ZP_DIST = dist;
    /* near plane follows the zoom, so scenery between camera and party (table legs)
     * is not cut away when close; back to the game's own value at step 0 */
    {
        int nr = zp_base_near * 100 / pct;
        if (nr < ZP_NEAR_MIN) nr = ZP_NEAR_MIN;
        *(int *)ZP_NEAR = zp_step == 0 ? zp_base_near : nr;
    }
    for (i = 0; i < 3; i++) {
        unsigned char *o = (unsigned char *)(ZP_OBJ + i * ZP_STRIDE);
        if (zp_step == 0) {
            o[ZP_FLAG] = 1;              /* back to the game's own projection */
        } else {
            o[ZP_FLAG] = 2;
            *(int *)(o + ZP_SX) = pct * 65536 / 100;
            *(int *)(o + ZP_SY) = pct * 65536 / 100;
        }
    }
    if (zp_step == 0) zp_on = 0;
}

static void cap_frame(void);

/* ---- Camera follows the leader (recomp's third_person_follow). After FL_IDLE
 * frames without a manual turn (right stick or LB/RB), the yaw swings toward the way
 * the leader walks, eased (about 0.8 s); not when the walk points more than FL_REACH
 * off the yaw (walking toward the camera). Field only. F7 turns it on and off. ---- */
#define FL_IDLE 60            /* frames without a manual turn before the follow starts */
#define FL_MIN_STEP 0x1000    /* smallest step (16.16) that counts as walking */
#define FL_REACH 0x600        /* yaw units: beyond this the camera does not swing */
#define FL_RATE 32            /* ease: 1/32 of the gap per frame (about 0.5 s) */
#define FL_POS_X 0xC          /* leader x (16.16) in MM_FIELD_OBJ */
#define FL_POS_Z 0x14         /* leader z (16.16) */
static int g_fl_on = 1;
static int g_fl_init = 0, g_fl_idle = 0, g_fl_acc = 0;
static int g_fl_px = 0, g_fl_pz = 0;     /* reference point for the walk direction */
static int g_fl_want = 0, g_fl_moving = 0;
#define FL_HOLD 20                       /* frames the camera keeps easing after a step */

/* atan without the C library: polynomial on [-1, 1], then the quadrant */
static double fl_atan(double x)
{
    double x2 = x * x;
    return x * (0.9998660 - x2 * (0.3302995 - x2 * (0.1801410 - x2 * (0.0851330 - x2 * 0.0208351))));
}

static double fl_atan2(double y, double x)
{
    const double PI = 3.14159265358979;
    double ay = y < 0 ? -y : y, ax = x < 0 ? -x : x;
    if (x == 0 && y == 0) return 0;
    if (ax >= ay) {
        double a = fl_atan(y / x);          /* |y/x| <= 1 */
        if (x < 0) a += (y >= 0 ? PI : -PI);
        return a;
    }
    {
        double a = (y >= 0 ? PI / 2 : -PI / 2) - fl_atan(x / y);
        return a;
    }
}

#include "walk_stick.h"            /* left stick walks at any angle (F6 toggles) */
#include "fade_speed.h"            /* normal fades limited, door fades quickened (F5 toggles) */
#include "autosave_slot2.h"        /* autosave to slot 2 on each field map, 3 backups (F4 toggles) */
#include "trace_hooks.h"           /* log only: candidate Setting screen routines (no behaviour change) */
#include "arm_probe.h"             /* log only: camera spring-arm probe (F3 toggles, off by default) */

static void follow_frame(int manual_turn)
{
    unsigned short mode;
    int x, z, dx, dz, gap, yaw, dl;
    double want;
    if ((UINT_PTR)GetModuleHandleA(NULL) != MM_IMAGE_BASE) return;
    mode = *(const unsigned short *)MM_GAME_MODE;
    x = *(const int *)(MM_FIELD_OBJ + FL_POS_X);
    z = *(const int *)(MM_FIELD_OBJ + FL_POS_Z);
    if (!g_fl_init) { g_fl_init = 1; g_fl_px = x; g_fl_pz = z; return; }
    if (manual_turn) g_fl_idle = 0;
    else if (g_fl_idle < 1000000) g_fl_idle++;
    /* the direction is taken from the walk since the last reference point, summed
     * over frames, so slow walking counts (as the recomp does per field tick) */
    dx = x - g_fl_px;
    dz = z - g_fl_pz;
    if ((double)dx * dx + (double)dz * dz >= (double)FL_MIN_STEP * FL_MIN_STEP) {
        want = fl_atan2((double)dx, -(double)dz) * 4096.0 / (2.0 * 3.14159265358979);
        g_fl_want = (int)(want < 0 ? want - 0.5 : want + 0.5);
        g_fl_px = x;
        g_fl_pz = z;
        g_fl_moving = FL_HOLD;
    }
    if (!g_fl_on || mode != 2) return;
    if (g_fl_moving <= 0 || g_fl_idle < FL_IDLE) return;
    g_fl_moving--;
    yaw = (int)*(const unsigned short *)MM_CAMERA_YAW;
    gap = (g_fl_want - yaw + 0x800 + 0x2000) % 0x1000 - 0x800;
    if (gap > FL_REACH || gap < -FL_REACH) return;
    /* eased step, kept in 1/256 units, whole units applied */
    g_fl_acc += gap * 256 / FL_RATE;
    dl = g_fl_acc / 256;
    g_fl_acc -= dl * 256;
    if (dl) {
        yaw = (yaw + dl) & 0x0FFF;
        *(unsigned short *)MM_CAMERA_YAW = (unsigned short)yaw;
    }
}

/* IDirect3DSwapChain9::Present (slot 3). Overlay drawn on the back buffer. */
static long __stdcall my_sc_present(void *sc, const void *a, const void *b, HWND h, const void *c, DWORD flags)
{
    LONG n = InterlockedIncrement(&g_sc_frames);
    if (n == 1 || n % 300 == 0) {
        char s[64];
        wsprintfA(s, "SwapChain::Present #%ld", (long)n);
        logln(s);
    }
    {
        long rc;
#ifndef NO_DRAW
        if (g_dev) {
            if (!g_tex && !g_tex_tried) { g_tex_tried = 1; build_overlay_tex(); }
            if (g_tex) {
                wheel_hook(g_game_hwnd);
                stick_frame();
                walk_frame();
                zoom_frame();
                arm_probe_frame();     /* log only */
                if (GetAsyncKeyState(VK_F3) & 1) {
                    g_ap_on = !g_ap_on;
                    logln(g_ap_on ? "arm probe ON" : "arm probe OFF");
                    status_show(g_ap_on ? "Camera probe: ON" : "Camera probe: OFF");
                }
                cap_frame();
                if (GetAsyncKeyState(VK_F7) & 1) {
                    g_fl_on = !g_fl_on;
                    logln(g_fl_on ? "camera follow ON" : "camera follow OFF");
                }
                if (GetAsyncKeyState(VK_F6) & 1) {
                    g_wk_on = !g_wk_on;
                    logln(g_wk_on ? "walk stick ON" : "walk stick OFF");
                    status_show(g_wk_on ? "Analog stick walking: ON" : "Analog stick walking: OFF");
                }
                if (GetAsyncKeyState(VK_F4) & 1) {
                    g_as_enabled = !g_as_enabled;
                    logln(g_as_enabled ? "autosave slot 2 ON" : "autosave slot 2 OFF");
                    status_show(g_as_enabled ? "Autosave slot 2: ON" : "Autosave slot 2: OFF");
                }
                if (GetAsyncKeyState(VK_F5) & 1) {
                    g_fs_enabled = !g_fs_enabled;
                    logln(g_fs_enabled ? "fade speed ON" : "fade speed OFF");
                    status_show(g_fs_enabled ? "Smooth fades: ON" : "Smooth fades: OFF");
                }
                if (GetAsyncKeyState(VK_F8) & 1) {
                    g_enc_off = !g_enc_off;
                    logln(g_enc_off ? "random encounters OFF" : "random encounters ON");
                    status_show(g_enc_off ? "Random battles: OFF" : "Random battles: ON");
                }
                {
                    unsigned short gm = *(const unsigned short *)MM_GAME_MODE;
                    follow_frame(g_stick_turn || gm == 6);   /* LB/RB also count as a manual turn */
                }
                masters_frame();
                flaglog_frame();
                minimap_frame();
                subtitle_frame();
                status_frame();
                draw_overlay_d3d(sc);
            }
        }
#endif
        rc = g_real_sc_present(sc, a, b, h, c, flags);
        return rc;
    }
}

/* ---- Draw-call inventory (F11 starts and stops it). Logs, for each texture, primitive
 * type and vertex/primitive count, how many draws happened while it ran. Read-only:
 * the calls themselves pass through unchanged. ---- */
typedef long (__stdcall *PFN_DIP)(void *, unsigned int, int, unsigned int, unsigned int, unsigned int, unsigned int);
typedef long (__stdcall *PFN_DP)(void *, unsigned int, unsigned int, unsigned int);
typedef long (__stdcall *PFN_ST)(void *, unsigned int, void *);
static PFN_DIP g_real_dip = NULL;
static PFN_DP g_real_dp = NULL;
static PFN_ST g_real_st = NULL;
static void *g_cur_tex = NULL;
static volatile int g_cap = 0;
static int g_cap_frames = 0;
#define CAP_ROWS 256
typedef struct { void *tex; unsigned int type, nv, pc, kind; unsigned long n; } CAP_ROW;
static CAP_ROW g_cap_rows[CAP_ROWS];
static int g_cap_nrows = 0;

static void cap_record(unsigned int type, unsigned int nv, unsigned int pc, unsigned int kind)
{
    int i;
    for (i = 0; i < g_cap_nrows; i++) {
        CAP_ROW *r = &g_cap_rows[i];
        if (r->tex == g_cur_tex && r->type == type && r->nv == nv && r->pc == pc && r->kind == kind) {
            r->n++;
            return;
        }
    }
    if (g_cap_nrows < CAP_ROWS) {
        CAP_ROW *r = &g_cap_rows[g_cap_nrows++];
        r->tex = g_cur_tex; r->type = type; r->nv = nv; r->pc = pc; r->kind = kind; r->n = 1;
    }
}

static unsigned long g_tot[5];      /* all calls since start: st, dip, dp, dpup, dipup */
static void *g_last_dev = NULL;     /* the device whose calls the hooks saw last */

static long __stdcall my_st(void *self, unsigned int stage, void *tex)
{
    g_tot[0]++;
    if (stage == 0) g_cur_tex = tex;
    return g_real_st(self, stage, tex);
}

static long __stdcall my_dip(void *self, unsigned int type, int bv, unsigned int mv,
                             unsigned int nv, unsigned int si, unsigned int pc)
{
    g_tot[1]++; g_last_dev = self;
    if (g_cap) cap_record(type, nv, pc, 1);
    return g_real_dip(self, type, bv, mv, nv, si, pc);
}

static long __stdcall my_dp(void *self, unsigned int type, unsigned int sv, unsigned int pc)
{
    g_tot[2]++; g_last_dev = self;
    if (g_cap) cap_record(type, sv, pc, 0);
    return g_real_dp(self, type, sv, pc);
}

/* user-pointer variants: DrawPrimitiveUP (83), DrawIndexedPrimitiveUP (84) */
typedef long (__stdcall *PFN_DPUP)(void *, unsigned int, unsigned int, void *, unsigned int);
typedef long (__stdcall *PFN_DIPUP)(void *, unsigned int, unsigned int, unsigned int, unsigned int,
                                    void *, unsigned int, void *, unsigned int);
static PFN_DPUP g_real_dpup = NULL;
static PFN_DIPUP g_real_dipup = NULL;

static long __stdcall my_dpup(void *self, unsigned int type, unsigned int pc, void *data, unsigned int stride)
{
    g_tot[3]++; g_last_dev = self;
    if (g_cap) cap_record(type, pc, pc, 2);
    return g_real_dpup(self, type, pc, data, stride);
}

static long __stdcall my_dipup(void *self, unsigned int type, unsigned int mv, unsigned int nv,
                               unsigned int pc, void *idx, unsigned int fmt, void *vtx, unsigned int stride)
{
    g_tot[4]++; g_last_dev = self;
    if (g_cap) cap_record(type, nv, pc, 3);
    return g_real_dipup(self, type, mv, nv, pc, idx, fmt, vtx, stride);
}

/* Called once per frame: F11 toggles the capture and writes the table to the log. */
static void cap_frame(void)
{
    int f11 = (GetAsyncKeyState(VK_F11) & 1) != 0;
    int i;
    char s[160];
    if (!f11) return;
    if (!g_cap) {
        g_cap_nrows = 0;
        g_cap_frames = 0;
        g_cap = 1;
        logln("capture ON (F11 again to stop)");
        return;
    }
    g_cap = 0;
    wsprintfA(s, "capture OFF: %d kinds of draw", g_cap_nrows);
    logln(s);
    wsprintfA(s, "totals since start: st=%lu dip=%lu dp=%lu dpup=%lu dipup=%lu last dev=%p",
              g_tot[0], g_tot[1], g_tot[2], g_tot[3], g_tot[4], g_last_dev);
    logln(s);
    for (i = 0; i < g_cap_nrows; i++) {
        CAP_ROW *r = &g_cap_rows[i];
        wsprintfA(s, "draw tex=%p type=%u %s nv=%u pc=%u n=%lu",
                  r->tex, r->type, r->kind ? "indexed" : "plain", r->nv, r->pc, r->n);
        logln(s);
    }
}

static long __stdcall my_createdev(void *d3d, UINT adapter, int type, HWND hwnd,
                                   DWORD flags, void *pp, void **ret)
{
    char s[160];
    long hr = g_real_createdev(d3d, adapter, type, hwnd, flags, pp, ret);
    if (hwnd) g_game_hwnd = hwnd;
    if (hr >= 0 && ret && *ret) {
        char dv[96];
        wsprintfA(dv, "device created: obj=%p vtable=%p", *ret, (void *)VT(*ret));
        logln(dv);
    }
    if (pp) {
        unsigned int *p = (unsigned int *)pp;
        wsprintfA(s, "params: %ux%u fmt=%u count=%u swap=%u windowed=%u",
                  p[0], p[1], p[2], p[3], p[6], p[8]);
        logln(s);
    }
    wsprintfA(s, "CreateDevice hr=%08X", (unsigned int)hr);
    logln(s);
    if (hr >= 0) sa_install(logln);   /* save anywhere: checks the EXE bytes before patching */
    if (hr >= 0) enc_install(logln);  /* no encounters: checks the three call sites first */
    if (hr >= 0) walk_install(logln); /* left stick walk: checks the three call sites first */
    if (hr >= 0) fs_install(logln);   /* fade speed: checks the call site first */
    if (hr >= 0) as_install(logln);   /* autosave slot 2: checks the call site first */
    if (hr >= 0) tr_install(logln);   /* trace: log only, checks the call sites first */
    if (hr >= 0 && ret && *ret && pp) {
        g_dev = *ret;
        g_w = ((unsigned int *)pp)[0];
        g_h = ((unsigned int *)pp)[1];
    }
    if (hr >= 0 && ret && *ret && InterlockedCompareExchange(&g_vt_dev_patched, 1, 0) == 0) {
        void *sc = NULL;
        if (((PFN_GetSwapChain)VT(*ret)[14])(*ret, 0, &sc) >= 0 && sc
            && InterlockedCompareExchange(&g_sc_patched, 1, 0) == 0) {
            if (patch_slot(VT(sc), 3, (void *)my_sc_present, (void **)&g_real_sc_present))
                logln("hooked IDirect3DSwapChain9::Present (slot 3)");
            /* device slots: SetTexture 65, DrawPrimitive 81, DrawIndexedPrimitive 82 */
            if (patch_slot(VT(*ret), 65, (void *)my_st, (void **)&g_real_st)
                && patch_slot(VT(*ret), 81, (void *)my_dp, (void **)&g_real_dp)
                && patch_slot(VT(*ret), 82, (void *)my_dip, (void **)&g_real_dip))
                logln("hooked device SetTexture/DrawPrimitive/DrawIndexedPrimitive");
            if (patch_slot(VT(*ret), 83, (void *)my_dpup, (void **)&g_real_dpup)
                && patch_slot(VT(*ret), 84, (void *)my_dipup, (void **)&g_real_dipup))
                logln("hooked device DrawPrimitiveUP/DrawIndexedPrimitiveUP");
        } else {
            logln("GetSwapChain failed");
        }
    }
    return hr;
}

__declspec(dllexport) void *WINAPI my_d3dc9(UINT sdk)
{
    void *d3d;
    if (!g_real_create9) {
        char path[MAX_PATH];
        HMODULE m;
        GetSystemDirectoryA(path, MAX_PATH);
        lstrcatA(path, "\\d3d9.dll");
        m = LoadLibraryA(path);
        if (m) g_real_create9 = (PFN_Create9)GetProcAddress(m, "Direct3DCreate9");
        logln(g_real_create9 ? "real d3d9 loaded" : "ERROR real d3d9 not found");
    }
    if (!g_real_create9) return NULL;
    d3d = g_real_create9(sdk);
    if (d3d && InterlockedCompareExchange(&g_vt_d3d_patched, 1, 0) == 0) {
        if (patch_slot(VT(d3d), 16, (void *)my_createdev, (void **)&g_real_createdev))
            logln("hooked IDirect3D9::CreateDevice (slot 16)");
    }
    return d3d;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DeleteFileA("d3d9_proxy.log");
        logln("d3d9_proxy loaded");
    }
    return TRUE;
}
