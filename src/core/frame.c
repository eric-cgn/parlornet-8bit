/* ParlorNet: the command line, the dialogs and pages of text, for the lobby and the tables (frame.h). */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "frame.h"

#ifdef MACHINE_GRBSS
#pragma bss-name (push, "GRBSS")        /* (machine.h: only while the graphics screen shows) */
#endif
char cmd_said[81];
static char line[41];                   /* one line cut from a longer text (cut) */
static char cmd[CMD_MAX + 2];
static char words_[120];                /* the words of a confirmation, from the parlor's INF lines */

/* The first line of s that fits in w columns, cut at a space, into line; returns the rest of s. */
static const char *__fastcall__ cut(const char *s, unsigned char w)
{
    unsigned char n = strlen(s), c;
    if (n <= w) {
        strcpy(line, s);
        return s + n;
    }
    for (c = w; c && s[c] != ' '; --c)
        ;
    if (!c)
        c = w;
    memcpy(line, s, c);
    line[c] = 0;
    s += c;
    while (*s == ' ')
        ++s;
    return s;
}

/* ------------------------------------------------------------------------------------------------ */
/* Dialogs */

#define DLG_Y 40                        /* the box's top line (a whole row of cells on the C64) */
#define DLG_W 32                        /* the words' width, in columns */

static unsigned char bx[2], bn[2], by;

/* The mark round button i, drawn or taken away (exclusive-or). */
static void __fastcall__ mark(unsigned char i)
{
    hg_xor = 1;
    ui_box(bx[i] * COL_PX - 3, by - 3, (bn[i] + 2) * COL_PX + 6, 14);
    hg_xor = 0;
}

unsigned char __fastcall__ dialog(const char *words, unsigned char how)
{
    static const char *const labels[] = {"Yes", "No", "Accept", "Decline"};
    static const char *s;
    static unsigned char k, n, y, cur, r, keep;
    keep = ui_retkey;
    ui_retkey = 0;
    for (n = 0, s = words; *s; ++n)
        s = cut(s, DLG_W);
    y = n * 8 + 40;                     /* the box: a row above the words, one between them and the buttons */
    ui_hide();
    ui_rect(2, DLG_Y, 36, y, F_BLACK);
    ui_ink(F_WHITE, F_BLACK);
    ui_box(2 * COL_PX + 2, DLG_Y + 2, 36 * COL_PX - 4, y - 4);
    for (y = DLG_Y + 8, s = words; *s; y += 8) {
        s = cut(s, DLG_W);
        ui_text(4, y, line);
    }
    by = y + 8;
    ui_targets_clear();
    k = how & D_OFFER ? 2 : 0;
    bx[0] = 9;
    bx[1] = 22;
    for (n = 0; n < 2; ++n) {
        bn[n] = strlen(labels[k + n]);
        ui_button(bx[n], by, labels[k + n], n);
    }
    cur = how & D_FIRST ? 0 : 1;
    ui_point(cur);
    mark(cur);
    for (;;) {
        k = ui_poll();
        if (t_cur != cur && t_cur < 2) {
            mark(cur);
            cur = t_cur;
            mark(cur);
        }
        if (!k)
            continue;
        if (k == U_CLICK) {
            r = ui_clicked == 0;
            break;
        }
        if (k == K_RET || k == ' ') {
            r = cur == 0;
            break;
        }
        if (k == K_TAB)
            ui_point(cur ^ 1);
        else if ((k | 0x20) == 'y') {
            r = D_YES;
            break;
        } else if ((k | 0x20) == 'n') {
            r = D_NO;
            break;
        } else if (k == K_ESC || k == K_BACK) {
            r = how & D_OFFER ? D_AWAY : D_NO;
            break;
        }
    }
    ui_retkey = keep;
    return r;
}

/* ------------------------------------------------------------------------------------------------ */
/* Pages of text */

#define P_ROWS ((MSG_Y - 16) / 8)       /* the rows of text on a page, from line 16 */
#define P_NEXT 0
#define P_BACK 1
#define P_CLOSE 2

