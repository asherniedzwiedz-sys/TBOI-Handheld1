; Undercroft - startup code, NMI handler and fast helpers.
; Target: NES / Famiclone, mapper 0 (NROM-256), vertical mirroring.

.export __STARTUP__ : absolute = 1
.import _main
.import __RAM_START__, __RAM_SIZE__
.import copydata, zerobss
.importzp sp

.exportzp _frame_cnt, _nmi_ready, _lag_cnt, _ppu_ctrl, _ppu_mask
.exportzp _scroll_x, _scroll_y, _vram_idx, _pal_dirty, _pad, _pad_new
.exportzp _rng_seed, _oam_idx, _spr_x, _spr_y, _spr_tile, _spr_attr
.exportzp _meta_ptr, _meta_and, _meta_or
.export _oam_buf, _vram_buf, _pal_buf
.export _pad_poll, _wait_frame, _rand8
.export _oam_begin, _oam_spr, _oam_meta_spr, _oam_hide_rest

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014
JOY1      = $4016

.segment "ZEROPAGE"
_frame_cnt: .res 1
_nmi_ready: .res 1
_lag_cnt:   .res 1
_ppu_ctrl:  .res 1
_ppu_mask:  .res 1
_scroll_x:  .res 1
_scroll_y:  .res 1
_vram_idx:  .res 1
_pal_dirty: .res 1
_pad:       .res 1
_pad_new:   .res 1
pad_prev:   .res 1
_rng_seed:  .res 2
_oam_idx:   .res 1
oam_full:   .res 1
_spr_x:     .res 1
_spr_y:     .res 1
_spr_tile:  .res 1
_spr_attr:  .res 1
_meta_ptr:  .res 2
_meta_and:  .res 1
_meta_or:   .res 1

.segment "OAM"
_oam_buf:   .res 256

.segment "BSS"
; VRAM update list: [addr_hi, addr_lo, len, bytes...]*, terminated by 0
_vram_buf:  .res 160
_pal_buf:   .res 32

.segment "HEADER"
    .byte "NES", $1A
    .byte 2             ; 2 x 16 KB PRG-ROM
    .byte 1             ; 1 x 8 KB CHR-ROM
    .byte $01           ; mapper 0, vertical mirroring
    .byte $00
    .byte 0,0,0,0,0,0,0,0

.segment "STARTUP"
reset:
    sei
    cld
    ldx #$40
    stx $4017           ; disable APU frame IRQ
    ldx #$FF
    txs
    inx
    stx PPUCTRL
    stx PPUMASK
    stx $4010           ; no DMC IRQ
    bit PPUSTATUS
@vb1:
    bit PPUSTATUS
    bpl @vb1
    txa
@clr:
    sta $00,x
    sta $0100,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne @clr
    lda #$FF
@clroam:
    sta $0200,x
    inx
    bne @clroam
@vb2:
    bit PPUSTATUS
    bpl @vb2

    lda #<(__RAM_START__ + __RAM_SIZE__)
    sta sp
    lda #>(__RAM_START__ + __RAM_SIZE__)
    sta sp+1
    jsr zerobss
    jsr copydata

    lda #$A5
    sta _rng_seed
    lda #$1C
    sta _rng_seed+1
    lda #$0F
    sta $4015           ; enable pulse 1/2, triangle, noise

    lda #%10001000      ; NMI on, sprites from $1000, BG from $0000
    sta _ppu_ctrl
    sta PPUCTRL
    jmp _main

; ---------------------------------------------------------------------------
; NMI: runs every vblank. Only touches the PPU when the main loop has
; finished a frame (nmi_ready=1), so it never fights with direct PPU writes.
nmi:
    pha
    txa
    pha
    tya
    pha

    lda _nmi_ready
    bne @update
    inc _lag_cnt
    jmp @done

@update:
    lda #0
    sta OAMADDR
    lda #>_oam_buf
    sta OAMDMA

    lda _pal_dirty
    beq @vram
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ldx #0
@pal:
    lda _pal_buf,x
    sta PPUDATA
    inx
    cpx #32
    bne @pal
    lda #0
    sta _pal_dirty

