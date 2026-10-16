/* ===================================================================
 * CATALOG.C  -  CATALOG.EXE, the online game catalogue for CASTALIA DOS
 * -------------------------------------------------------------------
 * Part of CASTALIA DOS.  Copyright (c) 2026 The Castalia DOS Project.
 * SPDX-License-Identifier: MIT
 *
 * Browse a list of games published on the internet, download one, and
 * find it in the Game Launcher - from a 386 with an ISA network card.
 *
 *   CATALOG                 browse and install (text-mode UI)
 *   CATALOG /LIST           print the catalogue and exit
 *   CATALOG /INSTALL <ID>   install one game without the UI
 *   CATALOG /URL <url>      use another catalogue than [catalog] url
 *   CATALOG /NONET          the network is already up: load nothing
 *
 * The network is borrowed, not kept.  If no packet driver is resident,
 * the one named in [net] driver of CASTALIA.INI is loaded, DHCP asks
 * for an address, and on the way out TERMIN unloads the driver again:
 * a game started afterwards gets back every byte the driver took.  A
 * driver that was already loaded (by AUTOEXEC, say) is left alone.
 *
 * The TCP/IP work is mTCP's (DHCP.EXE, HTGET.EXE), the card's is a
 * Crynwr packet driver, unpacking is Info-ZIP's UNZIP: all run as
 * child programs with spawnv(), never through COMMAND.COM, so their
 * exit codes come back intact and nothing in the catalogue - which
 * arrived over the network - is ever parsed by a shell.
 *
 * An install is: room in GAMES.INI and on the disk, download, check
 * size and CRC-32 against the index, unpack into C:\GAMES\<ID>, check
 * the program is there, then add the game to GAMES.INI (through
 * SAFEIO, backed up first).  Any step that fails stops the install
 * and says which; GAMES.INI is only touched by the last one.
 *
 * See docs/NETWORK.md.  C89, Open Watcom, large model.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <dos.h>
#include <time.h>

#include "UI.H"
#include "INI.H"
#include "SAFEIO.H"
#include "DIRW.H"
#include "CATIDX.H"
#include "CRC32.H"

#define CAST_INI      "C:\\CASTALIA\\CFG\\CASTALIA.INI"
#define GAMES_BAK     "C:\\CASTALIA\\BACKUP\\GAMES.INI"
#define NET_DIR       "C:\\CASTALIA\\NET"
#define CAT_FILE      NET_DIR "\\CATALOG.TXT"
#define MTCP_CFG      NET_DIR "\\MTCP.CFG"
#define DEFAULT_URL   "http://dos.castaliaos.com/catalog/CATALOG.TXT"

#define MAX_ENTRIES   128
#define LAUNCH_MAX    64        /* LAUNCH.EXE's MAX_GAMES */

/* Exit codes for the command-line modes. */
#define RC_OK         0
#define RC_USAGE      1
#define RC_NET        2
#define RC_INDEX      3
#define RC_INSTALL    4

/* HTGET maps the HTTP status to its exit code: 21 is "200 OK". */
#define HTGET_OK      21

static CAT_ENTRY cat[MAX_ENTRIES];
static char cat_installed[MAX_ENTRIES]; /* cached: checking the disk for every
                                         * row on every key is slow on a 386 */
static int ncat = 0;
static int cat_bad = 0;         /* lines refused by the parser          */
static int cat_over = 0;        /* good lines past MAX_ENTRIES          */

/* Settings, read once from CASTALIA.INI. */
static char cfg_url[CAT_URL_MAX + 1];
static char cfg_driver[96];
static int  cfg_vector = 0x60;
static int  cfg_dhcp = 1;
static char cfg_games_root[64];
static char cfg_games_ini[80];

static int net_ours = 0;        /* we loaded the driver, so we unload it */
static int net_up_done = 0;
static int nonet = 0;
static char last_error[96];

/* --- Small helpers -------------------------------------------------- */

static void copystr(char *dst, const char *src, int size)
{
    strncpy(dst, src, (size_t)size - 1);
    dst[size - 1] = '\0';
}

/* Case-insensitive equality, ASCII only. */
static int same_ci(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z') x = (char)(x - 32);
        if (y >= 'a' && y <= 'z') y = (char)(y - 32);
        if (x != y)
            return 0;
    }
    return *a == '\0' && *b == '\0';
}

