/*
 * UNDERCROFT - an original top-down roguelite dungeon crawler for the NES.
 * Built for NES-on-a-chip handhelds (mapper 0, 32 KB PRG, 8 KB CHR).
 *
 * Controls: D-pad move, A shoot (aim locks while held, so you can strafe),
 *           B drop a bomb, Start map/pause.
 */
#include <string.h>
#include "nes.h"
#include "tiles.h"
#include "rooms.h"

/* ------------------------------------------------------------------ tuning */
#define LAST_FLOOR 5
#define MAX_EN 8
#define MAX_PS 10
#define MAX_ES 12
#define MAX_PK 4
#define MAX_BM 2
#define MAX_FX 6
#define VBUF_LIMIT 150

/* ------------------------------------------------------------- metatiles */
#define MT_FLOOR     0
#define MT_FLOOR2    1
#define MT_WALL      2
#define MT_ROCK      3
#define MT_PIT       4
#define MT_DOOR      5
#define MT_DOOR_L    6
#define MT_DOOR_R    7
#define MT_DOOR_TB   8
#define MT_TRAP      9
#define MT_PEDESTAL 10
#define MT_BDOOR    11
#define MT_BDOOR_L  12
#define MT_BDOOR_R  13
#define MT_BDOOR_TB 14
#define MT_TDOOR_L  15
#define MT_TDOOR_R  16
#define MT_TDOOR_TB 17

#define F_WALK 1
#define F_SHOT 2
#define F_FLY  4

static const unsigned char mt_base[] = {
    T_FLOOR, T_FLOOR2, T_WALL, T_ROCK, T_PIT, T_DOOR_CLOSED, T_DOOR_L, T_DOOR_R,
    T_DOOR_TB, T_TRAP, T_PEDESTAL, T_DOOR_CLOSED, T_DOOR_L, T_DOOR_R, T_DOOR_TB,
    T_DOOR_L, T_DOOR_R, T_DOOR_TB
};
static const unsigned char mt_pal[] = { 0,0,1,0,0,2,1,1,1,2,0,3,3,3,3,2,2,2 };
const unsigned char mt_flags[] = { 0,0,7,3,1,7,0,0,0,0,0,7,0,0,0,0,0,0 };

/* door metatiles by kind: 0 normal, 1 boss, 2 treasure */
static const unsigned char door_l[] = { MT_DOOR_L, MT_BDOOR_L, MT_TDOOR_L };
static const unsigned char door_r[] = { MT_DOOR_R, MT_BDOOR_R, MT_TDOOR_R };
static const unsigned char door_tb[] = { MT_DOOR_TB, MT_BDOOR_TB, MT_TDOOR_TB };
static const unsigned char door_c[] = { MT_DOOR, MT_BDOOR, MT_DOOR };

/* ------------------------------------------------------------- floor map */
#define FM_W 9
#define FM_SIZE 81
#define FM_START 40
#define RT_NORMAL 1
#define RT_START  2
#define RT_BOSS   3
#define RT_ITEM   4
#define RF_TYPE    0x07
#define RF_VISITED 0x10
#define RF_CLEARED 0x20
#define RF_SEEN    0x40

static const unsigned char fm_col[FM_SIZE] = {
    0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8,
    0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8, 0,1,2,3,4,5,6,7,8
};

/* --------------------------------------------------------------- enemies */
#define E_BAT    1
#define E_SLIME  2
#define E_GAZER  3
#define E_BUG    4
#define E_BOSS   5
static const unsigned char en_base_hp[] = { 0, 3, 6, 6, 8, 0 };
const unsigned char en_w[]  = { 0, 12, 12, 12, 12, 28 };
const unsigned char en_h[]  = { 0,  6, 10, 12, 12, 22 };
const unsigned char en_ox[] = { 0,  2,  2,  2,  2,  2 };
const unsigned char en_oy[] = { 0,  1,  5,  2,  2,  8 };

/* pickups */
#define PK_HEART 1
#define PK_HALF  2
#define PK_BOMB  3
#define PK_ITEM  16

#define FX_EXPL 1
#define FX_POOF 2

#define SF_TRIPLE 1
#define SF_PIERCE 2


static const unsigned char hop_h[33] = {
    0,3,6,8,11,13,15,16,18,19,21,22,23,23,24,24,24,24,24,23,23,22,21,19,18,16,15,13,11,8,6,3,0
};

