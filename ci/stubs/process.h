/* ci/stubs/process.h  -  HOST SYNTAX-CHECK STUB ONLY. NOT A REAL DOS HEADER.
 *
 * Open Watcom's <process.h> declares spawnv() and P_WAIT; Linux has no
 * such header.  putenv() lives in Watcom's <stdlib.h>, but glibc hides
 * it under -std=c89, so it is declared here for the gate as well.  Used
 * ONLY by scripts/check.sh via -Ici/stubs; never part of a real build.
 */
#ifndef CASTALIA_STUB_PROCESS_H
#define CASTALIA_STUB_PROCESS_H

#define P_WAIT 0

int spawnv(int mode, const char *path, const char * const *argv);
int putenv(char *env);

#endif /* CASTALIA_STUB_PROCESS_H */