static void set_error(const char *a, const char *b)
{
    sprintf(last_error, "%.60s%.34s", a, b ? b : "");
}

static void load_settings(void)
{
    const char *v;

    copystr(cfg_url, DEFAULT_URL, sizeof(cfg_url));
    copystr(cfg_games_root, "C:\\GAMES", sizeof(cfg_games_root));
    copystr(cfg_games_ini, "C:\\CASTALIA\\CFG\\GAMES.INI", sizeof(cfg_games_ini));
    cfg_driver[0] = '\0';

    if (ini_open(CAST_INI) != INI_OK)
        return;
    v = ini_get("catalog", "url");
    if (v != NULL && v[0] != '\0')
        copystr(cfg_url, v, sizeof(cfg_url));
    v = ini_get("net", "driver");
    if (v != NULL)
        copystr(cfg_driver, v, sizeof(cfg_driver));
    v = ini_get("net", "packet_int");
    if (v != NULL && v[0] != '\0') {
        long n = strtol(v, NULL, 16);
        if (n >= 0x60 && n <= 0x80)
            cfg_vector = (int)n;
    }
    cfg_dhcp = ini_get_bool("net", "dhcp", 1);
    v = ini_get("paths", "games");
    if (v != NULL && v[0] != '\0')
        copystr(cfg_games_root, v, sizeof(cfg_games_root));
    v = ini_get("launcher", "games_file");
    if (v != NULL && v[0] != '\0')
        copystr(cfg_games_ini, v, sizeof(cfg_games_ini));
}

/* Run a program and wait for it.  argv[0] is the full path. */
static int run(const char **argv)
{
    int rc;
    fflush(stdout);
    rc = spawnv(P_WAIT, argv[0], (const char * const *)argv);
    return rc;
}

/* --- The network ------------------------------------------------------ */

/* A packet driver's handler starts with a 3-byte jump and then the
 * text "PKT DRVR" - the signature the Packet Driver Specification
 * gives for finding one. */
static int pkt_present(int vector)
{
    static const char sig[] = "PKT DRVR";
    unsigned short far *ivt = (unsigned short far *)MK_FP(0, vector * 4);
    unsigned short ofs = ivt[0], seg = ivt[1];
    char far *h;
    int i;

    if (seg == 0 && ofs == 0)
        return 0;
    h = (char far *)MK_FP(seg, ofs + 3);
    for (i = 0; i < 8; i++)
        if (h[i] != sig[i])
            return 0;
    return 1;
}

/* mTCP finds its settings through %MTCPCFG%; DHCP writes the address
 * it gets into the same file.  Created on first use, so a fresh
 * install needs nothing set up by hand. */
static int ensure_mtcp_cfg(void)
{
    static char env[] = "MTCPCFG=" MTCP_CFG;
    FILE *fp;

    putenv(env);
    if (sio_exists(MTCP_CFG))
        return 0;
    fp = fopen(MTCP_CFG, "wb");
    if (fp == NULL)
        return -1;
    fprintf(fp, "PACKETINT 0x%X\r\nHOSTNAME CASTALIA\r\n", cfg_vector);
    return sio_close(fp) == 0 ? 0 : -1;
}

/* Split "NE2000.COM 0x60 10 0x300" into argv; a program named without a
 * path is looked for in C:\CASTALIA\NET. */
static int driver_argv(char *line, const char **argv, int max, char *prog,
                       int progsize)
{
    int n = 0;
    char *p = line, *word;

    while (*p && n < max - 1) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;
        word = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
        argv[n++] = word;
    }
    argv[n] = NULL;
    if (n == 0)
        return -1;
    if (strchr(argv[0], '\\') == NULL && strchr(argv[0], ':') == NULL) {
        if ((int)(strlen(NET_DIR) + strlen(argv[0]) + 2) > progsize)
            return -1;
        sprintf(prog, "%s\\%s", NET_DIR, argv[0]);
        argv[0] = prog;
    }
    return 0;
}

/* Bring the network up.  0 on success; on failure last_error says why.
 * Runs outside the UI: the driver and DHCP print as they go. */