static const unsigned char bit_tbl[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

static const unsigned char palette[32] = {
    0x0F,0x08,0x18,0x10,  0x0F,0x00,0x10,0x20,  0x0F,0x07,0x17,0x27,  0x0F,0x16,0x30,0x27,
    0x0F,0x0F,0x21,0x36,  0x0F,0x16,0x27,0x38,  0x0F,0x0F,0x1A,0x3A,  0x0F,0x0F,0x13,0x30
};

/* ------------------------------------------------------------ metasprites */
#define MS16(t,a)  { 0,0,(t),(a), 8,0,(t)+1,(a), 0,8,(t)+2,(a), 8,8,(t)+3,(a), 128 }
#define MS16H(t,a) { 8,0,(t),(a)|0x40, 0,0,(t)+1,(a)|0x40, 8,8,(t)+2,(a)|0x40, 0,8,(t)+3,(a)|0x40, 128 }
#define MSSYM(t,a) { 0,0,(t),(a), 8,0,(t),(a)|0x40, 0,8,(t)+1,(a), 8,8,(t)+1,(a)|0x40, 128 }
#define MSSYMV(t,a) { 0,0,(t)+1,(a)|0x80, 8,0,(t)+1,(a)|0xC0, 0,8,(t),(a)|0x80, 8,8,(t),(a)|0xC0, 128 }
#define MSBAT(t,a) { 0,0,(t),(a), 8,0,(t),(a)|0x40, 128 }
#define MS4WAY(t,a) { 0,0,(t),(a), 8,0,(t),(a)|0x40, 0,8,(t),(a)|0x80, 8,8,(t),(a)|0xC0, 128 }

static const unsigned char ms_pl_up_a[]    = MS16(S_PL_UP_A, 0);
static const unsigned char ms_pl_up_b[]    = MS16(S_PL_UP_B, 0);
static const unsigned char ms_pl_down_a[]  = MS16(S_PL_DOWN_A, 0);
static const unsigned char ms_pl_down_b[]  = MS16(S_PL_DOWN_B, 0);
static const unsigned char ms_pl_left_a[]  = MS16H(S_PL_SIDE_A, 0);
static const unsigned char ms_pl_left_b[]  = MS16H(S_PL_SIDE_B, 0);
static const unsigned char ms_pl_right_a[] = MS16(S_PL_SIDE_A, 0);
static const unsigned char ms_pl_right_b[] = MS16(S_PL_SIDE_B, 0);
static const unsigned char * const pl_meta[8] = {
    ms_pl_up_a, ms_pl_up_b, ms_pl_down_a, ms_pl_down_b,
    ms_pl_left_a, ms_pl_left_b, ms_pl_right_a, ms_pl_right_b
};

static const unsigned char ms_bat_a[]    = MSBAT(S_BAT_A, 3);
static const unsigned char ms_bat_b[]    = MSBAT(S_BAT_B, 3);
static const unsigned char ms_slime_a[]  = MSSYM(S_SLIME_A, 2);
static const unsigned char ms_slime_b[]  = MSSYM(S_SLIME_B, 2);
static const unsigned char ms_gazer[]    = MSSYM(S_GAZER, 3);
static const unsigned char ms_bug_a[]    = MSSYM(S_BUG_A, 3);
static const unsigned char ms_bug_b[]    = MSSYM(S_BUG_B, 3);
static const unsigned char ms_bug_da[]   = MSSYMV(S_BUG_A, 3);
static const unsigned char ms_bug_db[]   = MSSYMV(S_BUG_B, 3);
static const unsigned char ms_expl_a[]   = MS4WAY(S_EXPL_A, 1);
static const unsigned char ms_expl_b[]   = MS4WAY(S_EXPL_B, 1);

#define BROW(r) 0,(r)*8,S_BOSS+(r)*2,2, 8,(r)*8,S_BOSS+(r)*2+1,2, \
                16,(r)*8,S_BOSS+(r)*2+1,0x42, 24,(r)*8,S_BOSS+(r)*2,0x42
static const unsigned char ms_boss[] = { BROW(0), BROW(1), BROW(2), BROW(3), 128 };

/* items: 1..8 */
static const unsigned char item_tile[9] = {
    0, S_IT_FLAME, S_IT_GLASS, S_IT_BOOT, S_IT_LENS, S_IT_HEART, S_IT_TRIPLE, S_IT_SPEAR, S_IT_BOMBS
};
static const unsigned char item_pal[9] = { 0, 1, 3, 3, 3, 1, 1, 3, 3 };
static const char * const item_name[9] = {
    "",
    "SHARP EMBER - DAMAGE UP",
    "SANDGLASS - FIRE RATE UP",
    "SWIFT BOOT - SPEED UP",
    "SPYGLASS - RANGE UP",
    "HEART JAR - HEALTH UP",
    "TRIPLE WICK - TRIPLE SHOT",
    "PIERCER - SHOTS PIERCE",
    "BOMB SACK - PLUS 5 BOMBS"
};

/* free floor cells to try, nearest the room centre first */
static const unsigned char drop_cells[] = {
    6*16+7, 6*16+8, 5*16+7, 7*16+8, 5*16+8, 7*16+7, 6*16+5, 6*16+10,
    4*16+7, 8*16+8, 6*16+3, 6*16+12, 3*16+3, 9*16+12, 6*16+1
};

/* ================================================================ state */
#pragma bss-name(push, "ZEROPAGE")
unsigned int pl_x, pl_y;            /* 8.8 fixed point, sprite top-left */
unsigned char px8, py8;             /* cached pixel position */
unsigned char pcx, pcy;             /* player body centre */
unsigned char pl_face, pl_aim, pl_hp, pl_maxhp, pl_bombs, pl_inv, pl_cd, pl_moving;
unsigned char st_dmg, st_delay, st_range, st_sspd, st_flags;
unsigned int st_speed;
unsigned char bx, by, bw, bh, bmask; /* box_hit() arguments */
unsigned char ce, ecx, ecy;          /* current enemy and its centre */
unsigned char room_grace, en_count, msg_timer, boss_dirty, boss_slot, boss_max;
unsigned char cur, floor_num, game_result, item_pool;
unsigned char en_type[MAX_EN], en_hp[MAX_EN], en_t[MAX_EN], en_st[MAX_EN];
unsigned char en_flash[MAX_EN], en_dir[MAX_EN];
unsigned char en_l[MAX_EN], en_r[MAX_EN], en_tp[MAX_EN], en_bt[MAX_EN]; /* hitboxes */
unsigned char pl_l, pl_r, pl_t, pl_b;                                    /* player body box */
unsigned char sl, sr, stp, sbt, skip_e;                                  /* shot box */
unsigned char en_px[MAX_EN], en_py[MAX_EN];                              /* pixel positions */
#pragma bss-name(pop)

extern unsigned int try_x, try_y;   /* fast.s */
#pragma zpsym("try_x")
#pragma zpsym("try_y")
void en_try(void);
void es_move(void);
unsigned char box_hit(void);
void __fastcall__ set_box(unsigned char e);
unsigned char shot_find(void);

unsigned int en_x[MAX_EN], en_y[MAX_EN];
int en_vx[MAX_EN], en_vy[MAX_EN];
unsigned char ps_x[MAX_PS], ps_y[MAX_PS], ps_life[MAX_PS], ps_hit[MAX_PS];
unsigned char ps_dx[MAX_PS], ps_dy[MAX_PS];
unsigned int es_x[MAX_ES], es_y[MAX_ES];
int es_vx[MAX_ES], es_vy[MAX_ES];
unsigned char es_on[MAX_ES], es_px[MAX_ES], es_py[MAX_ES];
unsigned char pk_type[MAX_PK], pk_x[MAX_PK], pk_y[MAX_PK];
unsigned char bm_x[MAX_BM], bm_y[MAX_BM], bm_t[MAX_BM];
unsigned char fx_x[MAX_FX], fx_y[MAX_FX], fx_t[MAX_FX], fx_kind[MAX_FX];
unsigned char room[208];            /* 16 x 13 metatiles */
unsigned char fmap[FM_SIZE], flayout[FM_SIZE], fextra[FM_SIZE];
unsigned char mtq[16], mtq_n;
unsigned char spawn_cells[6], spawn_n;
int aim_vx, aim_vy;
unsigned char vb;

#define GR_NONE 0
#define GR_DEAD 1
#define GR_WIN  2

#define CELL_OF(x, y) ((unsigned char)(((unsigned char)((y) - 32) & 0xF0) + ((x) >> 4)))
#define FLAGS_AT(x, y) (mt_flags[room[CELL_OF(x, y)]])
#define NT(x, y) (0x2000 + ((y) << 5) + (x))

/* ============================================================== sound */
static void sfx_shoot(void)  { APU(0x4000)=0x44; APU(0x4001)=0x92; APU(0x4002)=0x60; APU(0x4003)=0x00; }
static void sfx_hit(void)    { APU(0x400C)=0x01; APU(0x400E)=0x05; APU(0x400F)=0x00; }
static void sfx_kill(void)   { APU(0x400C)=0x03; APU(0x400E)=0x08; APU(0x400F)=0x10; }
static void sfx_boom(void)   { APU(0x400C)=0x0F; APU(0x400E)=0x0D; APU(0x400F)=0x40; }
static void sfx_pickup(void) { APU(0x4004)=0x86; APU(0x4005)=0xBA; APU(0x4006)=0xC0; APU(0x4007)=0x10; }
static void sfx_hurt(void)   { APU(0x4004)=0xC3; APU(0x4005)=0x93; APU(0x4006)=0x40; APU(0x4007)=0x41; }
static void sfx_door(void)   { APU(0x4008)=0x18; APU(0x400A)=0x00; APU(0x400B)=0x81; }
static void sfx_eshot(void)  { APU(0x4008)=0x08; APU(0x400A)=0x80; APU(0x400B)=0x08; }

/* ======================================================== PPU helpers */
static void ppu_off(void) { ppu_mask = 0; wait_frame(); }
static void ppu_on(void)  { ppu_mask = 0x1E; wait_frame(); }

static void vram_adr(unsigned int a)
{
    PPU_ADDR = (unsigned char)(a >> 8);
    PPU_ADDR = (unsigned char)a;
}

static void vram_fill(unsigned char v, unsigned int n)
{
    while (n) { PPU_DATA = v; --n; }
}

static void vram_str(const char *s)
{
    while (*s) PPU_DATA = *s++;
}

static void put_str(unsigned int a, const char *s)
{
    vram_adr(a);
    vram_str(s);
}

/* buffered writes, applied by the NMI handler during the next vblank */
static void vbuf_hdr(unsigned int addr, unsigned char len)
{
    if (vram_idx + len + 4 > VBUF_LIMIT) wait_frame();
    vb = vram_idx;
    vram_buf[vb] = (unsigned char)(addr >> 8);
    vram_buf[vb + 1] = (unsigned char)addr;
    vram_buf[vb + 2] = len;
    vb += 3;
}
#define VBUF_PUT(v) (vram_buf[vb++] = (v))
static void vbuf_end(void) { vram_buf[vb] = 0; vram_idx = vb; }

static void vbuf_text(unsigned int addr, const char *s, unsigned char len)
{
    unsigned char n;
    vbuf_hdr(addr, len);
    for (n = 0; n < len; ++n) {
        if (*s) VBUF_PUT(*s++);
        else VBUF_PUT(' ');
    }
    vbuf_end();
}

static unsigned char absdiff(unsigned char a, unsigned char b)
{
    return a > b ? a - b : b - a;
}

/* ============================================================ the HUD */
static void hud_hearts(void)
{
    unsigned char h, n, v, base;
    n = pl_maxhp >> 1;
    vbuf_hdr(NT(2, 1), 12);
    for (h = 0; h < 12; ++h) {
        base = h << 1;
        if (h >= n) v = 0;
        else if (pl_hp >= base + 2) v = T_HEART_F;
        else if (pl_hp == base + 1) v = T_HEART_H;
        else v = T_HEART_E;
        VBUF_PUT(v);
    }
    vbuf_end();
}

static void hud_bombs(void)
{
    unsigned char tens, ones;
    tens = 0; ones = pl_bombs;
    while (ones >= 10) { ones -= 10; ++tens; }
    vbuf_hdr(NT(17, 1), 3);
    VBUF_PUT(T_BOMB_ICON);
    VBUF_PUT('0' + tens);
    VBUF_PUT('0' + ones);
    vbuf_end();
}

static void hud_floor(void)
{
    vbuf_hdr(NT(23, 1), 7);
    VBUF_PUT('F'); VBUF_PUT('L'); VBUF_PUT('O'); VBUF_PUT('O'); VBUF_PUT('R');
    VBUF_PUT(' '); VBUF_PUT('0' + floor_num);
    vbuf_end();
}

static void show_msg(const char *s)
{
    vbuf_text(NT(2, 2), s, 28);
    msg_timer = 150;
}

static void hud_boss(void)
{
    unsigned char s, fill;
    unsigned int hp16, acc;
    fill = 0;
    if (boss_slot != 255) {
        hp16 = (unsigned int)en_hp[boss_slot] << 4;
        acc = 0;
        for (s = 0; s < 16; ++s) {
            if (hp16 > acc) ++fill;
            acc += boss_max;
        }
    }
    vbuf_hdr(NT(8, 3), 16);
    for (s = 0; s < 16; ++s) {
        if (boss_slot == 255) VBUF_PUT(0);
        else VBUF_PUT(s < fill ? T_BAR_F : T_BAR_E);
    }
    vbuf_end();
}

static void hud_all(void)
{
    hud_hearts();
    hud_bombs();
    hud_floor();
    hud_boss();
}

/* ======================================================= room drawing */
static unsigned char attr_byte(unsigned char top, unsigned char c0)
{
    unsigned char i, v;
    i = (top << 4) + c0;
    v = mt_pal[room[i]] | (mt_pal[room[i + 1]] << 2);
    if (top < 12) v |= (mt_pal[room[i + 16]] << 4) | (mt_pal[room[i + 17]] << 6);
    return v;
}

static void draw_room(void)
{
    unsigned char r, c, m, n;
    vram_adr(0x2000);
    vram_fill(0, 128);
    for (r = 0; r < 13; ++r) {
        n = r << 4;
        for (c = 0; c < 16; ++c) { m = mt_base[room[n + c]]; PPU_DATA = m; PPU_DATA = m + 1; }
        for (c = 0; c < 16; ++c) { m = mt_base[room[n + c]]; PPU_DATA = m + 2; PPU_DATA = m + 3; }
    }
    for (c = 0; c < 8; ++c) PPU_DATA = 0xFF;
    for (r = 0; r < 13; r += 2)
        for (c = 0; c < 16; c += 2)
            PPU_DATA = attr_byte(r, c);
}

static void mt_queue(unsigned char c)
{
    if (mtq_n < 16) mtq[mtq_n++] = c;
}

/* redraw up to 3 queued metatiles per frame through the vram buffer */
static void mt_flush(void)
{
    unsigned char n, c, m, r, col;
    unsigned int addr;
    for (n = 0; n < 3 && mtq_n; ++n) {
        c = mtq[--mtq_n];
        m = mt_base[room[c]];
        r = c >> 4;
        col = c & 15;
        addr = 0x2080 + ((unsigned int)r << 6) + (col << 1);
        vbuf_hdr(addr, 2); VBUF_PUT(m); VBUF_PUT(m + 1); vbuf_end();
        vbuf_hdr(addr + 32, 2); VBUF_PUT(m + 2); VBUF_PUT(m + 3); vbuf_end();
        vbuf_hdr(0x23C0 + (((r >> 1) + 1) << 3) + (col >> 1), 1);
        VBUF_PUT(attr_byte(r & 0xFE, col & 0xFE));
        vbuf_end();
    }
}

/* ===================================================== floor generation */
static unsigned char neighbor(unsigned char c, unsigned char d)
{
    switch (d) {
    case 0: return c >= FM_W ? c - FM_W : 255;
    case 1: return c < FM_SIZE - FM_W ? c + FM_W : 255;
    case 2: return fm_col[c] ? c - 1 : 255;
    default: return fm_col[c] != FM_W - 1 ? c + 1 : 255;
    }
}

static unsigned char count_nb(unsigned char c)
{
    unsigned char d, n, m;
    n = 0;
    for (d = 0; d < 4; ++d) {
        m = neighbor(c, d);
        if (m != 255 && fmap[m]) ++n;
    }
    return n;
}

static unsigned char pick_item(void)
{
    unsigned char id;
    if (!item_pool) item_pool = 0xFF;
    for (;;) {
        id = rand8() & 7;
        if (item_pool & bit_tbl[id]) {
            item_pool &= ~bit_tbl[id];
            return id + 1;
        }
    }
}

static void gen_floor(void)
{
    unsigned char target, count, qh, qt, c, d, n, ndead, best, bestd;
    unsigned char *queue, *dist;
    queue = room;
    dist = room + 100;
    target = 5 + (floor_num << 1);
    for (;;) {
        memset(fmap, 0, FM_SIZE);
        fmap[FM_START] = RT_START;
        count = 1; queue[0] = FM_START; qh = 0; qt = 1;
        while (qh < qt) {
            c = queue[qh++];
            for (d = 0; d < 4; ++d) {
                if (count >= target) break;
                n = neighbor(c, d);
                if (n == 255 || fmap[n]) continue;
                if (rand8() & 1) continue;
                if (count_nb(n) > 1) continue;
                fmap[n] = RT_NORMAL;
                queue[qt++] = n;
                ++count;
            }
        }
        if (count < target) continue;

        memset(dist, 255, FM_SIZE);
        dist[FM_START] = 0; queue[0] = FM_START; qh = 0; qt = 1;
        while (qh < qt) {
            c = queue[qh++];
            for (d = 0; d < 4; ++d) {
                n = neighbor(c, d);
                if (n != 255 && fmap[n] && dist[n] == 255) {
                    dist[n] = dist[c] + 1;
                    queue[qt++] = n;
                }
            }
        }
        ndead = 0; best = 255; bestd = 0;
        for (c = 0; c < FM_SIZE; ++c) {
            if (fmap[c] == RT_NORMAL && count_nb(c) == 1) {
                queue[ndead++] = c;
                if (dist[c] > bestd) { bestd = dist[c]; best = c; }
            }
        }
        if (ndead < 2 || bestd < 3) continue;
        fmap[best] = RT_BOSS;
        do { c = queue[rand8() % ndead]; } while (c == best);
        fmap[c] = RT_ITEM;
        break;
    }
    for (c = 0; c < FM_SIZE; ++c) {
        flayout[c] = (rand8() % NUM_LAYOUTS) | (rand8() & 0xC0);
        fextra[c] = 0;
        d = fmap[c];
        if (d == RT_ITEM || d == RT_BOSS) fextra[c] = pick_item();
        if (d == RT_START || d == RT_ITEM) fmap[c] |= RF_CLEARED;
    }
}

/* ========================================================== room build */
static void set_doors(unsigned char open, unsigned char upd)
{
    unsigned char d, n, kind, nt, ct, c1, c2;
    ct = fmap[cur] & RF_TYPE;
    for (d = 0; d < 4; ++d) {
        n = neighbor(cur, d);
        if (n == 255 || !fmap[n]) continue;
        nt = fmap[n] & RF_TYPE;
        if (nt == RT_BOSS || ct == RT_BOSS) kind = 1;
        else if (nt == RT_ITEM || ct == RT_ITEM) kind = 2;
        else kind = 0;
        switch (d) {
        case 0: c1 = 7; c2 = 8; break;
        case 1: c1 = 199; c2 = 200; break;
        case 2: c1 = 96; c2 = 255; break;
        default: c1 = 111; c2 = 255; break;
        }
        if (c2 != 255) {
            room[c1] = open ? door_l[kind] : door_c[kind];
            room[c2] = open ? door_r[kind] : door_c[kind];
        } else {
            room[c1] = open ? door_tb[kind] : door_c[kind];
        }
        if (upd) {
            mt_queue(c1);
            if (c2 != 255) mt_queue(c2);
        }
    }
}

static void build_room(void)
{
    unsigned char x, y, sx, sy, v, lay, cell, type, h;
    const unsigned char *L;
    type = fmap[cur] & RF_TYPE;
    memset(room, MT_FLOOR, 208);
    memset(room, MT_WALL, 16);
    memset(room + 192, MT_WALL, 16);
    for (y = 1; y < 12; ++y) {
        room[y << 4] = MT_WALL;
        room[(y << 4) + 15] = MT_WALL;
    }
    spawn_n = 0;
    if (type == RT_NORMAL) {
        lay = flayout[cur];
        L = layouts[lay & 15];
        for (y = 0; y < 11; ++y) {
            sy = (lay & 0x80) ? 10 - y : y;
            for (x = 0; x < 14; ++x) {
                sx = (lay & 0x40) ? 13 - x : x;
                v = L[sy * 14 + sx];
                cell = ((y + 1) << 4) + x + 1;
                if (v == 1) room[cell] = MT_ROCK;
                else if (v == 2) room[cell] = MT_PIT;
                else if (v == 3 && spawn_n < 6) spawn_cells[spawn_n++] = cell;
            }
        }
    }
    /* sprinkle cracked floor tiles, stable per room */
    for (cell = 17; cell < 191; ++cell) {
        if (room[cell] != MT_FLOOR) continue;
        h = cell ^ (cur << 3) ^ (cell >> 3);
        if ((h & 15) == 7) room[cell] = MT_FLOOR2;
    }
    if (type == RT_ITEM) room[6 * 16 + 7] = MT_PEDESTAL;
    if (type == RT_BOSS && (fmap[cur] & RF_CLEARED)) {
        room[5 * 16 + 7] = MT_PEDESTAL;
        room[8 * 16 + 7] = MT_TRAP;
    }
    set_doors((fmap[cur] & RF_CLEARED) != 0, 0);
}

/* ============================================================ entities */
static void spawn_enemy(unsigned char type, unsigned char x, unsigned char y)
{
    unsigned char e;
    for (e = 0; e < MAX_EN; ++e)
        if (!en_type[e]) break;
    if (e == MAX_EN) return;
    en_type[e] = type;
    en_x[e] = (unsigned int)x << 8;
    en_y[e] = (unsigned int)y << 8;
    en_vx[e] = 0; en_vy[e] = 0;
    en_st[e] = 0; en_flash[e] = 0; en_dir[e] = 0;
    en_t[e] = 20 + (rand8() & 31);
    en_hp[e] = en_base_hp[type] + ((floor_num - 1) << 1);
    if (type == E_BOSS) {
        en_hp[e] = 50 + floor_num * 20;
        boss_max = en_hp[e];
        boss_slot = e;
        en_t[e] = 50;
        boss_dirty = 1;
    }
    set_box(e);
    ++en_count;
}

static void spawn_eshot(unsigned char x, unsigned char y)
{
    unsigned char s;
    for (s = 0; s < MAX_ES; ++s) {
        if (!es_on[s]) {
            es_on[s] = 1;
            es_x[s] = (unsigned int)x << 8;
            es_y[s] = (unsigned int)y << 8;
            es_vx[s] = aim_vx;
            es_vy[s] = aim_vy;
            es_px[s] = x;
            es_py[s] = y;
            return;
        }
    }
}

static void spawn_pickup(unsigned char type, unsigned char x, unsigned char y)
{
    unsigned char k;
    for (k = 0; k < MAX_PK; ++k) {
        if (!pk_type[k]) {
            pk_type[k] = type; pk_x[k] = x; pk_y[k] = y;
            return;
        }
    }
}

static void spawn_fx(unsigned char kind, unsigned char x, unsigned char y)
{
    unsigned char f;
    for (f = 0; f < MAX_FX; ++f) {
        if (!fx_t[f]) {
            fx_kind[f] = kind; fx_x[f] = x; fx_y[f] = y;
            fx_t[f] = kind == FX_EXPL ? 20 : 12;
            return;
        }
    }
}

static unsigned char free_cell(void)
{
    unsigned char n, c;
    for (n = 0; n < sizeof(drop_cells); ++n) {
        c = drop_cells[n];
        if (room[c] == MT_FLOOR || room[c] == MT_FLOOR2) return c;
    }
    return 6 * 16 + 1;
}

static void spawn_drop(unsigned char type)
{
    unsigned char c;
    c = free_cell();
    spawn_pickup(type, ((c & 15) << 4) + 4, (c & 0xF0) + 32 + 4);
}

static unsigned char aim_dir(unsigned char sx, unsigned char sy, unsigned char tx, unsigned char ty)
{
    unsigned char ax, ay, left, up;
    left = tx < sx;
    up = ty < sy;
    ax = left ? sx - tx : tx - sx;
    ay = up ? sy - ty : ty - sy;
    if (ax < (ay >> 1)) return up ? 0 : 4;
    if (ay < (ax >> 1)) return left ? 6 : 2;
    if (up) return left ? 7 : 1;
    return left ? 5 : 3;
}

static void set_aim(unsigned char d, unsigned int spd)
{
    int dg;
    dg = (spd >> 1) + (spd >> 3) + (spd >> 4) + (spd >> 6);
    switch (d) {
    case 0:  aim_vx = 0;    aim_vy = -spd; break;
    case 1:  aim_vx = dg;   aim_vy = -dg;  break;
    case 2:  aim_vx = spd;  aim_vy = 0;    break;
    case 3:  aim_vx = dg;   aim_vy = dg;   break;
    case 4:  aim_vx = 0;    aim_vy = spd;  break;
    case 5:  aim_vx = -dg;  aim_vy = dg;   break;
    case 6:  aim_vx = -spd; aim_vy = 0;    break;
    default: aim_vx = -dg;  aim_vy = -dg;  break;
    }
}

/* ======================================================== player hurt */
static void hurt_player(unsigned char n)
{
    if (pl_inv || !pl_hp) return;
    pl_hp = pl_hp > n ? pl_hp - n : 0;
    pl_inv = 60;
    sfx_hurt();
    hud_hearts();
    if (!pl_hp) game_result = GR_DEAD;
}

static void hurt_enemy(unsigned char e, unsigned char dmg)
{
    unsigned char ex, ey;
    if (en_hp[e] > dmg) {
        en_hp[e] -= dmg;
        en_flash[e] = 6;
        sfx_hit();
        if (e == boss_slot) boss_dirty = 1;
        return;
    }
    ex = en_px[e];
    ey = en_py[e];
    if (en_type[e] == E_BOSS) {
        spawn_fx(FX_EXPL, ex, ey);
        spawn_fx(FX_EXPL, ex + 16, ey + 16);
        spawn_fx(FX_EXPL, ex + 8, ey + 8);
        boss_slot = 255;
        boss_dirty = 1;
        memset(es_on, 0, MAX_ES);
        sfx_boom();
    } else {
        spawn_fx(FX_POOF, ex + 4, ey + 4);
        sfx_kill();
    }
    en_type[e] = 0;
    --en_count;
}

/* ======================================================== enemy AI */
static unsigned char move_enemy(unsigned char mask)
{
    unsigned char ty, blocked, ox, oy, ex2, np;
    ty = en_type[ce];
    blocked = 0;
    bw = en_w[ty]; bh = en_h[ty]; bmask = mask;
    ox = en_ox[ty]; oy = en_oy[ty];
    en_try();

    np = (unsigned char)(try_x >> 8);
    bx = np + ox;
    by = en_py[ce] + oy;
    ex2 = bx + bw;
    if (bx < 16 || ex2 > 240 || ex2 < bx || box_hit()) {
        blocked = 1;
    } else {
        en_x[ce] = try_x;
        en_px[ce] = np;
    }
    np = (unsigned char)(try_y >> 8);
    bx = en_px[ce] + ox;
    by = np + oy;
    ex2 = by + bh;
    if (by < 48 || ex2 > 224 || ex2 < by || box_hit()) {
        blocked |= 2;
    } else {
        en_y[ce] = try_y;
        en_py[ce] = np;
    }
    return blocked;
}

static void ai_bat(void)
{
    unsigned char b;
    if (!en_t[ce]) {
        en_t[ce] = 16 + (rand8() & 15);
        set_aim(aim_dir(ecx, ecy, pcx, pcy), 0x00B0 + (floor_num << 3));
        en_vx[ce] = aim_vx + (int)(rand8() & 0x7F) - 0x40;
        en_vy[ce] = aim_vy + (int)(rand8() & 0x7F) - 0x40;
    } else {
        --en_t[ce];
    }
    b = move_enemy(F_FLY);
    if (b & 1) en_vx[ce] = -en_vx[ce];
    if (b & 2) en_vy[ce] = -en_vy[ce];
}

static void ai_slime(void)
{
    if (!en_st[ce]) {
        if (en_t[ce]) {
            --en_t[ce];
        } else {
            en_st[ce] = 1;
            en_t[ce] = 22;
            set_aim(aim_dir(ecx, ecy, pcx, pcy), 0x0130 + (floor_num << 4));
            en_vx[ce] = aim_vx;
            en_vy[ce] = aim_vy;
        }
    } else {
        move_enemy(F_WALK);
        if (--en_t[ce] == 0) {
            en_st[ce] = 0;
            en_t[ce] = 24 + (rand8() & 31);
        }
    }
}

static void ai_gazer(void)
{
    if (en_t[ce]) {
        --en_t[ce];
    } else {
        set_aim(aim_dir(ecx, ecy, pcx, pcy), 0x0160 + (floor_num << 4));
        spawn_eshot(ecx - 4, ecy - 4);
        sfx_eshot();
        en_t[ce] = 90 + (rand8() & 63) - (floor_num << 3);
    }
}

static void bug_vel(unsigned char d, int spd)
{
    en_dir[ce] = d;
    en_vx[ce] = d == 2 ? -spd : (d == 3 ? spd : 0);
    en_vy[ce] = d == 0 ? -spd : (d == 1 ? spd : 0);
}

static void ai_bug(void)
{
    switch (en_st[ce]) {
    case 0:
        if (!en_t[ce] || move_enemy(F_WALK)) {
            bug_vel(rand8() & 3, 0x0090);
            en_t[ce] = 30 + (rand8() & 63);
        } else {
            --en_t[ce];
        }
        if (absdiff(ecx, pcx) < 10) {
            en_st[ce] = 1;
            bug_vel(pcy < ecy ? 0 : 1, 0x0280);
        } else if (absdiff(ecy, pcy) < 10) {
            en_st[ce] = 1;
            bug_vel(pcx < ecx ? 2 : 3, 0x0280);
        }
        break;
    case 1:
        if (move_enemy(F_WALK)) {
            en_st[ce] = 2;
            en_t[ce] = 40;
            sfx_hit();
        }
        break;
    default:
        if (--en_t[ce] == 0) en_st[ce] = 0;
        break;
    }
}

static void boss_ring(void)
{
    unsigned char d, step;
    step = floor_num == 1 ? 2 : 1;
    for (d = (frame_cnt & 1) && step == 2 ? 1 : 0; d < 8; d += step) {
        set_aim(d, 0x0120 + (floor_num << 4));
        spawn_eshot(ecx - 4, ecy - 4);
    }
    sfx_boom();
}

static void ai_boss(void)
{
    if (!en_st[ce]) {
        if (en_t[ce]) {
            --en_t[ce];
        } else {
            en_st[ce] = 1;
            en_t[ce] = 32;
            set_aim(aim_dir(ecx, ecy, pcx, pcy), 0x0140 + (floor_num << 4));
            en_vx[ce] = aim_vx;
            en_vy[ce] = aim_vy;
        }
    } else {
        move_enemy(F_WALK);
        if (--en_t[ce] == 0) {
            en_st[ce] = 0;
            en_t[ce] = 56 - (floor_num << 2);
            boss_ring();
            if (floor_num >= 2 && en_count < 4 && (rand8() & 1))
                spawn_enemy(E_BAT, ecx - 8, ecy - 8);
        }
    }
}

static void update_enemies(void)
{
    unsigned char e, ty, ex, ey;
    if (room_grace) { --room_grace; return; }
    for (e = 0; e < MAX_EN; ++e) {
        ty = en_type[e];
        if (!ty) continue;
        if (en_flash[e]) --en_flash[e];
        ce = e;
        ex = en_px[e];
        ey = en_py[e];
        if (ty == E_BOSS) { ecx = ex + 16; ecy = ey + 16; }
        else { ecx = ex + 8; ecy = ey + 8; }
        switch (ty) {
        case E_BAT:   ai_bat();   break;
        case E_SLIME: ai_slime(); break;
        case E_GAZER: ai_gazer(); break;
        case E_BUG:   ai_bug();   break;
        default:      ai_boss();  break;
        }
        if (!en_type[e]) continue;
        set_box(e);
        if (pl_inv) continue;
        if (ty == E_BOSS && en_st[e] && hop_h[en_t[e]] > 8) continue;
        if (en_l[e] >= pl_r || en_r[e] <= pl_l) continue;
        if (en_tp[e] >= pl_b || en_bt[e] <= pl_t) continue;
        hurt_player(ty == E_BOSS ? 2 : 1);
    }
}

/* ================================================================ shots */
static void add_pshot(unsigned char x, unsigned char y, signed char dx, signed char dy)
{
    unsigned char s;
    for (s = 0; s < MAX_PS; ++s) {
        if (!ps_life[s]) {
            ps_x[s] = x; ps_y[s] = y;
            ps_dx[s] = dx; ps_dy[s] = dy;
            ps_life[s] = st_range;
            ps_hit[s] = 0;
            return;
        }
    }
}

static void fire(void)
{
    signed char dx, dy, s;
    unsigned char x, y;
    s = st_sspd;
    dx = 0; dy = 0;
    switch (pl_aim) {
    case 0: dy = -s; break;
    case 1: dy = s; break;
    case 2: dx = -s; break;
    default: dx = s; break;
    }
    x = px8 + 4 + (dx > 0 ? 4 : (dx < 0 ? -4 : 0));
    y = py8 + 4 + (dy > 0 ? 4 : (dy < 0 ? -4 : 0));
    add_pshot(x, y, dx, dy);
    if (st_flags & SF_TRIPLE) {
        if (dx) { add_pshot(x, y, dx, -1); add_pshot(x, y, dx, 1); }
        else    { add_pshot(x, y, -1, dy); add_pshot(x, y, 1, dy); }
    }
    sfx_shoot();
}

static void update_pshots(void)
{
    unsigned char s, e, x, y, cx, cy;
    for (s = 0; s < MAX_PS; ++s) {
        if (!ps_life[s]) continue;
        x = ps_x[s] + ps_dx[s];
        y = ps_y[s] + ps_dy[s];
        ps_x[s] = x;
        ps_y[s] = y;
        cx = x + 4;
        cy = y + 4;
        if (x < 12 || x > 236 || y < 44 || y > 220 || (FLAGS_AT(cx, cy) & F_SHOT)) {
            ps_life[s] = 0;
            spawn_fx(FX_POOF, x, y);
            continue;
        }
        if (--ps_life[s] == 0) {
            spawn_fx(FX_POOF, x, y);
            continue;
        }
        sl = x + 2; sr = x + 6; stp = y + 2; sbt = y + 6;
        skip_e = ps_hit[s] - 1;
        e = shot_find();
        if (e != 255) {
            hurt_enemy(e, st_dmg);
            if (st_flags & SF_PIERCE) ps_hit[s] = e + 1;
            else ps_life[s] = 0;
        }
    }
}

static void update_eshots(void)
{
    unsigned char s, x, y, cx, cy;
    es_move();
    for (s = 0; s < MAX_ES; ++s) {
        if (!es_on[s]) continue;
        x = es_px[s];
        y = es_py[s];
        cx = x + 4;
        cy = y + 4;
        if (x < 12 || x > 236 || y < 44 || y > 220 || (FLAGS_AT(cx, cy) & F_SHOT)) {
            es_on[s] = 0;
            continue;
        }
        sl = x + 2; sr = x + 6; stp = y + 2; sbt = y + 6;
        if (sr <= pl_l || pl_r <= sl || sbt <= pl_t || pl_b <= stp) continue;
        es_on[s] = 0;
        hurt_player(1);
    }
}

/* ======================================================= bombs and fx */
static void explode(unsigned char cx, unsigned char cy)
{
    unsigned char e, ty, c, n, k;
    static const unsigned char around[9] = { 0xEF, 0xF0, 0xF1, 0xFF, 0, 1, 15, 16, 17 };
    spawn_fx(FX_EXPL, cx - 8, cy - 8);
    sfx_boom();
    for (e = 0; e < MAX_EN; ++e) {
        ty = en_type[e];
        if (!ty) continue;
        if (absdiff(en_px[e] + (ty == E_BOSS ? 16 : 8), cx) < 28 &&
            absdiff(en_py[e] + (ty == E_BOSS ? 16 : 8), cy) < 28)
            hurt_enemy(e, 12);
    }
    if (absdiff(pcx, cx) < 22 && absdiff(pcy, cy) < 22) hurt_player(2);
    c = CELL_OF(cx, cy);
    for (k = 0; k < 9; ++k) {
        n = c + around[k];
        if (n < 208 && room[n] == MT_ROCK) {
            room[n] = MT_FLOOR;
            mt_queue(n);
        }
    }
}

static void update_bombs(void)
{
    unsigned char b;
    for (b = 0; b < MAX_BM; ++b) {
        if (bm_t[b] && --bm_t[b] == 0) explode(bm_x[b] + 4, bm_y[b] + 4);
    }
}

static void update_fx(void)
{
    unsigned char f;
    for (f = 0; f < MAX_FX; ++f)
        if (fx_t[f]) --fx_t[f];
}

/* =========================================================== items */
static void apply_item(unsigned char id)
{
    switch (id) {
    case 1: if (st_dmg < 9) ++st_dmg; break;
    case 2: st_delay = st_delay > 9 ? st_delay - 3 : 6; break;
    case 3: if (st_speed < 0x0240) st_speed += 0x0030; break;
    case 4: if (st_range < 60) st_range += 12; if (st_sspd < 5) ++st_sspd; break;
    case 5: if (pl_maxhp < 24) pl_maxhp += 2; pl_hp = pl_maxhp; hud_hearts(); break;
    case 6: st_flags |= SF_TRIPLE; if (st_delay < 26) st_delay += 4; break;
    case 7: st_flags |= SF_PIERCE; break;
    default: pl_bombs += 5; if (pl_bombs > 99) pl_bombs = 99; hud_bombs(); break;
    }
    show_msg(item_name[id]);
}

static void update_pickups(void)
{
    unsigned char k, t, w;
    for (k = 0; k < MAX_PK; ++k) {
        t = pk_type[k];
        if (!t) continue;
        w = t >= PK_ITEM ? 16 : 8;
        if (pk_x[k] >= px8 + 12 || px8 + 4 >= pk_x[k] + w) continue;
        if (pk_y[k] >= py8 + 14 || py8 + 4 >= pk_y[k] + w) continue;
        if (t == PK_HEART || t == PK_HALF) {
            if (pl_hp >= pl_maxhp) continue;
            pl_hp += t == PK_HEART ? 2 : 1;
            if (pl_hp > pl_maxhp) pl_hp = pl_maxhp;
            hud_hearts();
        } else if (t == PK_BOMB) {
            if (pl_bombs < 99) ++pl_bombs;
            hud_bombs();
        } else {
            apply_item(t - PK_ITEM);
            fextra[cur] = 0;
        }
        pk_type[k] = 0;
        sfx_pickup();
    }
}

/* ======================================================= room flow */
static void spawn_room_contents(void)
{
    unsigned char type, n, max, c, off;
    type = fmap[cur] & RF_TYPE;
    if (!(fmap[cur] & RF_CLEARED)) {
        if (type == RT_BOSS) {
            spawn_enemy(E_BOSS, 112, 64);
        } else if (type == RT_NORMAL) {
            max = 2 + floor_num;
            off = spawn_n ? rand8() % spawn_n : 0;
            for (n = 0; n < spawn_n && n < max; ++n) {
                c = spawn_cells[(n + off) % spawn_n];
                spawn_enemy(1 + rand8() % (floor_num < 3 ? floor_num + 1 : 4),
                            (c & 15) << 4, (c & 0xF0) + 32);
            }
        }
        room_grace = 40;
    }
    if (type == RT_ITEM && fextra[cur])
        spawn_pickup(PK_ITEM + fextra[cur], 112, 116);
    else if (type == RT_BOSS && (fmap[cur] & RF_CLEARED) && fextra[cur])
        spawn_pickup(PK_ITEM + fextra[cur], 112, 100);
    else if (type == RT_NORMAL && fextra[cur]) {
        spawn_drop(fextra[cur]);
        fextra[cur] = 0;
    }
}

static void enter_room(unsigned char dir)
{
    unsigned char d, n;
    ppu_off();
    memset(en_type, 0, MAX_EN);
    memset(ps_life, 0, MAX_PS);
    memset(es_on, 0, MAX_ES);
    memset(pk_type, 0, MAX_PK);
    memset(bm_t, 0, MAX_BM);
    memset(fx_t, 0, MAX_FX);
    en_count = 0; mtq_n = 0; boss_slot = 255; room_grace = 0;
    fmap[cur] |= RF_VISITED;
    for (d = 0; d < 4; ++d) {
        n = neighbor(cur, d);
        if (n != 255 && fmap[n]) fmap[n] |= RF_SEEN;
    }
    build_room();
    switch (dir) {
    case 0:  pl_x = 120 << 8; pl_y = 204 << 8; break;
    case 1:  pl_x = 120 << 8; pl_y = 44 << 8;  break;
    case 2:  pl_x = 224 << 8; pl_y = 120 << 8; break;
    case 3:  pl_x = 16 << 8;  pl_y = 120 << 8; break;
    default: pl_x = 120 << 8; pl_y = 120 << 8; break;
    }
    spawn_room_contents();
    draw_room();
    ppu_on();
    hud_all();
    msg_timer = 0;
}

static void change_room(unsigned char d)
{
    unsigned char k;
    if ((fmap[cur] & RF_TYPE) == RT_NORMAL) {
        for (k = 0; k < MAX_PK; ++k)
            if (pk_type[k] && pk_type[k] < PK_ITEM) fextra[cur] = pk_type[k];
    }
    k = neighbor(cur, d);
    if (k == 255 || !fmap[k]) return;
    cur = k;
    enter_room(d);
}

static void clear_room(void)
{
    unsigned char type, r;
    fmap[cur] |= RF_CLEARED;
    set_doors(1, 1);
    sfx_door();
    type = fmap[cur] & RF_TYPE;
    if (type == RT_NORMAL) {
        r = rand8() & 7;
        if (r == 0) spawn_drop(PK_HEART);
        else if (r == 1) spawn_drop(PK_HALF);
        else if (r == 2) spawn_drop(PK_BOMB);
    } else if (type == RT_BOSS) {
        room[5 * 16 + 7] = MT_PEDESTAL;
        room[8 * 16 + 7] = MT_TRAP;
        mt_queue(5 * 16 + 7);
        mt_queue(8 * 16 + 7);
        if (fextra[cur]) spawn_pickup(PK_ITEM + fextra[cur], 112, 100);
        show_msg(floor_num == LAST_FLOOR ? "THE WAY OUT IS OPEN" : "A TRAPDOOR OPENS");
    }
}

static void next_floor(void)
{
    if (floor_num == LAST_FLOOR) {
        game_result = GR_WIN;
        return;
    }
    ++floor_num;
    gen_floor();
    cur = FM_START;
    enter_room(255);
    show_msg("YOU DESCEND DEEPER");
}

/* ============================================================== player */
static void update_player(void)
{
    unsigned char held, x2, y2, c, t1, t2, nudge, o;
    unsigned int spd, n;
    held = pad & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT);
    spd = st_speed;
    if ((held & (PAD_UP | PAD_DOWN)) && (held & (PAD_LEFT | PAD_RIGHT))) spd -= spd >> 2;
    pl_moving = held != 0;

    /* facing: newest direction pressed wins, otherwise keep a held one */
    if (pad_new & PAD_UP) pl_face = 0;
    else if (pad_new & PAD_DOWN) pl_face = 1;
    else if (pad_new & PAD_LEFT) pl_face = 2;
    else if (pad_new & PAD_RIGHT) pl_face = 3;
    else if (held) {
        if (!(held & (PAD_UP >> pl_face)) || pl_face > 3) {
            if (held & PAD_UP) pl_face = 0;
            else if (held & PAD_DOWN) pl_face = 1;
            else if (held & PAD_LEFT) pl_face = 2;
            else pl_face = 3;
        }
    }

    bw = 10; bh = 8; bmask = F_WALK;
    /* horizontal */
    if (held & (PAD_LEFT | PAD_RIGHT)) {
        n = (held & PAD_LEFT) ? pl_x - spd : pl_x + spd;
        bx = (n >> 8) + 3; by = py8 + 8;
        if (!box_hit()) {
            pl_x = n;
        } else if (!(held & (PAD_UP | PAD_DOWN))) {
            /* assist: slide around corners and into doorways */
            x2 = (held & PAD_LEFT) ? bx : bx + 9;
            y2 = by + 7;
            t1 = FLAGS_AT(x2, by) & F_WALK;
            t2 = FLAGS_AT(x2, y2) & F_WALK;
            nudge = 0;
            if (t1 && !t2) nudge = 1;
            else if (!t1 && t2) nudge = 255;
            else if (t1 && t2) {
                o = (by - 32) & 15;
                if (o <= 5 && by >= 56 && !(FLAGS_AT(x2, by - 8) & F_WALK)) nudge = 255;
                else if (o >= 3 && by < 208 && !(FLAGS_AT(x2, by + 15) & F_WALK)) nudge = 1;
            }
            if (nudge) {
                bx = px8 + 3; by = py8 + 8 + (signed char)nudge;
                if (!box_hit()) pl_y += nudge == 1 ? 0x100 : -0x100;
            }
        }
        px8 = pl_x >> 8;
        py8 = pl_y >> 8;
    }
    /* vertical */
    if (held & (PAD_UP | PAD_DOWN)) {
        n = (held & PAD_UP) ? pl_y - spd : pl_y + spd;
        bx = px8 + 3; by = (n >> 8) + 8;
        if (!box_hit()) {
            pl_y = n;
        } else if (!(held & (PAD_LEFT | PAD_RIGHT))) {
            y2 = (held & PAD_UP) ? by : by + 7;
            x2 = bx + 9;
            t1 = FLAGS_AT(bx, y2) & F_WALK;
            t2 = FLAGS_AT(x2, y2) & F_WALK;
            nudge = 0;
            if (t1 && !t2) nudge = 1;
            else if (!t1 && t2) nudge = 255;
            else if (t1 && t2) {
                o = bx & 15;
                if (o <= 5 && bx >= 24 && !(FLAGS_AT(bx - 8, y2) & F_WALK)) nudge = 255;
                else if (o >= 1 && bx < 224 && !(FLAGS_AT(bx + 17, y2) & F_WALK)) nudge = 1;
            }
            if (nudge) {
                bx = px8 + 3 + (signed char)nudge; by = py8 + 8;
                if (!box_hit()) pl_x += nudge == 1 ? 0x100 : -0x100;
            }
        }
        px8 = pl_x >> 8;
        py8 = pl_y >> 8;
    }
    pcx = px8 + 8;
    pcy = py8 + 10;
    pl_l = px8 + 4; pl_r = px8 + 12; pl_t = py8 + 4; pl_b = py8 + 14;

    /* shooting: aim locks while A is held */
    if (pad & PAD_A) {
        if (pad_new & PAD_A) pl_aim = pl_face;
        if (!pl_cd) { fire(); pl_cd = st_delay; }
    } else {
        pl_aim = pl_face;
    }
    if (pl_cd) --pl_cd;

    /* bombs */
    if ((pad_new & PAD_B) && pl_bombs) {
        for (c = 0; c < MAX_BM; ++c) {
            if (!bm_t[c]) {
                bm_x[c] = px8 + 4;
                bm_y[c] = py8 + 6;
                bm_t[c] = 90;
                --pl_bombs;
                hud_bombs();
                break;
            }
        }
    }
    if (pl_inv) --pl_inv;

    /* leaving through a door */
    if (pcy < 42) { change_room(0); return; }
    if (pcy > 230) { change_room(1); return; }
    if (pcx < 12) { change_room(2); return; }
    if (pcx > 244) { change_room(3); return; }

    /* trapdoor */
    c = CELL_OF(pcx, py8 + 12);
    if (room[c] == MT_TRAP) next_floor();
}

