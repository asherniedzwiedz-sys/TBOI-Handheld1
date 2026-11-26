; Hand-written hot paths for main.c (16-bit adds that cc65 compiles slowly).

.importzp _ce
.import _en_x, _en_y, _en_vx, _en_vy
.import _es_on, _es_x, _es_y, _es_vx, _es_vy, _es_px, _es_py
.export _en_try, _es_move
.exportzp _try_x, _try_y

MAX_ES = 12             ; must match main.c

.segment "ZEROPAGE"
_try_x: .res 2
_try_y: .res 2

.segment "CODE"

; void en_try(void)
; try_x = en_x[ce] + en_vx[ce];  try_y = en_y[ce] + en_vy[ce]
_en_try:
    lda _ce
    asl a
    tax
    clc
    lda _en_x,x
    adc _en_vx,x
    sta _try_x
    lda _en_x+1,x
    adc _en_vx+1,x
    sta _try_x+1
    clc
    lda _en_y,x
    adc _en_vy,x
    sta _try_y
    lda _en_y+1,x
    adc _en_vy+1,x
    sta _try_y+1
    rts

; void es_move(void)
; Moves every active enemy bullet and stores its pixel position in es_px/es_py.
_es_move:
    ldy #0
    ldx #0
@loop:
    lda _es_on,y
    beq @next
    clc
    lda _es_x,x
    adc _es_vx,x
    sta _es_x,x
    lda _es_x+1,x
    adc _es_vx+1,x
    sta _es_x+1,x
    sta _es_px,y
    clc
    lda _es_y,x
    adc _es_vy,x
    sta _es_y,x
    lda _es_y+1,x
    adc _es_vy+1,x
    sta _es_y+1,x
    sta _es_py,y
@next:
    inx
    inx
    iny
    cpy #MAX_ES
    bne @loop
    rts

; ---------------------------------------------------------------------------
.importzp _bx, _by, _bw, _bh, _bmask
.importzp _en_type, _en_l, _en_r, _en_tp, _en_bt, _en_px, _en_py
.importzp _sl, _sr, _stp, _sbt, _skip_e
.import _room, _mt_flags, _en_ox, _en_oy, _en_w, _en_h
.export _box_hit, _set_box, _shot_find

.segment "ZEROPAGE"
row1:   .res 1
row2:   .res 1
col1:   .res 1
col2:   .res 1
e_tmp:  .res 1
t_ox:   .res 1
t_oy:   .res 1
t_w:    .res 1
t_h:    .res 1

.segment "CODE"

; unsigned char box_hit(void)
; Is the box (bx, by, bw, bh) touching a metatile whose flags match bmask?
; The room is 16 metatiles wide and starts at screen y = 32.
_box_hit:
    lda _by
    sec
    sbc #32
    and #$F0
    sta row1
    lda _by
    clc
    adc _bh
    sec
    sbc #33
    and #$F0
    sta row2
    lda _bx
    lsr a
    lsr a
    lsr a
    lsr a
    sta col1
    lda _bx
    clc
    adc _bw
    sec
    sbc #1
    lsr a
    lsr a
    lsr a
    lsr a
    sta col2

    lda row1
    ora col1
    tax
    ldy _room,x
    lda _mt_flags,y
    and _bmask
    bne @hit
    lda row1
    ora col2
    tax
    ldy _room,x
    lda _mt_flags,y
    and _bmask
    bne @hit
    lda row2
    ora col1
    tax
    ldy _room,x
    lda _mt_flags,y
    and _bmask
    bne @hit
    lda row2
    ora col2
    tax
    ldy _room,x
    lda _mt_flags,y
    and _bmask
    bne @hit
    lda #0
    tax
    rts
@hit:
    lda #1
    ldx #0
    rts

; void __fastcall__ set_box(unsigned char e)
; Caches enemy e's pixel position and hitbox (left, right, top, bottom).
_set_box:
    sta e_tmp
    tay
    ldx _en_type,y
    lda _en_ox,x
    sta t_ox
    lda _en_oy,x
    sta t_oy
    lda _en_w,x
    sta t_w
    lda _en_h,x
    sta t_h
    lda e_tmp
    asl a
    tax
    lda _en_x+1,x
    sta _en_px,y
    clc
    adc t_ox
    sta _en_l,y
    clc
    adc t_w
    sta _en_r,y
    lda _en_y+1,x
    sta _en_py,y
    clc
    adc t_oy
    sta _en_tp,y
    clc
    adc t_h
    sta _en_bt,y
    rts

; unsigned char shot_find(void)
; First live enemy (other than skip_e) whose hitbox overlaps the box
; sl..sr x stp..sbt. Returns 255 if none.
_shot_find:
    ldx #0
@loop:
    lda _en_type,x
    beq @next
    cpx _skip_e
    beq @next
    lda _en_l,x
    cmp _sr
    bcs @next
    lda _sl
    cmp _en_r,x
    bcs @next
    lda _en_tp,x
    cmp _sbt
    bcs @next
    lda _stp
    cmp _en_bt,x
    bcs @next
    txa
    ldx #0
    rts
@next:
    inx
    cpx #8
    bne @loop
    lda #$FF
    ldx #0
    rts
