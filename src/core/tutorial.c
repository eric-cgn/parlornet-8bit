/* ParlorNet: the tutorial (an overlay, ovl.h). A game of Tic Tac Toe against the tutorial partner, in 18 steps
   that teach how every game works: the pointer and Return, a shortcut, the command line, a typed move, the
   chat, help and a dialog (docs/design.md in the ParlorNet repository). It runs only on this screen and never
   reaches the parlor: its commands are its own, and the partner's moves are written in. Each step's
   instruction is in the Lesson box and on the message and status lines; a key that does not do what the
   step asks gets it again, after "Not yet." The keys work as at a table, once their step has come. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "game.h"
#include "frame.h"
#include "ovl.h"

#define B_UNDO 20                       /* the panel's Undo button; the squares are 0 to 8 */
#define BX 2                            /* the board: its left column, top line, a square's columns and lines */
#define BY 24
#define SW 5
#define SH 32
#define PX 19                           /* the panel's column */

/* The instructions, the key names put in: "Return" or "RETURN", "Esc" or "RUN/STOP", the command key. */
static const char *const steps[18] = {
    "This tutorial shows how every game works. Press " KEY_RET " to begin.",
    "Move the pointer to the middle square with the arrows.",
    "Press " KEY_RET " to place your X. A click does the same.",
    "Your partner placed an O on a1. Move to c1 and press " KEY_RET ".",
    "Press " OA "U to take back that X. The Undo button does the same.",
    "Your X is gone. Place it on c1 again.",
    "Press / to type a command.",
    "Type time and press " KEY_RET ". The lit keys went dark as you typed.",
    "Press / again, type anything, and press " KEY_ESC " to clear it.",
    "You can type a move too. Press /, type c3 and press " KEY_RET ".",
    "Press " OA "T to open the chat. The Chat button does the same.",
    "Type hello and press " KEY_RET " to say it.",
    "Press " KEY_ESC " to go back to the board.",
    "You can chat from the board too. Type /say good game and " KEY_RET ".",
    "Press ? to open help. Space turns the page, and " KEY_ESC " closes it.",
    "Press " OA "R to see a game ask first, then " KEY_ESC " to answer No.",
    "Move to c2 and press " KEY_RET " to finish your line.",
    "You won. Press " KEY_ESC " to go back. /tutorial runs it again any time."};

/* What each step teaches, for the Lesson box. */
static const char *const teach[18] = {
    "Return", "moving", "placing", "the last move", "a shortcut", "undo", "the command line", "a command",
    "clearing a line", "a typed move", "the chat", "saying a line", "leaving the chat", "chat from the board",
    "help", "a dialog", "the end of a game", "going back"};

static char board[9];
static unsigned char step;              /* 1 to 18 */
static unsigned char held;              /* the message line has an answer until the next key */
static unsigned char done;
static char line[41];
#define CHAT_N 8
static char chat_l[CHAT_N][41];
static unsigned char chat_n;
static char cmd[CMD_MAX + 2];

/* The first line of s that fits in w columns, cut at a space, into line; returns the rest. */
static const char *__fastcall__ cut(const char *s, unsigned char w)
{
    unsigned char i, v = 0, c = 0;      /* (a key's mark, below a space, takes no column) */
    for (i = 0; s[i]; ++i)
        if ((unsigned char)s[i] >= ' ') {
            if (v == w)
                break;
            if (s[i] == ' ')
                c = i;
            ++v;
        }
    if (!s[i] || s[i] == ' ')
        c = i;
    else if (!c)
        c = i;
    memcpy(line, s, c);
    line[c] = 0;
    s += c;
    while (*s == ' ')
        ++s;
    return s;
}

/* A sentence on the message line, and its rest on the status line. */
static void two_lines(const char *s)
{
    s = cut(s, 40);
    ui_msg(line);
    ui_status(s);
}

/* The step's instruction, after "Not yet." when a key did something else. */
static void instruct(unsigned char again)
{
    static char b[96];
    strcpy(b, again ? "Not yet. " : "");
    strcat(b, steps[step - 1]);
    two_lines(b);
    held = 0;
}

/* An answer on the message line, which stays until the next key. */
static void answer(const char *s)
{
    two_lines(s);
    held = 1;
}

static void lesson(void)
{
    static char b[24];
    const char *s;
    unsigned char y;
    ui_rect(PX, 72, 40 - PX, MSG_Y - 88, F_BLACK);     /* (down to the Undo button) */
    ui_ink(F_BLACK, F_WHITE);
    ui_textn(PX, 72, "Lesson", 40 - PX);
    ui_ink(F_WHITE, F_BLACK);
    strcpy(b, "Step ");
    strcat(b, utoa10(step));
    strcat(b, " of 18.");
    ui_text(PX, 80, b);
    strcpy(b, "It teaches ");
    ui_text(PX, 88, b);
    ui_text(PX, 96, teach[step - 1]);
    ui_text(PX + strlen(teach[step - 1]), 96, ".");
    for (y = 112, s = steps[step - 1]; *s && y < MSG_Y - 24; y += 8) {
        s = cut(s, 40 - PX);
        ui_text(PX, y, line);
    }
}