/* ============================================================ drawing */
static void draw_enemies(void)
{
    unsigned char e, ty;
    for (e = 0; e < MAX_EN; ++e) {
        ty = en_type[e];
        if (!ty) continue;
        spr_x = en_px[e];
        spr_y = en_py[e] - 1;
        meta_and = 0xFF; meta_or = 0;
        if (en_flash[e]) { meta_and = 0xFC; meta_or = 1; }
        switch (ty) {
        case E_BAT:
            meta_ptr = (frame_cnt & 8) ? ms_bat_a : ms_bat_b;
            break;
        case E_SLIME:
            meta_ptr = en_st[e] ? ms_slime_b : ms_slime_a;
            break;
        case E_GAZER:
            meta_ptr = ms_gazer;
            if (en_t[e] < 16 && !room_grace && (frame_cnt & 2)) { meta_and = 0xFC; meta_or = 1; }
            break;
        case E_BUG:
            if (en_dir[e] == 1) meta_ptr = (frame_cnt & 4) ? ms_bug_da : ms_bug_db;
            else meta_ptr = (frame_cnt & 4) ? ms_bug_a : ms_bug_b;
            break;
        default:
            meta_ptr = ms_boss;
            if (en_st[e]) spr_y -= hop_h[en_t[e]];
            break;
        }
        oam_meta_spr();
    }
}