static int net_up(void)
{
    static char line[96], prog[80];
    const char *argv[12];
    const char *dhcp[3];
    int rc;

    if (net_up_done)
        return 0;
    if (ensure_mtcp_cfg() != 0) {
        set_error("Cannot write ", MTCP_CFG);
        return -1;
    }
    if (nonet || pkt_present(cfg_vector)) {
        net_up_done = 1;
        return 0;
    }
    if (cfg_driver[0] == '\0') {
        set_error("No network card set up: see [net] in CASTALIA.INI", NULL);
        return -1;
    }
    copystr(line, cfg_driver, sizeof(line));
    if (driver_argv(line, argv, 12, prog, sizeof(prog)) != 0) {
        set_error("Cannot read [net] driver = ", cfg_driver);
        return -1;
    }
    printf("Loading the network driver: %s\n", cfg_driver);
    rc = run(argv);
    if (rc == -1 || !pkt_present(cfg_vector)) {
        set_error("The network driver did not load. Check its IRQ and port: ",
                  cfg_driver);
        return -1;
    }
    net_ours = 1;

    if (cfg_dhcp) {
        printf("Asking the network for an address (DHCP)...\n");
        dhcp[0] = NET_DIR "\\DHCP.EXE";
        dhcp[1] = NULL;
        rc = run(dhcp);
        if (rc != 0) {
            set_error("No address from DHCP. Is the cable in, the router on?",
                      NULL);
            return -1;
        }
    }
    net_up_done = 1;
    return 0;
}

/* Unload the driver if this program loaded it. */
static void net_down(void)
{
    static char vec[8];
    const char *argv[3];

    if (!net_ours)
        return;
    sprintf(vec, "0x%X", cfg_vector);
    argv[0] = NET_DIR "\\TERMIN.COM";
    argv[1] = vec;
    argv[2] = NULL;
    run(argv);
    net_ours = 0;
    net_up_done = 0;
}

/* HTGET picks its local TCP port as 2048 + rand(), and mTCP seeds rand()
 * with time(NULL) - whole seconds.  Two HTGETs started in the same second
 * therefore connect from the same port to the same server, and the second
 * SYN meets the first connection while it is still closing: it is dropped
 * and HTGET gives up after ten seconds.  An index fetch followed at once
 * by the game's download is exactly that, so a new HTGET never starts in
 * the second the previous one did. */
static void next_second(void)
{
    static time_t last = 0;
    while (time(NULL) == last)
        ;
    last = time(NULL);
}

/* Fetch 'url' into 'file'.  0 on success, else last_error is set. */
static int fetch(const char *url, const char *file, int quiet)
{
    const char *argv[7];
    int n = 0, rc, attempt;

    argv[n++] = NET_DIR "\\HTGET.EXE";
    if (quiet)
        argv[n++] = "-quiet";
    argv[n++] = "-o";
    argv[n++] = file;
    argv[n++] = url;
    argv[n] = NULL;
    /* One retry for a plain network failure (code 1): a dropped packet
     * on a congested link is not worth giving up a download for.  An
     * answer from the server, whatever it says, is not retried. */
    for (attempt = 0; attempt < 2; attempt++) {
        remove(file);
        next_second();
        rc = run(argv);
        if (rc != 1)
            break;
    }
    if (rc == HTGET_OK)
        return 0;
    remove(file);
    if (rc == -1)
        set_error("HTGET.EXE is missing from ", NET_DIR);
    else if (rc >= 30 && rc < 40)
        /* HTGET does not follow redirects, and the usual one is to
         * https://, which no 386 can speak.  Say so plainly. */
        set_error("The server redirected the request (to https?): ", url);
    else if (rc >= 40 && rc < 50)
        set_error("The server has no such file: ", url);
    else if (rc >= 50 && rc < 60)
        set_error("The server failed to answer: ", url);
    else {
        char what[40];
        sprintf(what, "The download failed (HTGET code %d): ", rc);
        set_error(what, url);
    }
    return -1;
}

/* --- The catalogue ----------------------------------------------------- */

/* Read CATALOG.TXT.  0 on success (even if some lines were refused,
 * which cat_bad counts); -1 if the file is not a catalogue at all. */