static void next(void)
{
    ++step;
    ui_hide();
    lesson();
    ui_show();
    if (!held)
        instruct(0);
}

static void square(unsigned char i)
{
    static const char *const x[] = {"X X", " X ", "X X"};
    static const char *const o[] = {"OOO", "O O", "OOO"};
    unsigned char c = BX + i % 3 * SW, y = BY + i / 3 * SH, r;
    ui_rect(c, y, SW, SH, F_BLACK);
    ui_box(c * COL_PX + 1, y + 1, SW * COL_PX - 2, SH - 2);
    if (board[i] != ' ')
        for (r = 0; r < 3; ++r)
            ui_text(c + 1, y + 4 + r * 8 + 4, (board[i] == 'X' ? x : o)[r]);
}

static void buttons(void)
{
    unsigned char n = t_n;
    ui_rect(0, BUTTON_Y, 40, 8, F_BLACK);
    ui_button(0, BUTTON_Y, "Chat", B_CHAT);
    ui_button(7, BUTTON_Y, "Help", B_HELP);
    ui_button(14, BUTTON_Y, "Resign", B_RESIGN);
    ui_button(23, BUTTON_Y, "Lobby", B_LOBBY);
    if (ui_unlit)
        t_n = n;
}

static void draw(void)
{
    static const char cols[] = "abc";
    unsigned char i, keep = t_cur < t_n ? t_id[t_cur] : 0;
    ui_cls();
    ui_ink(F_BLACK, F_WHITE);
    ui_textn(0, 0, "Tic Tac Toe tutorial", 32);
    ui_ink(F_WHITE, F_BLACK);
    for (i = 0; i < 3; ++i) {
        line[0] = cols[i];
        line[1] = 0;
        ui_text(BX + i * SW + 2, BY - 8, line);
        line[0] = '1' + i;
        ui_text(0, BY + i * SH + 12, line);
    }
    for (i = 0; i < 9; ++i) {
        square(i);
        ui_target((BX + i % 3 * SW) * COL_PX / 2 + SW * COL_PX / 4, BY + i / 3 * SH + SH / 2, SW * COL_PX / 4 - 1,
                  SH / 2 - 1, i);
    }
    ui_text(PX, 16, "X  you");
    ui_text(PX, 24, "O  tutorial partner");
    ui_text(PX, 40, "Tutorial only.");
    ui_text(PX, 48, "Nothing you do here");
    ui_text(PX, 56, "reaches the parlor.");
    ui_button(PX, MSG_Y - 16, "Undo", B_UNDO);
    buttons();
    lesson();
    ui_point(ui_find(keep));
    instruct(0);
}

/* The partner's O, and a sentence for it. */
static void partner(unsigned char i)
{
    static char b[40];
    board[i] = 'O';
    square(i);
    strcpy(b, "Your partner placed an O on ");
    b[strlen(b) + 2] = 0;
    b[strlen(b) + 1] = '1' + i / 3;
    b[strlen(b)] = 'a' + i % 3;
    strcat(b, ".");
    answer(b);
}

/* An X on square i, where the step asks for it. */
static void place(unsigned char i)
{
    static const unsigned char want[] = {3, 4, 4, 2, 6, 2, 10, 8, 17, 5};
    unsigned char k;
    for (k = 0; k < sizeof want; k += 2)
        if (step == want[k] && i == want[k + 1])
            break;
    if (k == sizeof want) {
        instruct(1);
        return;
    }
    board[i] = 'X';
    ui_hide();
    square(i);
    ui_show();
    if (step == 3)
        partner(0);
    else if (step == 6)
        partner(6);
    else if (step == 10)
        partner(1);
    else if (step == 17) {
        ui_status("Game over. You won.");
        answer("You have three in a row.");
    }
    next();
}

/* The chat: what was said, and a line to say more. Esc, or Return on an empty line, goes back. */
static void chat(void)
{
    static char b[42];
    unsigned char i, y;
    if (step == 11)
        ++step;
    for (;;) {
        ui_cls();
        ui_ink(F_BLACK, F_WHITE);
        ui_textn(0, 0, "Tic Tac Toe tutorial", 32);
        ui_ink(F_WHITE, F_BLACK);
        for (i = 0, y = MSG_Y - 8 * chat_n - 8; i < chat_n; ++i, y += 8)
            ui_text(0, y, chat_l[i]);
        ui_text(0, MSG_Y, "Say:");
        ui_status(step == 12 ? steps[11] : (const char *)("Press " KEY_ESC " to go back to the board."));
        b[0] = 0;
        if (!ui_input(5, MSG_Y, 35, b, 30, 0) || !b[0])
            break;
        if (chat_n + 2 > CHAT_N) {
            memmove(chat_l[0], chat_l[2], (CHAT_N - 2) * 41);
            chat_n -= 2;
        }
        strcpy(chat_l[chat_n], "you: ");
        strcat(chat_l[chat_n++], b);
        strcpy(chat_l[chat_n++], "tutorial partner: Hello to you too!");
        if (step == 12)
            ++step;
    }
    if (step == 13)
        ++step;
    draw();
}