static void draw_shots(void)
{
    unsigned char s;
    spr_tile = S_SHOT_P; spr_attr = 1;
    for (s = 0; s < MAX_PS; ++s) {
        if (!ps_life[s]) continue;
        spr_x = ps_x[s]; spr_y = ps_y[s] - 1;
        oam_spr();
    }
    spr_tile = S_SHOT_E; spr_attr = 3;
    for (s = 0; s < MAX_ES; ++s) {
        if (!es_on[s]) continue;
        spr_x = es_px[s]; spr_y = es_py[s] - 1;
        oam_spr();
    }
}

static void draw_misc(void)
{
    unsigned char k, t;
    static unsigned char ms_item[17];
    for (k = 0; k < MAX_PK; ++k) {
        t = pk_type[k];
        if (!t) continue;
        spr_x = pk_x[k]; spr_y = pk_y[k] - 1;
        if (t >= PK_ITEM) {
            t -= PK_ITEM;
            ms_item[0] = 0; ms_item[1] = 0; ms_item[2] = item_tile[t];     ms_item[3] = item_pal[t];
            ms_item[4] = 8; ms_item[5] = 0; ms_item[6] = item_tile[t] + 1; ms_item[7] = item_pal[t];
            ms_item[8] = 0; ms_item[9] = 8; ms_item[10] = item_tile[t] + 2; ms_item[11] = item_pal[t];
            ms_item[12] = 8; ms_item[13] = 8; ms_item[14] = item_tile[t] + 3; ms_item[15] = item_pal[t];
            ms_item[16] = 128;
            if (frame_cnt & 32) --spr_y;
            meta_ptr = ms_item; meta_and = 0xFF; meta_or = 0;
            oam_meta_spr();
        } else {
            spr_tile = t == PK_HEART ? S_HEART_P : (t == PK_HALF ? S_HHEART_P : S_BOMB);
            spr_attr = t == PK_BOMB ? 3 : 1;
            oam_spr();
        }
    }
    for (k = 0; k < MAX_FX; ++k) {
        if (!fx_t[k]) continue;
        spr_x = fx_x[k]; spr_y = fx_y[k] - 1;
        if (fx_kind[k] == FX_EXPL) {
            meta_ptr = fx_t[k] > 10 ? ms_expl_a : ms_expl_b;
            meta_and = 0xFF; meta_or = 0;
            oam_meta_spr();
        } else {
            spr_tile = S_POOF; spr_attr = 3;
            oam_spr();
        }
    }
}

