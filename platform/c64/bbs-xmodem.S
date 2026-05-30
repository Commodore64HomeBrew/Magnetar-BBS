; bbs-xmodem.s — XMODEM/CRC (file mode), after Daryl Rictor / codebase.c64.org
; I/O via C: getc, putc, read_block, write_block

SOH     = $01
EOT     = $04
ACK     = $06
NAK     = $15
ESC     = $1b
CR      = $0d
LF      = $0a

.import _bbs_xmodem_poll
.import _bbs_xmodem_putc
.import _bbs_xmodem_inbyte
.import _bbs_xmodem_read_block
.import _bbs_xmodem_write_block

.export _bbs_xmodem_send
.export _bbs_xmodem_recv

.include "bbs-screen-layout.inc"

.bss
lastblk: .res 1
blkno:   .res 1
errcnt:  .res 1
crc:     .res 1
crch:    .res 1
retry:   .res 1
retry2:  .res 1

.segment "CODE"
GetByte:
        lda #0
        sta retry
GBlp:   jsr _bbs_xmodem_poll
        beq GBto
        lda _bbs_xmodem_inbyte
        sec
        rts
GBto:   dec retry
        bne GBlp
        dec retry2
        bne GBlp
        clc
        rts

PutChr:
        jmp _bbs_xmodem_putc

Flush:
        lda #$70
        sta retry2
Fl1:    jsr GetByte
        bcs Fl1
        rts

; CRC-16-CCITT (XMODEM), init 0 — no 512-byte lookup tables (saves MAIN/BSS arena)
CalcCRC:
        lda #0
        sta crc
        sta crch
        ldy #2
C1:     lda BBS_XMODEM_RBUF_BASE,y
        eor crch
        sta crch
        ldx #8
C2:     asl crc
        rol crch
        bcc C3
        lda crc
        eor #$21
        sta crc
        lda crch
        eor #$10
        sta crch
C3:     dex
        bne C2
        iny
        cpy #$82
        bne C1
        rts

; Send 132 bytes from Rbuf
SndBlk:
        ldx #0
        lda #SOH
        jsr PutChr
S1:     lda BBS_XMODEM_RBUF_BASE,x
        jsr PutChr
        inx
        cpx #$84
        bne S1
        rts

; A=0 ok, $fe abort, $ff error
.export _bbs_xmodem_send
_bbs_xmodem_send:
        lda #0
        sta errcnt
        sta lastblk
        lda #1
        sta blkno
W4C:    lda #$ff
        sta retry2
        jsr GetByte
        bcc W4C
        cmp #'C'
        beq SLoop
        cmp #ESC
        bne W4C
        lda #$fe
        rts
SLoop:  lda lastblk
        beq SBlk
        jmp SDone
SBlk:   inc blkno
        lda blkno
        sta BBS_XMODEM_RBUF_BASE
        eor #$ff
        sta BBS_XMODEM_RBUF_BASE+1
        jsr _bbs_xmodem_read_block
        pha
        jsr CalcCRC
        lda crc
        ldy #130
        sta BBS_XMODEM_RBUF_BASE,y
        iny
        lda crch
        sta BBS_XMODEM_RBUF_BASE,y
        ldx #0
        pla
        bne SLst
SRtry:  jsr SndBlk
        lda #$ff
        sta retry2
        jsr GetByte
        bcc SErr
        cmp #ACK
        beq SLoop
        cmp #NAK
        beq SErr
        cmp #ESC
        bne SErr
        lda #$fe
        rts
SErr:   inc errcnt
        lda errcnt
        cmp #10
        bcc SRtry
        lda #$ff
        rts
SLst:   sta lastblk
        jmp SRtry
SDone:  lda #EOT
        jsr PutChr
        lda #$ff
        sta retry2
        jsr GetByte
        lda #0
        rts

; Receive file blocks
.export _bbs_xmodem_recv
_bbs_xmodem_recv:
        lda #1
        sta blkno
        lda #'C'
        jsr PutChr
        lda #0
        sta errcnt
RTop:   lda #$ff
        sta retry2
        jsr GetByte
        bcc RTop
        cmp #ESC
        bne RGot
        lda #$fe
        rts
RGot:   cmp #SOH
        beq RBlk
        cmp #EOT
        bne RBad
        lda #ACK
        jsr PutChr
        lda #0
        rts
RBad:   jsr Flush
        lda #NAK
        jsr PutChr
        jmp RTop
RBlk:   ldx #0
        lda #$ff
        sta retry2
RB1:    jsr GetByte
        bcc RBad
        sta BBS_XMODEM_RBUF_BASE,x
        inx
        cpx #$84
        bne RB1
        lda BBS_XMODEM_RBUF_BASE
        cmp blkno
        bne RBad
        lda BBS_XMODEM_RBUF_BASE+1
        eor #$ff
        cmp blkno
        bne RBad
        jsr CalcCRC
        lda BBS_XMODEM_RBUF_BASE+130
        cmp crc
        bne RBad
        lda BBS_XMODEM_RBUF_BASE+131
        cmp crch
        bne RBad
        jsr _bbs_xmodem_write_block
        lda #ACK
        jsr PutChr
        inc blkno
        lda #0
        sta errcnt
        jmp RTop
