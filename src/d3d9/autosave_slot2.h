/* Autosave on entering a field map, into save slot 2 (SAV\02.DAT) for the GOG PC
 * build of BOF4 (BOF4.exe, 1.0 GOG, image base 0x400000).
 *
 * Each map change in field mode (game mode 1) runs the map load sub_676CF0. The
 * call site is 0x521562; the stub calls the load, then, once the map is in place,
 * writes slot 2 with the game's own save writer sub_52A5E0. Before each write the
 * current 02.DAT is copied to the backup files (see below), so the last three
 * versions are kept:
 *
 *   SAV\02_bak1.DAT   the file before the last autosave
 *   SAV\02_bak2.DAT   the one before that
 *   SAV\02_bak3.DAT   the oldest of the three; older copies are deleted
 *
 * F4 turns autosave on or off (on by default). A status line shows the state.
 *
 * The writer is the one the camp menu's Save uses (sub_52A5E0 -> sub_52A760 ->
 * sub_52A800). It takes a record whose byte at +0x12 is the record index (0 here)
 * and whose byte at +0x16 is the slot index minus one (1 for slot 2). The record
 * lives in a static buffer here, not in the game's menu state.
 *
 * Premises checked on this EXE (static, IDA, BOF4_copy.exe):
 *   - 0x521562 holds E8 rel32 to sub_676CF0 (no arguments; the next instruction
 *     is at 0x521567);
 *   - sub_676CF0 starts with push ecx (0x51);
 *   - sub_52A5E0 starts with push esi (0x56) and takes one argument, the record;
 *   - sub_52A760 writes "SAV\%02d.DAT" with the record's slot byte plus one.
 * The install checks all of them before patching the call site.
 */
#ifndef AUTOSAVE_SLOT2_H
#define AUTOSAVE_SLOT2_H

#define AS_MODULE_BASE   0x400000u
#define AS_SITE          0x521562u   /* call sub_676CF0 in the field map-load case */
#define AS_MAP_LOAD      0x676CF0u   /* sub_676CF0: loads the map (no arguments) */
#define AS_SAVE_WRITE    0x52A5E0u   /* sub_52A5E0: builds the image and writes the slot */
#define AS_KEEP          3           /* backups kept */

/* 1 = autosave on (the default); 0 = off. */
static volatile int g_as_enabled = 1;
static int g_as_installed = 0;
static unsigned char g_as_rec[0x200];   /* fake record for the writer, zeroed */

/* Copies SAV\02.DAT to the backup chain, dropping the oldest. Missing files are
 * skipped, so the first autosave simply makes no backup. */
static void as_backup_slot2(void)
{
    char cur[32], b1[32], b2[32], b3[32];
    lstrcpyA(cur, "SAV\\02.DAT");
    lstrcpyA(b1, "SAV\\02_bak1.DAT");
    lstrcpyA(b2, "SAV\\02_bak2.DAT");
    lstrcpyA(b3, "SAV\\02_bak3.DAT");
    if (GetFileAttributesA(cur) == INVALID_FILE_ATTRIBUTES) return;
    DeleteFileA(b3);
    MoveFileExA(b2, b3, MOVEFILE_REPLACE_EXISTING);
    MoveFileExA(b1, b2, MOVEFILE_REPLACE_EXISTING);
    CopyFileA(cur, b1, FALSE);
}

/* Called from the stub after the map has loaded. */
static void as_autosave(void)
{
    typedef void (__cdecl *SaveFn)(void *);
    SaveFn save = (SaveFn)(UINT_PTR)AS_SAVE_WRITE;

    as_backup_slot2();
    g_as_rec[0x12] = 0;   /* record index */
    g_as_rec[0x16] = 1;   /* slot index: file 02 */
    save(g_as_rec);
}

/* Naked stub, entered by the patched call (no arguments). It runs the map load,
 * then autosave when enabled. All registers are kept, so the caller sees the
 * same state as the original call. */
__declspec(naked) static void as_stub(void)
{
    __asm {
        pushad
        mov  eax, AS_MAP_LOAD
        call eax                    /* the map load, as the game called it */
        popad
        pushad
        cmp  g_as_enabled, 0
        je   as_skip
        call as_autosave
    as_skip:
        popad
        ret
    }
}

/* True when the site is the game's own call to the map load, and the two
 * functions it depends on start as expected. */
static int as_site_is_game_call(unsigned int site)
{
    const unsigned char *p = (const unsigned char *)(UINT_PTR)site;
    const unsigned char *load = (const unsigned char *)(UINT_PTR)AS_MAP_LOAD;
    const unsigned char *save = (const unsigned char *)(UINT_PTR)AS_SAVE_WRITE;
    unsigned int target;
    if (IsBadReadPtr(p, 5) || IsBadReadPtr(load, 1) || IsBadReadPtr(save, 1)) return 0;
    if (p[0] != 0xE8) return 0;
    target = site + 5 + (unsigned int)p[1] + ((unsigned int)p[2] << 8)
             + ((unsigned int)p[3] << 16) + ((unsigned int)p[4] << 24);
    return target == AS_MAP_LOAD && load[0] == 0x51 && save[0] == 0x56;
}

/* Install once. Safe to call repeatedly. Checks the module base and the call
 * site before writing the five bytes. */
static void as_install(void (*log)(const char *))
{
    unsigned char *p = (unsigned char *)(UINT_PTR)AS_SITE;
    unsigned int rel_target = (unsigned int)(UINT_PTR)as_stub - (AS_SITE + 5);
    DWORD old;
    int i;
    char s[160];
    unsigned char want[5];

    if (g_as_installed) return;
    g_as_installed = 1;
    if ((unsigned int)(UINT_PTR)GetModuleHandleA(NULL) != AS_MODULE_BASE) {
        wsprintfA(s, "autosave slot 2: module base is not %08X; not installed", AS_MODULE_BASE);
        if (log) log(s);
        return;
    }
    if (!as_site_is_game_call(AS_SITE)) {
        if (log) log("autosave slot 2: expected call to sub_676CF0 not found; not installed");
        return;
    }
    want[0] = 0xE8;
    for (i = 0; i < 4; i++) want[1 + i] = (unsigned char)(rel_target >> (8 * i));
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    for (i = 0; i < 5; i++) p[i] = want[i];
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    wsprintfA(s, "autosave slot 2: site %08X redirected to as_stub", AS_SITE);
    if (log) log(s);
}

#endif