static void draw_bombs(void)
{
    unsigned char k;
    spr_tile = S_BOMB;
    for (k = 0; k < MAX_BM; ++k) {
        if (!bm_t[k]) continue;
        spr_x = bm_x[k]; spr_y = bm_y[k] - 1;
        spr_attr = (bm_t[k] < 30 ? (bm_t[k] & 4) : (bm_t[k] & 16)) ? 1 : 3;
        oam_spr();
    }
}

static void draw_sprites(void)
{
    unsigned char f;
    oam_begin();
    draw_bombs();      /* in front of the player, so you can see your bomb */
    if (!(pl_inv & 4)) {
        f = pl_aim << 1;
        if (pl_moving && (frame_cnt & 8)) ++f;
        meta_ptr = pl_meta[f];
        meta_and = 0xFF; meta_or = 0;
        spr_x = px8; spr_y = py8 - 1;
        oam_meta_spr();
    }
    /* alternate priority so overlapping sprites flicker instead of vanishing */
    if (frame_cnt & 1) { draw_shots(); draw_enemies(); draw_misc(); }
    else { draw_misc(); draw_enemies(); draw_shots(); }
    oam_hide_rest();
}

/* ============================================================== screens */
static void clear_screen(unsigned int nt)
{
    vram_adr(nt);
    vram_fill(0, 960);
    vram_fill(0xFF, 64);
}

