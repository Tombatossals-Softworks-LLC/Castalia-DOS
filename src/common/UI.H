/* ===================================================================
 * UI.H  -  Text-mode user-interface helpers for CASTALIA DOS tools
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * A tiny 80x25 VGA text-mode toolkit: direct video-memory writes for
 * speed on a 386SX, BIOS calls for the cursor, and INT 16h for the
 * keyboard.  No dynamic allocation.  C89, Open Watcom / Turbo C.
 *
 * Colours are standard VGA text attributes: low nibble = foreground
 * (0..15), high nibble = background (0..7, or 0..15 with blink off).
 * =================================================================== */

#ifndef CASTALIA_UI_H
#define CASTALIA_UI_H

/* Screen geometry (fixed 80x25 colour text mode). */
#define SCR_W   80
#define SCR_H   25

/* 16 standard VGA colours. */
#define C_BLACK     0
#define C_BLUE      1
#define C_GREEN     2
#define C_CYAN      3
#define C_RED       4
#define C_MAGENTA   5
#define C_BROWN     6
#define C_LGRAY     7
#define C_DGRAY     8
#define C_LBLUE     9
#define C_LGREEN    10
#define C_LCYAN     11
#define C_LRED      12
#define C_LMAGENTA  13
#define C_YELLOW    14      /* Castalia "amber" */
#define C_WHITE     15

/* Build a text attribute byte from foreground/background colours. */
#define UI_ATTR(fg, bg) ((unsigned char)((((bg) & 0x07) << 4) | ((fg) & 0x0F)))

/* Castalia standard palette (blue field, grey/white panels, amber trim). */
#define A_DESKTOP   UI_ATTR(C_LGRAY,  C_BLUE)   /* background field       */
#define A_PANEL     UI_ATTR(C_BLACK,  C_LGRAY)  /* dialog/panel body      */
#define A_PANELHDR  UI_ATTR(C_WHITE,  C_LGRAY)  /* panel heading text     */
#define A_FRAME     UI_ATTR(C_WHITE,  C_BLUE)   /* frame lines on field   */
#define A_TITLE     UI_ATTR(C_YELLOW, C_BLUE)   /* amber titles           */
#define A_ITEM      UI_ATTR(C_LGRAY,  C_BLUE)   /* normal menu item       */
#define A_ITEMSEL   UI_ATTR(C_BLACK,  C_YELLOW) /* selected item bar      */
#define A_HINT      UI_ATTR(C_DGRAY,  C_BLUE)   /* dim hint text          */
#define A_STATUS    UI_ATTR(C_BLACK,  C_LGRAY)  /* bottom status bar      */
#define A_WARN      UI_ATTR(C_YELLOW, C_RED)    /* warning text           */

/* Extended-key codes: 0x100 | BIOS scan code. */
#define KEY_ENTER   13
#define KEY_ESC     27
#define KEY_TAB     9
#define KEY_BKSP    8
#define KEY_SPACE   32
#define KEY_UP      0x148
#define KEY_DOWN    0x150
#define KEY_LEFT    0x14B
#define KEY_RIGHT   0x14D
#define KEY_HOME    0x147
#define KEY_END     0x14F
#define KEY_PGUP    0x149
#define KEY_PGDN    0x151
#define KEY_INS     0x152
#define KEY_DEL     0x153
#define KEY_F1      0x13B
#define KEY_F2      0x13C
#define KEY_F3      0x13D
#define KEY_F4      0x13E
#define KEY_F5      0x13F
#define KEY_F6      0x140
#define KEY_F7      0x141
#define KEY_F8      0x142
#define KEY_F9      0x143
#define KEY_F10     0x144

/* --- Lifecycle ------------------------------------------------------ */
void ui_init(void);                 /* text mode, hide cursor, INT 24h  */
void ui_done(void);                 /* restore cursor and INT 24h       */

/* --- Critical errors (INT 24h) ---------------------------------------
 * Between ui_init() and ui_done() a disk error no longer prints DOS's
 * "Abort, Retry, Ignore, Fail?" over the screen: the DOS call simply
 * fails (fopen returns NULL, fwrite comes up short) and the error code
 * is kept here.  ui_done() puts the previous handler back, so programs
 * started through system() get the normal DOS prompt again.
 *
 * ui_crit_take() returns the code of the last critical error since the
 * previous call (0 write-protected, 2 drive not ready, ... 12 general
 * failure), or -1 if there was none, and clears it.  ui_crit_text()
 * turns a code into a short phrase for a status line. */
int         ui_crit_take(void);
const char *ui_crit_text(int code);

/* --- Drawing -------------------------------------------------------- */
void ui_cls(unsigned char attr);
void ui_putc(int x, int y, char ch, unsigned char attr);
void ui_puts(int x, int y, const char *s, unsigned char attr);
void ui_putlim(int x, int y, const char *s, int maxlen, unsigned char attr);
void ui_fill(int x, int y, int w, int h, char ch, unsigned char attr);
void ui_box(int x, int y, int w, int h, unsigned char attr);   /* single */
void ui_dbox(int x, int y, int w, int h, unsigned char attr);  /* double */
void ui_hline(int x, int y, int w, unsigned char attr);
void ui_center(int y, const char *s, unsigned char attr);

/* --- Cursor / input ------------------------------------------------- */
void ui_gotoxy(int x, int y);
void ui_cursor(int on);
int  ui_getkey(void);               /* ASCII, or 0x100|scancode          */
int  ui_keywaiting(void);           /* 1 if a key sits in the BIOS buffer */

/* --- Timing ----------------------------------------------------------
 * BIOS tick counter at 0040:006C, 18.2 ticks per second.  Wraps at
 * midnight; callers measuring intervals should tolerate one wrap. */
unsigned long ui_ticks(void);

/* Yield to DOS/host while idle (INT 28h).  Our ui_keywaiting() reads the
 * BIOS buffer directly rather than via INT 16h, so a poll loop built on it
 * would otherwise spin a real CPU (or emulator core) at 100%.  Call this
 * once per turn of such a loop so TSRs run and DOSBox/86Box can throttle. */
void ui_idle(void);

/* --- Widgets ---------------------------------------------------------
 * Gradient progress bar: 'permille' (0..1000) of width 'w' is filled
 * with block characters (partial edge cell uses shade characters), the
 * rest is dotted with 'dimattr'.  Used by CASTMARK/CASTCOPY/CASTDOC. */
void ui_hbar(int x, int y, int w, int permille,
             unsigned char attr, unsigned char dimattr);

/* Inline single-line text editor at (x,y) width w.  Edits 'buf' in
 * place (capacity 'maxlen').  Returns 1 on Enter (commit), 0 on Esc.
 * Supports Left/Right/Home/End/Backspace/Del and horizontal scroll. */
int  ui_editline(char *buf, int maxlen, int x, int y, int w);

#endif /* CASTALIA_UI_H */