@vram:
    ldx #0
@vloop:
    lda _vram_buf,x
    beq @vdone
    sta PPUADDR
    lda _vram_buf+1,x
    sta PPUADDR
    ldy _vram_buf+2,x
    inx
    inx
    inx
@vbyte:
    lda _vram_buf,x
    sta PPUDATA
    inx
    dey
    bne @vbyte
    jmp @vloop
@vdone:
    lda #0
    sta _vram_idx
    sta _vram_buf

    lda _ppu_ctrl
    sta PPUCTRL
    lda _scroll_x
    sta PPUSCROLL
    lda _scroll_y
    sta PPUSCROLL
    lda _ppu_mask
    sta PPUMASK
    lda #0
    sta _nmi_ready

@done:
    inc _frame_cnt
    pla
    tay
    pla
    tax
    pla
irq:
    rti

; ---------------------------------------------------------------------------
; void wait_frame(void) - hand the finished frame to NMI and wait for it.
_wait_frame:
    lda #1
    sta _nmi_ready
@w:
    lda _nmi_ready
    bne @w
    rts

; void pad_poll(void) - read controller 1 into pad / pad_new.
; Bits: A=$80 B=$40 Select=$20 Start=$10 Up=$08 Down=$04 Left=$02 Right=$01
_pad_poll:
    lda _pad
    sta pad_prev
    lda #1
    sta JOY1
    lda #0
    sta JOY1
    ldx #8
@bit:
    lda JOY1
    and #3
    cmp #1
    rol _pad
    dex
    bne @bit
    lda pad_prev
    eor #$FF
    and _pad
    sta _pad_new
    rts

; unsigned char rand8(void) - 16-bit Galois LFSR, 8 iterations.
_rand8:
    ldy #8
    lda _rng_seed
@r:
    asl a
    rol _rng_seed+1
    bcc @n
    eor #$39
@n:
    dey
    bne @r
    sta _rng_seed
    ldx #0
    rts

; void oam_begin(void) - start building a new sprite list.
_oam_begin:
    lda #0
    sta _oam_idx
    sta oam_full
    rts

; void oam_spr(void) - add one sprite from spr_x/spr_y/spr_tile/spr_attr.
_oam_spr:
    lda oam_full
    bne @full
    ldx _oam_idx
    lda _spr_y
    sta _oam_buf,x
    lda _spr_tile
    sta _oam_buf+1,x
    lda _spr_attr
    sta _oam_buf+2,x
    lda _spr_x
    sta _oam_buf+3,x
    inx
    inx
    inx
    inx
    stx _oam_idx
    bne @full
    inc oam_full
@full:
    rts

; void oam_meta_spr(void) - add a metasprite at spr_x/spr_y.
; meta_ptr -> list of [dx, dy, tile, attr], terminated by dx=$80.
; Final attr = (attr & meta_and) | meta_or (used for hit flashes).
_oam_meta_spr:
    lda oam_full
    bne @end
    ldx _oam_idx
    ldy #0
@loop:
    lda (_meta_ptr),y
    cmp #$80
    beq @save
    clc
    adc _spr_x
    sta _oam_buf+3,x
    iny
    lda (_meta_ptr),y
    clc
    adc _spr_y
    sta _oam_buf,x
    iny
    lda (_meta_ptr),y
    sta _oam_buf+1,x
    iny
    lda (_meta_ptr),y
    and _meta_and
    ora _meta_or
    sta _oam_buf+2,x
    iny
    inx
    inx
    inx
    inx
    bne @loop
    inc oam_full
@save:
    stx _oam_idx
@end:
    rts

; void oam_hide_rest(void) - move unused sprites off screen.
_oam_hide_rest:
    lda oam_full
    bne @done
    ldx _oam_idx
    lda #$FF
@h:
    sta _oam_buf,x
    inx
    inx
    inx
    inx
    bne @h
@done:
    rts

.segment "VECTORS"
    .word nmi, reset, irq

.segment "CHARS"
    .incbin "chr.bin"