static unsigned char big_base(char ch)
{
    switch (ch) {
    case 'U': return BIG_U; case 'N': return BIG_N; case 'D': return BIG_D;
    case 'E': return BIG_E; case 'R': return BIG_R; case 'C': return BIG_C;
    case 'O': return BIG_O; case 'F': return BIG_F; default: return BIG_T;
    }
}

static void draw_title(void)
{
    static const char logo[] = "UNDERCROFT";
    unsigned char n, b;
    ppu_off();
    oam_begin(); oam_hide_rest();
    clear_screen(0x2000);
    vram_adr(NT(6, 5));
    for (n = 0; n < 10; ++n) { b = big_base(logo[n]); PPU_DATA = b; PPU_DATA = b + 1; }
    vram_adr(NT(6, 6));
    for (n = 0; n < 10; ++n) { b = big_base(logo[n]); PPU_DATA = b + 2; PPU_DATA = b + 3; }
    put_str(NT(5, 9),  "DESCEND FIVE FLOORS AND");
    put_str(NT(5, 10), "ESCAPE THE UNDERCROFT");
    put_str(NT(5, 14), "D-PAD   MOVE");
    put_str(NT(5, 15), "A       SHOOT");
    put_str(NT(5, 16), "        HOLD A TO KEEP AIM");
    put_str(NT(5, 17), "B       DROP A BOMB");
    put_str(NT(5, 18), "START   MAP AND PAUSE");
    put_str(NT(7, 26), "AN ORIGINAL HOMEBREW");
    ppu_on();
}