void __fastcall__ pages(const char *title, char **lines, unsigned char n)
{
    static unsigned char first[8];      /* each page's first line */
    static unsigned char np, p, i, k, row, keep;
    static const char *s;
    static char b[48];
    /* where each page starts: at a "\f", or at a line that would not fit below the one before */
    np = 1;
    first[0] = 0;
    row = 0;
    for (i = 0; i < n; ++i) {
        if (lines[i][0] == '\f') {
            if (np < 8)
                first[np++] = i + 1;
            row = 0;
            continue;
        }
        for (k = 0, s = lines[i]; !k || *s; ++k)
            s = cut(s, 40);
        if (row + k > P_ROWS && row && np < 8) {
            first[np++] = i;
            row = 0;
        }
        row += k;
    }
    keep = ui_retkey;
    ui_retkey = 2;                      /* the arrows, Space and Return come as keys */
    p = 0;
    for (;;) {
        ui_cls();
        strcpy(b, title);
        if (np > 1) {
            strcat(b, ", page ");
            strcat(b, utoa10(p + 1));
            strcat(b, " of ");
            strcat(b, utoa10(np));
        }
        ui_ink(F_BLACK, F_WHITE);
        ui_textn(0, 0, b, 32);
        ui_ink(F_WHITE, F_BLACK);
        row = 16;
        for (i = first[p]; i < n && (p + 1 >= np || i < first[p + 1]); ++i) {
            if (lines[i][0] == '\f')
                break;
            s = lines[i];
            do {
                s = cut(s, 40);
                if (row < MSG_Y)
                    ui_text(0, row, line);
                row += 8;
            } while (*s);
        }
        /* the foot: what Space, Left and Esc do here */
        if (p + 1 < np) {
            strcpy(b, "Press Space for page ");
            strcat(b, utoa10(p + 2));
            strcat(b, ", or " KEY_ESC " to close.");
            ui_button(0, BUTTON_Y, "Next page", P_NEXT);
        } else if (p) {
            strcpy(b, "Press Left for page ");
            strcat(b, utoa10(p));
            strcat(b, ", or " KEY_ESC " to close.");
        } else
            strcpy(b, "Press " KEY_ESC " to close.");
        if (p)
            ui_button(13, BUTTON_Y, "Page back", P_BACK);
        ui_button(26, BUTTON_Y, "Close", P_CLOSE);
        ui_status(b);
        ui_point(0);
        do
            k = ui_poll();
        while (!k);
        if (k == U_CLICK)
            k = ui_clicked == P_NEXT ? ' ' : ui_clicked == P_BACK ? K_LEFT : K_ESC;
        if ((k == ' ' || k == K_RIGHT) && p + 1 < np)
            ++p;
        else if (k == K_LEFT && p)
            --p;
        else if (k == K_ESC || k == K_RET || k == K_BACK)
            break;
    }
    ui_retkey = keep;
}

/* ------------------------------------------------------------------------------------------------ */
/* The command line */

void cmd_show(void)
{
    const char *s = cut(cmd_said, 40);
    ui_msg(line);
    if (*s)
        ui_status(s);
}

/* The reply to a command: a confirmation's dialog, or what to show. */
static unsigned char cmd_reply(void)
{
    static unsigned char i, r, out;
    if (net_req() != R_OK) {
        if (!r_final)
            return C_SENT;
        strncpy(cmd_said, err_text(r_final), 80);
        return C_SENT | C_SAID;
    }
    if (r_n && strncmp(r_line[0], "DLG ", 4) == 0) {
        /* DLG kind [table], then the dialog's words: Yes sends CMD [table] YES /kind */
        words(r_line[0]);
        words_[0] = 0;
        for (i = 1; i < r_n; ++i)
            if (strlen(words_) + strlen(r_line[i]) < sizeof words_ - 2) {
                if (i > 1)
                    strcat(words_, " ");
                strcat(words_, r_line[i] + (r_line[i][3] ? 4 : 3));
            }
        out = is(w[1], "quit");
        q_set("CMD");
        if (wn > 2)
            q_add(w[2]);
        q_add("YES /");
        q_cat(w[1]);
        if (dialog(words_, 0) != D_YES)
            return C_REDRAW;
        if (out) {                      /* (signed out: the sign-in screen comes next) */
            net_req();
            return C_OUT | C_REDRAW;
        }
        return cmd_reply() | C_REDRAW;
    }
    for (i = 0; i < r_n; ++i)
        r_line[i] += r_line[i][3] ? 4 : 3;      /* "INF text" -> "text"; "INF" alone, an empty line */
    if (!r_n)
        return C_SENT;
    /* lines that fit the message line and the status line together, joined; more as pages */
    cmd_said[0] = 0;
    for (i = 0; i < r_n && strlen(cmd_said) + strlen(r_line[i]) < sizeof cmd_said - 1; ++i) {
        if (i)
            strcat(cmd_said, " ");
        strcat(cmd_said, r_line[i]);
    }
    if (i == r_n)
        return C_SENT | C_SAID;
    pages(cmd, r_line, r_n);
    return C_SENT | C_REDRAW;
}

unsigned char __fastcall__ cmd_send(unsigned int table, const char *s)
{
    static const char *const own[] = {"help", "leave", "tutorial"};
    static unsigned char i, n;
    if (s != cmd)
        strcpy(cmd, s);
    cmd_said[0] = 0;
    for (n = 1; cmd[n] && cmd[n] != ' '; ++n)
        ;
    for (i = 0; i < 3; ++i)
        if (n - 1 == strlen(own[i]) && strncmp(cmd + 1, own[i], n - 1) == 0)
            return C_HELP << i;
    q_set("CMD");
    if (table)
        q_num(table);
    q_add(cmd);
    return cmd_reply();
}

unsigned char __fastcall__ cmd_line(unsigned int table, void (*lit)(void))
{
    static unsigned char k;
    cmd[0] = '/';
    cmd[1] = 0;
    ui_unlit = 1;
    lit();
    k = ui_input(0, MSG_Y, 40, cmd, CMD_MAX, 0);
    ui_unlit = 0;
    lit();
    ui_msg("");
    if (!k || !cmd[1])
        return C_CLEARED;
    if (cmd[0] != '/') {                /* (the slash taken back and the line typed without it) */
        memmove(cmd + 1, cmd, strlen(cmd) + 1);
        cmd[0] = '/';
    }
    return cmd_send(table, cmd);
}

#ifdef MACHINE_GRBSS
#pragma bss-name (pop)
#endif