static int load_catalogue(const char *path)
{
    static char line[CAT_LINE_MAX + 2];
    FILE *fp;
    int header = 0, rc;
    CAT_ENTRY e;

    ncat = cat_bad = cat_over = 0;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        set_error("Cannot open ", path);
        return -1;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* A line longer than any valid one: drop the rest of it, and
         * the line with it, rather than read its tail as a new line. */
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            int c;
            while ((c = fgetc(fp)) != EOF && c != '\n')
                ;
            cat_bad++;
            continue;
        }
        if (!header) {
            char *t = line;
            while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n')
                t++;
            if (*t == '\0')
                continue;
            if (!cat_header_ok(t)) {
                fclose(fp);
                set_error("Not a Castalia catalogue: ", path);
                return -1;
            }
            header = 1;
            continue;
        }
        rc = cat_parse_line(line, &e);
        if (rc == CAT_SKIP)
            continue;
        if (rc != CAT_OK) {
            cat_bad++;
            continue;
        }
        if (ncat >= MAX_ENTRIES) {
            cat_over++;
            continue;
        }
        cat[ncat++] = e;
    }
    fclose(fp);
    if (!header) {
        set_error("The catalogue is empty: ", path);
        return -1;
    }
    return 0;
}

static int find_entry(const char *id)
{
    int i;
    for (i = 0; i < ncat; i++)
        if (same_ci(cat[i].id, id))
            return i;
    return -1;
}

static void game_dir(const CAT_ENTRY *e, char *out)
{
    sprintf(out, "%.50s\\%s", cfg_games_root, e->id);
}

static int installed(const CAT_ENTRY *e)
{
    char path[80];
    game_dir(e, path);
    strcat(path, "\\");
    strcat(path, e->exe);
    return sio_exists(path);
}

static void scan_installed(void)
{
    int i;
    for (i = 0; i < ncat; i++)
        cat_installed[i] = (char)installed(&cat[i]);
}

/* Free bytes on the drive of 'path' (INT 21h AH=36h), or 0 if unknown. */
static unsigned long disk_free(const char *path)
{
    union REGS r;
    char d = path[0];

    if (d >= 'a' && d <= 'z')
        d = (char)(d - 32);
    if (d < 'A' || d > 'Z' || path[1] != ':')
        return 0;
    r.h.ah = 0x36;
    r.h.dl = (unsigned char)(d - 'A' + 1);
    int86(0x21, &r, &r);
    if (r.x.ax == 0xFFFF)
        return 0;
    return (unsigned long)r.x.ax * (unsigned long)r.x.bx *
           (unsigned long)r.x.cx;
}

/* --- GAMES.INI -------------------------------------------------------- */

/* What adding 'e' would do to GAMES.INI: 1 if it is already there, 0 if
 * it can be added, -1 if it cannot (last_error says why). */
static int games_ini_check(const CAT_ENTRY *e)
{
    int i, n, games = 0, rc;
    long bytes;
    FILE *fp;

    rc = ini_open(cfg_games_ini);
    if (rc == INI_ERR_OPEN)
        return 0;                   /* a new GAMES.INI will be written */
    if (rc != INI_OK) {
        set_error("GAMES.INI is full or unreadable: ", cfg_games_ini);
        return -1;
    }
    if (ini_get(e->id, "exe") != NULL)
        return 1;
    n = ini_section_count();
    for (i = 0; i < n; i++)
        if (ini_get(ini_section_name(i), "exe") != NULL)
            games++;
    if (games >= LAUNCH_MAX || n >= INI_MAX_SECTIONS) {
        set_error("The Game Launcher already holds 64 games.", NULL);
        return -1;
    }
    /* The launcher reads GAMES.INI into a 16 KB buffer; a file grown
     * past it would not load at all, losing every game, not just the
     * new one. */
    fp = fopen(cfg_games_ini, "rb");
    if (fp == NULL)
        return 0;
    fseek(fp, 0L, SEEK_END);
    bytes = ftell(fp);
    fclose(fp);
    if (bytes + 400L >= (long)INI_MAX_SIZE) {
        set_error("GAMES.INI is too large to add another game.", NULL);
        return -1;
    }
    return 0;
}

/* Append 'e' to GAMES.INI, keeping everything already in it (comments
 * included): a new file is written beside it and swapped in whole. */
