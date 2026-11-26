/* Interface to the assembly runtime in crt0.s */
#ifndef NES_H
#define NES_H

extern unsigned char frame_cnt, nmi_ready, lag_cnt, ppu_ctrl, ppu_mask;
extern unsigned char scroll_x, scroll_y, vram_idx, pal_dirty, pad, pad_new;
extern unsigned int rng_seed;
extern unsigned char oam_idx, spr_x, spr_y, spr_tile, spr_attr;
extern const unsigned char *meta_ptr;
extern unsigned char meta_and, meta_or;
#pragma zpsym("frame_cnt")
#pragma zpsym("nmi_ready")
#pragma zpsym("lag_cnt")
#pragma zpsym("ppu_ctrl")
#pragma zpsym("ppu_mask")
#pragma zpsym("scroll_x")
#pragma zpsym("scroll_y")
#pragma zpsym("vram_idx")
#pragma zpsym("pal_dirty")
#pragma zpsym("pad")
#pragma zpsym("pad_new")
#pragma zpsym("rng_seed")
#pragma zpsym("oam_idx")
#pragma zpsym("spr_x")
#pragma zpsym("spr_y")
#pragma zpsym("spr_tile")
#pragma zpsym("spr_attr")
#pragma zpsym("meta_ptr")
#pragma zpsym("meta_and")
#pragma zpsym("meta_or")

extern unsigned char oam_buf[256];
extern unsigned char vram_buf[160];
extern unsigned char pal_buf[32];

void pad_poll(void);
void wait_frame(void);
unsigned char rand8(void);
void oam_begin(void);
void oam_spr(void);
void oam_meta_spr(void);
void oam_hide_rest(void);

#define PPU_ADDR (*(volatile unsigned char *)0x2006)
#define PPU_DATA (*(volatile unsigned char *)0x2007)
#define APU(a)   (*(volatile unsigned char *)(a))

#define PAD_A      0x80
#define PAD_B      0x40
#define PAD_SELECT 0x20
#define PAD_START  0x10
#define PAD_UP     0x08
#define PAD_DOWN   0x04
#define PAD_LEFT   0x02
#define PAD_RIGHT  0x01

#endif
