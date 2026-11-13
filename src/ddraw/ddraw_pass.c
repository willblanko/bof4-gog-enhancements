/* ddraw.dll pass-through for BOF4 GOG (the real DirectDraw is in ddraw_gog.dll).
   Only forwards the exports. No overlay, no hook, no keys, no game-memory writes.
   Replaces the old proxy that drew the F8 "MESTRES" panel. No CRT. */

typedef unsigned int u32;
typedef void *HANDLE_T;

__declspec(dllimport) void *__stdcall LoadLibraryA(const char *name);
__declspec(dllimport) void *__stdcall GetProcAddress(void *h, const char *name);

static const char *g_names[14] = {
    "AcquireDDThreadLock", "CompleteCreateSysmemSurface", "D3DParseUnknownCommand",
    "DDInternalLock", "DDInternalUnlock", "DirectDrawCreate", "DirectDrawCreateClipper",
    "DirectDrawCreateEx", "DirectDrawEnumerateA", "DirectDrawEnumerateExA",
    "DirectDrawEnumerateExW", "DirectDrawEnumerateW", "DllGetClassObject", "ReleaseDDThreadLock"
};
void *g_real[14];

/* Each export jumps through the real pointer. Stack and arguments are untouched. */
#define TRAMP(i) __declspec(naked) void tramp_##i(void) { __asm { jmp dword ptr [g_real + i*4] } }
TRAMP(0) TRAMP(1) TRAMP(2) TRAMP(3) TRAMP(4) TRAMP(5) TRAMP(6)
TRAMP(7) TRAMP(8) TRAMP(9) TRAMP(10) TRAMP(11) TRAMP(12) TRAMP(13)

int __stdcall proxy_entry(void *hinst, unsigned long reason, void *reserved)
{
    (void)hinst; (void)reserved;
    if (reason == 1) { /* DLL_PROCESS_ATTACH */
        void *h = LoadLibraryA("ddraw_gog.dll");
        int i;
        if (!h) return 0;
        for (i = 0; i < 14; i++) g_real[i] = GetProcAddress(h, g_names[i]);
    }
    return 1;
}