static int games_ini_add(const CAT_ENTRY *e)
{
    static char buf[1024];
    char tmp[SIO_PATH], dir[80];
    FILE *in, *out;
    size_t n;
    int last = '\n';

    if (sio_tmpname(cfg_games_ini, tmp) != SIO_OK) {
        set_error("Path too long: ", cfg_games_ini);
        return -1;
    }
    out = fopen(tmp, "wb");
    if (out == NULL) {
        set_error("Cannot write next to ", cfg_games_ini);
        return -1;
    }
    in = fopen(cfg_games_ini, "rb");
    if (in != NULL) {
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
            fwrite(buf, 1, n, out);
            last = buf[n - 1];
        }
        fclose(in);
    }
    if (last != '\n')
        fprintf(out, "\r\n");
    game_dir(e, dir);
    fprintf(out, "\r\n[%s]\r\n", e->id);
    fprintf(out, "name        = %s\r\n", e->name);
    fprintf(out, "path        = %s\r\n", dir);
    fprintf(out, "exe         = %s\r\n", e->exe);
    fprintf(out, "profile     = %s\r\n", e->profile);
    fprintf(out, "sound       = %s\r\n", e->sound);
    fprintf(out, "mouse       = %s\r\n", e->mouse ? "yes" : "no");
    fprintf(out, "requires_cd = no\r\n");
    fprintf(out, "notes       = %s\r\n", e->notes);
    if (sio_close(out) != 0) {
        remove(tmp);
        set_error("The disk is full: GAMES.INI was not changed.", NULL);
        return -1;
    }
    if (sio_exists(cfg_games_ini))
        sio_copy(cfg_games_ini, GAMES_BAK);
    if (sio_replace(tmp, cfg_games_ini) != SIO_OK) {
        set_error("Could not replace ", cfg_games_ini);
        return -1;
    }
    return 0;
}

/* --- Installing --------------------------------------------------------- */

static void show_check(unsigned long done)
{
    printf("\r  Checking   %lu KB", done / 1024UL);
}

/* Install catalogue entry 'i'.  Runs outside the UI and prints what it
 * does.  0 on success; on failure last_error says why. */
static int install(int i)
{
    const CAT_ENTRY *e = &cat[i];
    char zip[80], dir[80], exe[96];
    const char *argv[8];
    unsigned long crc, size, need, have;
    int known, rc;

    printf("\nInstalling %s (%s)\n", e->name, e->id);

    /* 1. Room in the launcher, before anything is downloaded. */
    known = games_ini_check(e);
    if (known < 0)
        return -1;

    /* 2. Room on the disk: the ZIP and what it unpacks to, together. */
    game_dir(e, dir);
    need = e->size + e->disk_kb * 1024UL + 65536UL;
    have = disk_free(dir);
    if (have != 0 && have < need) {
        sprintf(last_error, "Not enough disk space: %lu KB needed, %lu KB free.",
                need / 1024UL, have / 1024UL);
        return -1;
    }

    /* 3. Download. */
    sprintf(zip, "%s\\%s.ZIP", NET_DIR, e->id);
    printf("  Downloading %lu KB from %s\n", (e->size + 1023UL) / 1024UL, e->url);
    if (fetch(e->url, zip, 0) != 0)
        return -1;

    /* 4. What arrived is what the index promised, before it is opened. */
    rc = crc32_file(zip, &crc, &size, show_check);
    printf("\n");
    if (rc != 0) {
        remove(zip);
        set_error("Cannot read the download back: ", zip);
        return -1;
    }
    if (size != e->size || crc != e->crc) {
        remove(zip);
        sprintf(last_error, "The download is damaged (%lu bytes, CRC %08lX).",
                size, crc);
        return -1;
    }
    printf("  Size and CRC-32 match the catalogue.\n");

    /* 5. Unpack.  Info-ZIP 6 drops "../" and drive letters from the
     * names inside an archive, so nothing lands outside 'dir'. */
    dirw_mkdir(cfg_games_root);
    dirw_mkdir(dir);
    sprintf(exe, "%s\\UNZIP.EXE", NET_DIR);
    argv[0] = exe;
    argv[1] = "-o";
    argv[2] = "-qq";
    argv[3] = zip;
    argv[4] = "-d";
    argv[5] = dir;
    argv[6] = NULL;
    printf("  Unpacking into %s\n", dir);
    rc = run(argv);
    remove(zip);
    if (rc == -1) {
        set_error("UNZIP.EXE is missing from ", NET_DIR);
        return -1;
    }
    if (rc != 0 && rc != 1) {     /* 1 = warnings, files are there */
        sprintf(last_error, "UNZIP failed (code %d) in %.40s", rc, dir);
        return -1;
    }
    if (!installed(e)) {
        sprintf(last_error, "%s is not in the archive.", e->exe);
        return -1;
    }

    /* 6. Into the launcher. */
    if (known == 0) {
        if (games_ini_add(e) != 0)
            return -1;
        printf("  Added to the Game Launcher.\n");
    } else {
        printf("  Already in the Game Launcher: files refreshed.\n");
    }
    printf("Done.\n");
    return 0;
}