static void run_title(void)
{
    unsigned char t;
    draw_title();
    t = 0;
    for (;;) {
        pad_poll();
        rand8();
        if (pad_new & PAD_START) break;
        ++t;
        if ((t & 31) == 0) vbuf_text(NT(10, 22), (t & 32) ? "PRESS START" : "", 11);
        /* the hero on the title screen */
        oam_begin();
        meta_ptr = pl_meta[2 + ((t >> 4) & 1)];
        meta_and = 0xFF; meta_or = 0;
        spr_x = 120; spr_y = 88;
        oam_meta_spr();
        oam_hide_rest();
        wait_frame();
    }
    rng_seed ^= (unsigned int)t << 3;
    if (!rng_seed) rng_seed = 0x1CA5;
}

static void draw_map_screen(void)
{
    unsigned char x, y, c, v, t, known;
    ppu_off();
    clear_screen(0x2000);
    put_str(0x2000 + (2 << 5) + 12, "FLOOR ");
    PPU_DATA = '0' + floor_num;
    for (y = 0; y < FM_W; ++y) {
        for (x = 0; x < FM_W; ++x) {
            c = y * FM_W + x;
            v = fmap[c];
            if (!(v & RF_TYPE) || !(v & (RF_VISITED | RF_SEEN))) continue;
            t = v & RF_TYPE;
            if (c == cur) t = T_MAP_CUR;
            else if (t == RT_BOSS) t = T_MAP_BOSS;
            else if (t == RT_ITEM) t = T_MAP_ITEM;
            else t = (v & RF_VISITED) ? T_MAP_ROOM : T_MAP_SEEN;
            vram_adr(0x2000 + ((unsigned int)(5 + (y << 1)) << 5) + 7 + (x << 1));
            PPU_DATA = t;
            if (x < FM_W - 1) {
                known = fmap[c + 1] & (RF_VISITED | RF_SEEN);
                if ((fmap[c + 1] & RF_TYPE) && known && ((v | fmap[c + 1]) & RF_VISITED))
                    PPU_DATA = T_MAP_H;
            }
            if (y < FM_W - 1) {
                known = fmap[c + FM_W] & (RF_VISITED | RF_SEEN);
                if ((fmap[c + FM_W] & RF_TYPE) && known && ((v | fmap[c + FM_W]) & RF_VISITED)) {
                    vram_adr(0x2000 + ((unsigned int)(6 + (y << 1)) << 5) + 7 + (x << 1));
                    PPU_DATA = T_MAP_V;
                }
            }
        }
    }
    put_str(0x2000 + (25 << 5) + 9, "START TO RESUME");
    oam_begin(); oam_hide_rest();
    ppu_on();
}

