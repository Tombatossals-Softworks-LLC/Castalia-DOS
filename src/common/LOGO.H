/* ===================================================================
 * LOGO.H  -  Shared CASTALIA DOS text-mode logo art
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * One fortress keep and one block-letter wordmark, drawn in CP437 with
 * the Castalia palette (steel-gray walls, amber merlons and flags, lit
 * amber windows, on the blue field).  Shared by BANNER.EXE and SETUP.EXE
 * so the boot splash, the rescue disk, the installer, and the "installed"
 * screen all wear the same face.  See docs/BRANDING.md section 23.9.
 *
 * Drawing is done with the UI toolkit (ui_putc), so a caller must have
 * called ui_init() first.  Nothing here allocates; C89 throughout.
 * =================================================================== */

#ifndef CASTALIA_LOGO_H
#define CASTALIA_LOGO_H

/* The fortress keep: three merlon-topped towers on a curtain wall with a
 * central gate.  Occupies LOGO_KEEP_H rows and up to LOGO_KEEP_W cols. */
#define LOGO_KEEP_W 43
#define LOGO_KEEP_H 12

/* Draw the whole keep with its top-left at (x, y). */
void logo_keep(int x, int y);

/* A compact keep mark for menu / tool headers (the §23.9 Variant A crown):
 * three merlon-topped towers on a gated wall, ~7 rows tall. */
#define LOGO_MARK_W 21
#define LOGO_MARK_H 7
void logo_mark(int x, int y);

/* The compact mark's animation hooks, mirroring the full keep's: redraw
 * just its lit window, or just its pennants, in attribute 'attr'.  Lets a
 * menu or header keep the crown alive (candle flicker, fluttering flags)
 * without repainting the art. */
void logo_mark_windows(int x, int y, unsigned char attr);
void logo_mark_flags(int x, int y, unsigned char attr);

/* Draw a single keep row r (0..LOGO_KEEP_H-1) at (x, y0 + r).  Lets a
 * caller animate the keep rising row by row from the ground up. */
void logo_keep_row(int x, int y0, int r);

/* Redraw just the tower flags (the pennants) in attribute 'attr' - used
 * to flicker them amber/red while the banner holds. */
void logo_keep_flags(int x, int y0, unsigned char attr);

/* Redraw just the lit windows in attribute 'attr' - used to flicker them
 * like candlelight (amber/brown) while the banner holds. */
void logo_keep_windows(int x, int y0, unsigned char attr);

/* Big 5-row block-letter text.  Only A-Z, 0-9 and space are shaped;
 * anything else draws as blank.  Every cell uses 'attr', so pass a
 * foreground-on-blue attribute and the letter gaps stay on the field. */
#define LOGO_BIG_H 5

/* Draw block text at (x, y); returns the width in columns actually used. */
int  logo_bigtext(int x, int y, const char *s, unsigned char attr);

/* Width in columns a block-text string would occupy (for centering). */
int  logo_bigtext_width(const char *s);

/* Draw block text centered on row y. */
void logo_bigtext_center(int y, const char *s, unsigned char attr);

#endif /* CASTALIA_LOGO_H */