/* Fetch and read the index.  RC_OK, RC_NET or RC_INDEX. */
static int refresh(const char *url)
{
    if (net_up() != 0)
        return RC_NET;
    printf("Fetching the catalogue from %s\n", url);
    if (fetch(url, CAT_FILE, 1) != 0)
        return RC_NET;
    if (load_catalogue(CAT_FILE) != 0)
        return RC_INDEX;
    scan_installed();
    return RC_OK;
}

/* --- The screen --------------------------------------------------------- */

#define LIST_X    2
#define LIST_Y    2
#define LIST_W    40
#define LIST_H    20
#define LIST_ROWS (LIST_H - 2)
#define DET_X     43
#define DET_Y     2
#define DET_W     35
#define DET_H     20

static void draw_frame(void)
{
    char right[48];

    ui_cls(A_DESKTOP);
    ui_fill(0, 0, SCR_W, 1, ' ', A_TITLE);
    ui_puts(2, 0, "CASTALIA DOS", A_TITLE);
    ui_puts(15, 0, "Game Catalogue", UI_ATTR(C_WHITE, C_BLUE));
    sprintf(right, "%d games", ncat);
    ui_puts(SCR_W - (int)strlen(right) - 2, 0, right, A_TITLE);

    ui_box(LIST_X, LIST_Y, LIST_W, LIST_H, A_FRAME);
    ui_puts(LIST_X + 2, LIST_Y, " Online ", A_TITLE);
    ui_box(DET_X, DET_Y, DET_W, DET_H, A_FRAME);
    ui_puts(DET_X + 2, DET_Y, " Details ", A_TITLE);

    if (cat_bad > 0 || cat_over > 0) {
        sprintf(right, " %d line(s) skipped ", cat_bad + cat_over);
        ui_puts(LIST_X + LIST_W - (int)strlen(right) - 2, LIST_Y + LIST_H - 1,
                right, A_HINT);
    }
    ui_fill(0, SCR_H - 1, SCR_W, 1, ' ', A_STATUS);
    ui_puts(1, SCR_H - 1,
        " \x18\x19 Move   Enter Install   R Refresh   Esc Quit", A_STATUS);
}

static void draw_list(int sel, int top)
{
    int row, i, inner = LIST_W - 2;
    char line[64];
    unsigned char attr;

    for (row = 0; row < LIST_ROWS; row++) {
        i = top + row;
        ui_fill(LIST_X + 1, LIST_Y + 1 + row, inner, 1, ' ', A_ITEM);
        if (i >= ncat)
            continue;
        attr = (i == sel) ? A_ITEMSEL : A_ITEM;
        ui_fill(LIST_X + 1, LIST_Y + 1 + row, inner, 1, ' ', attr);
        if (cat[i].year)
            sprintf(line, "%c %-29.29s %4d", cat_installed[i] ? '\xFB' : ' ',
                    cat[i].name, cat[i].year);
        else
            sprintf(line, "%c %-29.29s", cat_installed[i] ? '\xFB' : ' ',
                    cat[i].name);
        ui_putlim(LIST_X + 2, LIST_Y + 1 + row, line, inner - 2, attr);
    }
    ui_putc(LIST_X + LIST_W - 2, LIST_Y + 1,
            (char)(top > 0 ? 0x1E : 0x20), A_FRAME);
    ui_putc(LIST_X + LIST_W - 2, LIST_Y + LIST_H - 2,
            (char)(top + LIST_ROWS < ncat ? 0x1F : 0x20), A_FRAME);
}