static void do_pause(void)
{
    draw_map_screen();
    for (;;) {
        pad_poll();
        if (pad_new & PAD_START) break;
        wait_frame();
    }
    /* the map used the room's nametable, so redraw the room */
    ppu_off();
    mtq_n = 0;
    draw_room();
    ppu_on();
    hud_all();
    msg_timer = 0;
}

static void run_end_screen(void)
{
    unsigned char t;
    ppu_off();
    oam_begin(); oam_hide_rest();
    clear_screen(0x2000);
    if (game_result == GR_WIN) {
        put_str(NT(10, 10), "YOU ESCAPED!");
        put_str(NT(6, 13), "THE UNDERCROFT LIES");
        put_str(NT(6, 14), "BEHIND YOU. WELL DONE.");
    } else {
        put_str(NT(12, 10), "YOU DIED");
        put_str(NT(10, 13), "ON FLOOR ");
        PPU_DATA = '0' + floor_num;
    }
    ppu_on();
    for (t = 0; t < 60; ++t) wait_frame();
    for (;;) {
        pad_poll();
        ++t;
        if ((t & 31) == 0) vbuf_text(NT(10, 20), (t & 32) ? "PRESS START" : "", 11);
        if (pad_new & PAD_START) break;
        wait_frame();
    }
}

/* ============================================================ the game */
static void new_game(void)
{
    pl_maxhp = 6; pl_hp = 6; pl_bombs = 1;
    st_dmg = 2; st_delay = 18; st_range = 36; st_sspd = 3; st_speed = 0x0170; st_flags = 0;
    pl_face = 1; pl_aim = 1; pl_inv = 0; pl_cd = 0;
    item_pool = 0xFF;
    floor_num = 1;
    game_result = GR_NONE;
    scroll_x = 0; scroll_y = 0;
    ppu_ctrl &= 0xFE;
    gen_floor();
    cur = FM_START;
    enter_room(255);
    px8 = pl_x >> 8; py8 = pl_y >> 8;
    show_msg("FIND THE WAY DOWN");
}

static void run_game(void)
{
    unsigned char t;
    for (;;) {
        pad_poll();
        if (pad_new & PAD_START) { do_pause(); continue; }
        px8 = pl_x >> 8; py8 = pl_y >> 8;
        pcx = px8 + 8; pcy = py8 + 10;
        update_player();
        if (game_result) break;
        update_pshots();
        update_enemies();
        update_eshots();
        update_bombs();
        update_pickups();
        update_fx();
        if (!en_count && !(fmap[cur] & RF_CLEARED) && !room_grace) clear_room();
        if (boss_dirty) { boss_dirty = 0; hud_boss(); }
        if (msg_timer && --msg_timer == 0) vbuf_text(NT(2, 2), "", 28);
        mt_flush();
        draw_sprites();
        wait_frame();
        if (game_result) break;
    }
    /* a short pause on death so the hit registers */
    if (game_result == GR_DEAD) {
        for (t = 0; t < 50; ++t) {
            pl_inv = 0;
            draw_sprites();
            wait_frame();
        }
    }
}

void main(void)
{
    memcpy(pal_buf, palette, 32);
    pal_dirty = 1;
    ppu_mask = 0;
    wait_frame();
    for (;;) {
        run_title();
        new_game();
        run_game();
        run_end_screen();
    }
}
