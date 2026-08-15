/* ci/stubs/conio.h  -  HOST SYNTAX-CHECK STUB ONLY. NOT A REAL DOS HEADER.
 *
 * Minimal <conio.h> so a host compiler can syntax-check sources that pull
 * it in.  Used only by scripts/check.sh; the real console I/O comes from
 * Open Watcom / Turbo C at build time.
 */
#ifndef CASTALIA_STUB_CONIO_H
#define CASTALIA_STUB_CONIO_H

int getch(void);
int getche(void);
int putch(int c);
int kbhit(void);

#endif /* CASTALIA_STUB_CONIO_H */