static void draw_wrapped(int x, int y, int w, int maxlines, const char *s,
                         unsigned char attr)
{
    char word[40];
    int col = 0, line = 0, i = 0, wl;

    while (s[i] != '\0' && line < maxlines) {
        wl = 0;
        while (s[i] == ' ')
            i++;
        while (s[i] != '\0' && s[i] != ' ' && wl < (int)sizeof(word) - 1)
            word[wl++] = s[i++];
        word[wl] = '\0';
        if (wl == 0)
            continue;
        if (col > 0 && col + 1 + wl > w) {
            col = 0;
            if (++line >= maxlines)
                break;
        }
        if (col > 0)
            ui_putc(x + col++, y + line, ' ', attr);
        ui_putlim(x + col, y + line, word, w - col, attr);
        col += wl;
    }
}

static void draw_detail(int sel)
{
    int x = DET_X + 2, y = DET_Y + 2, w = DET_W - 4;
    char buf[64];
    const CAT_ENTRY *e;

    ui_fill(DET_X + 1, DET_Y + 1, DET_W - 2, DET_H - 2, ' ', A_DESKTOP);
    if (ncat == 0) {
        ui_puts(x, y, "The catalogue is empty.", A_HINT);
        return;
    }
    e = &cat[sel];
    ui_putlim(x, y, e->name, w, UI_ATTR(C_YELLOW, C_BLUE));
    y += 2;
    sprintf(buf, "Genre   : %s", e->genre[0] ? e->genre : "-");
    ui_putlim(x, y++, buf, w, A_ITEM);
    if (e->year) {
        sprintf(buf, "Year    : %d", e->year);
        ui_putlim(x, y++, buf, w, A_ITEM);
    }
    sprintf(buf, "Needs   : %s%d or better", e->cpu == 86 ? "80" : "", e->cpu);
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "Download: %lu KB", (e->size + 1023UL) / 1024UL);
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "On disk : %lu KB", e->disk_kb);
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "Profile : %s", e->profile);
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "Sound   : %s   Mouse: %s", e->sound, e->mouse ? "yes" : "no");
    ui_putlim(x, y++, buf, w, A_ITEM);
    sprintf(buf, "Licence : %s", e->license);
    ui_putlim(x, y++, buf, w, A_ITEM);
    y++;
    draw_wrapped(x, y, w, DET_Y + DET_H - 4 - y, e->notes, A_ITEM);
    ui_puts(x, DET_Y + DET_H - 3,
            cat_installed[sel] ? "\xFB Installed" : "  Not installed",
            cat_installed[sel] ? UI_ATTR(C_LGREEN, C_BLUE) : A_HINT);
}

/* A one-box message; returns the key that closed it. */
static int message(const char *l1, const char *l2, const char *l3)
{
    int w = 64, x = (SCR_W - 64) / 2, y = 8;
    ui_fill(x, y, w, 7, ' ', A_PANEL);
    ui_dbox(x, y, w, 7, A_PANEL);
    ui_putlim(x + 3, y + 2, l1, w - 6, A_PANELHDR);
    if (l2)
        ui_putlim(x + 3, y + 3, l2, w - 6, A_PANEL);
    if (l3)
        ui_putlim(x + 3, y + 4, l3, w - 6, A_PANEL);
    return ui_getkey();
}

/* Leave the UI for a step that prints as it goes: a clean screen, the
 * cursor at the top, so the output does not land on the list. */
static void leave_ui(void)
{
    ui_cls(UI_ATTR(C_LGRAY, C_BLACK));
    ui_done();                  /* parks the cursor on the last row */
    ui_gotoxy(0, 0);
}

static void pause_key(void)
{
    printf("\nPress any key to return to the catalogue.");
    fflush(stdout);
    ui_getkey();
}

