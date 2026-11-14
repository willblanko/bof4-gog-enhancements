/* Spring-arm probe for the third-person camera (item 4). LOG ONLY: it changes no
 * game memory. F3 turns the probe on and off (off by default). While on, every
 * 30 field frames it writes one line with:
 *   - the leader's cell (x, z), the layer, and the cell attribute (grid lookup,
 *     the same as the minimap uses);
 *   - the camera offset triple 0xBAD7CC/D0/D4 and the yaw 0xBAD7C6.
 * The lines are used to confirm the scale of the triple and the wall attributes
 * before any arm is applied. See docs/reference/memory-map.md (camera section). */
#ifndef ARM_PROBE_H
#define ARM_PROBE_H

#define AP_TRIPLE_X 0xBAD7CCu   /* s32 lateral offset */
#define AP_TRIPLE_Y 0xBAD7D0u   /* s32 vertical offset */
#define AP_TRIPLE_Z 0xBAD7D4u   /* s32 distance along the view (ZP_DIST) */
#define AP_EVERY    30          /* field frames between lines */

static volatile int g_ap_on = 0;
static int g_ap_frames = 0;

static void arm_probe_frame(void)
{
    unsigned short mode;
    unsigned int lead = MM_FIELD_OBJ;
    int lx, lz, cx, cz, layer, w = 0, h = 0, ok, attr = -1;
    unsigned char *attrs = NULL;
    char s[220];

    if (!g_ap_on) return;
    if ((UINT_PTR)GetModuleHandleA(NULL) != MM_IMAGE_BASE) return;
    mode = *(const unsigned short *)MM_GAME_MODE;
    if (mode != 2) return;                  /* field only */
    if (++g_ap_frames % AP_EVERY) return;

    lx = (int)mm_r32(lead + 0xC);
    lz = (int)mm_r32(lead + 0x14);
    layer = (int)mm_r8(MM_MAP_SECTION) + (int)mm_r8(lead + 0x18);
    ok = mm_grid((unsigned int)layer, &w, &h, &attrs);
    cx = (lx + 0x8000) >> 16;
    cz = (lz + 0x8000) >> 16;
    if (ok) attr = mm_attr(attrs, w, h, cx, cz);

    wsprintfA(s, "arm probe: leader cell %d,%d layer %d grid %d attr %02X | triple %d %d %d | yaw %u",
              cx, cz, layer, ok, attr < 0 ? 0 : attr,
              (int)*(const int *)AP_TRIPLE_X, (int)*(const int *)AP_TRIPLE_Y,
              (int)*(const int *)AP_TRIPLE_Z, (unsigned int)mm_r16(MM_CAMERA_YAW));
    logln(s);

    /* the eight neighbour cells, in the order N, NE, E, SE, S, SW, W, NW (x right, z down) */
    if (ok) {
        static const int nx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
        static const int nz[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
        char nb[8 * 3 + 16];
        int k;
        for (k = 0; k < 8; k++) {
            int a = mm_attr(attrs, w, h, cx + nx[k], cz + nz[k]);
            wsprintfA(nb + k * 3, "%02X ", a & 0xff);
        }
        wsprintfA(s, "arm probe: neighbours N NE E SE S SW W NW: %s", nb);
        logln(s);
    }
}

#endif