/* A command typed after the slash, as the tutorial answers it. */
static void command(void)
{
    static char b[60];
    unsigned char i, clearing;
    if (step == 7) {
        ++step;
        lesson();                       /* (the lesson says what to type, while the line is open) */
    }
    cmd[0] = '/';
    cmd[1] = 0;
    ui_unlit = 1;
    buttons();
    ui_status(KEY_RET " sends it, and " KEY_ESC " clears it.");
    clearing = step == 9;               /* (the step asks for Esc, so a line sent is not it) */
    i = ui_input(0, MSG_Y, 40, cmd, CMD_MAX, 0);
    ui_unlit = 0;
    buttons();
    lesson();
    if (!i) {
        if (step == 9)
            next();
        else
            instruct(0);
        return;
    }
    if (strcmp(cmd, "/time") == 0) {
        if (signed_in) {
            strcpy(b, "You have been signed in for ");
            for (i = 0; clk_text()[i] == ' '; ++i)
                ;
            strcat(b, clk_text() + i);
            strcat(b, ".");
            answer(b);
        } else
            answer("You aren't signed in to a parlor now.");
        if (step == 8)
            next();
    } else if ((cmd[1] | 0x20) >= 'a' && (cmd[1] | 0x20) <= 'c' && cmd[2] >= '1' && cmd[2] <= '3' && !cmd[3]) {
        i = (cmd[2] - '1') * 3 + (cmd[1] | 0x20) - 'a';
        if (board[i] != ' ')
            answer("That square is taken.");
        else
            place(i);
    } else if (strncmp(cmd, "/say ", 5) == 0) {
        if (chat_n + 2 > CHAT_N) {
            memmove(chat_l[0], chat_l[2], (CHAT_N - 2) * 41);
            chat_n -= 2;
        }
        strcpy(chat_l[chat_n], "you: ");
        strncat(chat_l[chat_n++], cmd + 5, 30);
        strcpy(chat_l[chat_n++], "tutorial partner: Good game!");
        answer("tutorial partner: Good game!");
        if (step == 14)
            next();
    } else if (strcmp(cmd, "/help") == 0)
        done = 2;
    else if (strcmp(cmd, "/leave") == 0)
        done = 1;
    else if (strcmp(cmd, "/tutorial") == 0)
        answer("This is the tutorial already.");
    else
        answer("This is the tutorial, so that command does nothing here.");
    if (clearing)
        instruct(1);
}

static void help(void)
{
    g_help(11);
    if (step == 15)
        ++step;
    draw();
}

static void resign(void)
{
    unsigned char yes;
    strcpy(dlg_words, "Resign this game? Your partner wins. Leaving the table never resigns.");
    yes = dialog(dlg_words, 0);
    if (step == 16)
        ++step;
    draw();
    if (yes == D_YES)
        answer("In a real game, that would resign. Here the game goes on.");
}

static void run(void)
{
    unsigned char k, at = 255;
    memset(board, ' ', sizeof board);
    step = 1;
    chat_n = 0;
    done = 0;
    held = 0;
    ui_retkey = 0;
    gr_mode();
    draw();
    while (!done) {
        k = ui_poll();
        if (t_cur != at) {
            at = t_cur;
            if (step == 2 && at < t_n && t_id[at] == 4)
                next();
        }
        if (!k)
            continue;
        if (held)
            instruct(0);
        if (k == U_CLICK) {
            k = ui_clicked;
            if (step == 1) {
                next();
                continue;
            }
            if (k < 9) {
                if (board[k] != ' ')
                    answer("That square is taken.");
                else
                    place(k);
            } else if (k == B_UNDO)
                k = U_OA | 'U';
            else if (k == B_CHAT)
                k = U_OA | 'T';
            else if (k == B_HELP)
                k = '?';
            else if (k == B_RESIGN)
                k = U_OA | 'R';
            else if (k == B_LOBBY)
                return;
            if (k < 9)
                continue;
        }
        if (k == (U_OA | 'U') && step >= 5) {
            if (step == 5) {
                board[2] = ' ';
                ui_hide();
                square(2);
                ui_show();
                next();
            } else
                answer("There is no step of yours to take back.");
        } else if (k == '/' && step >= 7)
            command();
        else if ((k == (U_OA | 'T') || k == K_TAB) && step >= 11)
            chat();
        else if (k == '?' && step >= 15)
            help();
        else if (k == (U_OA | 'R') && step >= 16)
            resign();
        else if ((k == K_ESC || k == K_BACK) && step == 18)
            return;
        else if (k == K_RET && step == 1)
            next();
        else
            instruct(1);
        if (done == 2) {
            done = 0;
            help();
        }
    }
}

#pragma rodata-name (push, "OVHDR")
const struct tutorial tutorial_header = {run};
#pragma rodata-name (pop)