static int browse(void)
{
    int sel = 0, top = 0, key, k;
    char l1[80], l2[80];

    for (;;) {
        draw_frame();
        draw_list(sel, top);
        draw_detail(sel);
        key = ui_getkey();
        if (key == KEY_ESC)
            return RC_OK;
        if (key == KEY_UP && sel > 0)
            sel--;
        else if (key == KEY_DOWN && sel < ncat - 1)
            sel++;
        else if (key == KEY_PGUP)
            sel = sel > LIST_ROWS ? sel - LIST_ROWS : 0;
        else if (key == KEY_PGDN)
            sel = sel + LIST_ROWS < ncat ? sel + LIST_ROWS : (ncat ? ncat - 1 : 0);
        else if (key == KEY_HOME)
            sel = 0;
        else if (key == KEY_END && ncat > 0)
            sel = ncat - 1;
        else if (key == 'r' || key == 'R') {
            leave_ui();
            if (refresh(cfg_url) != RC_OK) {
                printf("\n%s\n", last_error);
                pause_key();
            }
            ui_init();
            sel = top = 0;
        } else if (key == KEY_ENTER && ncat > 0) {
            sprintf(l1, "Install %.40s?", cat[sel].name);
            sprintf(l2, "%lu KB download, %lu KB on disk, into %.24s\\%s",
                    (cat[sel].size + 1023UL) / 1024UL, cat[sel].disk_kb,
                    cfg_games_root, cat[sel].id);
            k = message(l1, l2, "Enter/Y = install    Esc/N = cancel");
            if (k == KEY_ENTER || k == 'y' || k == 'Y') {
                leave_ui();
                if (install(sel) != 0)
                    printf("\nNot installed: %s\n", last_error);
                else
                    printf("\nFind it in the Game Launcher (GAMES).\n");
                cat_installed[sel] = (char)installed(&cat[sel]);
                pause_key();
                ui_init();
            }
        }
        if (sel < top)
            top = sel;
        if (sel >= top + LIST_ROWS)
            top = sel - LIST_ROWS + 1;
    }
}

/* --- Entry points -------------------------------------------------------- */

static void usage(void)
{
    printf("CASTALIA DOS Game Catalogue\n\n"
           "  CATALOG                 browse and install\n"
           "  CATALOG /LIST           print the catalogue\n"
           "  CATALOG /INSTALL <ID>   install one game\n"
           "  CATALOG /URL <url>      use another catalogue\n"
           "  CATALOG /NONET          use a network that is already up\n");
}

static int list_mode(void)
{
    int i;
    for (i = 0; i < ncat; i++)
        printf("%-8s %-40s %4d %6lu KB %s%s\n", cat[i].id, cat[i].name,
               cat[i].year, (cat[i].size + 1023UL) / 1024UL, cat[i].license,
               cat_installed[i] ? "  [installed]" : "");
    printf("%d game(s)", ncat);
    if (cat_bad || cat_over)
        printf(", %d line(s) skipped", cat_bad + cat_over);
    printf("\n");
    return RC_OK;
}

int main(int argc, char **argv)
{
    static char tz_utc[] = "TZ=UTC0";
    const char *url = NULL, *want = NULL;
    int do_list = 0, i, rc = RC_OK;

    for (i = 1; i < argc; i++) {
        if (same_ci(argv[i], "/LIST"))
            do_list = 1;
        else if (same_ci(argv[i], "/INSTALL") && i + 1 < argc)
            want = argv[++i];
        else if (same_ci(argv[i], "/URL") && i + 1 < argc)
            url = argv[++i];
        else if (same_ci(argv[i], "/NONET"))
            nonet = 1;
        else {
            usage();
            return RC_USAGE;
        }
    }

    /* mTCP and UNZIP both complain, on every run, when TZ is unset; the
     * PC clock keeps local time with no zone, which UTC0 describes. */
    if (getenv("TZ") == NULL)
        putenv(tz_utc);

    load_settings();
    if (url != NULL)
        copystr(cfg_url, url, sizeof(cfg_url));

    rc = refresh(cfg_url);
    if (rc != RC_OK) {
        printf("\n%s\n", last_error);
        net_down();
        /* Run from the menu, the menu would redraw over the reason at
         * once; the command-line modes are for scripts and do not wait. */
        if (!do_list && want == NULL) {
            printf("\nHelp: \"Game Catalogue and Network\" (main menu, 9)."
                   "\nPress any key.");
            fflush(stdout);
            ui_getkey();
        }
        return rc;
    }

    if (do_list) {
        rc = list_mode();
    } else if (want != NULL) {
        i = find_entry(want);
        if (i < 0) {
            printf("No game with id %s in the catalogue.\n", want);
            rc = RC_INSTALL;
        } else if (install(i) != 0) {
            printf("Not installed: %s\n", last_error);
            rc = RC_INSTALL;
        }
    } else {
        ui_init();
        rc = browse();
        ui_done();
    }

    net_down();
    return rc;
}
