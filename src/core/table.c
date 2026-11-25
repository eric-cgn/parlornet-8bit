/* ParlorNet: a table. The view is fetched with VUE and drawn by the game; a move is
   sent with MOV and the view is fetched again, so the screen shows only what the server confirms. While
   it is another player's move, SIN asks every few seconds what has happened. */
#include <string.h>
#include "machine.h"
#include "net.h"
#include "app.h"
#include "game.h"
#include "games.h"
#include "ovl.h"

unsigned int g_table;
signed char g_seat;
unsigned char g_seats;
char g_names[8][13];
signed char g_turn;
unsigned char g_over, g_full, g_started, g_myturn;
char g_status[42];
char g_rules[13];
int g_score[8];
unsigned char g_bot;
unsigned char m_n;
#ifdef MACHINE_LOWBSS
/* On the IIgs, which has no serial ring, its place in low memory (parlorgs.cfg) keeps these; the program's
   own space is full. */
#pragma bss-name (push, "LOWBSS")
#endif
char *m_tok[MAXM];
static char part[42];
#ifdef MACHINE_LOWBSS
#pragma bss-name (pop)
#endif
char g_chkinfo[42];
unsigned char g_reply_more, g_reply_done;

/* The move tokens' text and the chat, in the language card's RAM above the overlays (TABLE in parlor.cfg). */
#pragma bss-name (push, "TABLE")
static char mbuf[640];
/* Chat: the last CHAT_N lines said at the table ("name: text"), oldest first. */
#define CHAT_N 12
#define CHAT_W 72
static char chat_l[CHAT_N][CHAT_W];
#pragma bss-name (pop)
static unsigned int mused;
/* The message line shows chat (chat_on): line chat_tk, its part chat_pg (a line longer than the screen is
   two). New lines step past on it (chat_run), a part every 3 seconds, until a key or a click; the newest
   then stays. ev_seq is where SIN asks from (the server's own mark moves on with VUE, which would skip
   chat), v_seq the view's: events up to it are drawn already. */
static unsigned char chat_n, chat_on, chat_run, chat_tk, chat_pg, part_pg, in_view, stale;
static unsigned int chat_at, ev_seq, v_seq;
static const struct game *G;
static unsigned char clk_seat, have_clk, view_g;
static unsigned long clk_secs;
static unsigned int clk_at;
static char gtype[16];
static unsigned char hover_at = 255;

unsigned char __fastcall__ m_has(const char *tok)
{
    unsigned char i;
    for (i = 0; i < m_n; ++i)
        if (strcmp(m_tok[i], tok) == 0)
            return 1;
    return 0;
}

const char *__fastcall__ g_name(unsigned char seat)
{
    static char b[8];
    if (seat < 8 && g_names[seat][0] && g_names[seat][0] != '-')
        return g_names[seat];
    strcpy(b, "seat ");
    strcat(b, utoa10(seat));
    return b;
}

/* Part part_pg of chat line i into part; 2 if the line is in two parts. */
static unsigned char __fastcall__ chat_part(unsigned char i)
{
    const char *s = chat_l[i];
    unsigned char cut = 40;
    if (strlen(s) <= 40) {
        strcpy(part, s);
        return 1;
    }
    while (cut && s[cut] != ' ')
        --cut;
    if (!cut)
        cut = 40;
    if (part_pg) {
        strcpy(part, "  ");
        strncat(part, s + cut + (s[cut] == ' '), 38);
    } else {
        memcpy(part, s, cut);
        part[cut] = 0;
    }
    return 2;
}

static void chat_show(void)
{
    if (chat_on) {
        part_pg = chat_pg;
        chat_part(chat_tk);
        ui_msg(part);
    }
}

/* A message; with nothing to say, the line shows the last thing said. */
void g_message(const char *s)
{
    chat_run = 0;
    chat_on = !*s && chat_n;
    if (chat_on) {
        chat_tk = chat_n - 1;
        chat_pg = 0;
        chat_show();
    } else
        ui_msg(s);
}

/* The next part or line of new chat on the message line. */
static void chat_step(void)
{
    unsigned char two = strlen(chat_l[chat_tk]) > 40;
    if (!chat_pg && two)
        chat_pg = 1;
    else if (chat_tk + 1 < chat_n) {
        ++chat_tk;
        chat_pg = 0;
    } else if (two)
        chat_pg = 0;
    else
        chat_run = 0;
    chat_at = ticks;
    chat_show();
}

/* The table's line in LST: the names in its seats. Only that line is asked for (table=): a player's
   whole list can be longer than the reply buffer, and a new table's line comes last among their own.
   A parlor without table= answers ERR, and then the whole list is read. */
static void load_names(void)
{
    unsigned char i, s;
    char *p;
    q_set("LST table=");
    q_cat(utoa10(g_table));
    if (net_req() != R_OK) {
        q_set("LST");
        if (net_req() != R_OK)
            return;
    }
    for (i = 0; i < r_n; ++i) {
        if (strncmp(r_line[i], "TAB ", 4) != 0)
            continue;
        words(r_line[i]);
        if (wn < 7 || num(w[1]) != g_table)
            continue;
        strncpy(g_rules, w[3], 12);
        strncpy(gtype, w[2], 15);
        p = strchr(w[4], '/');
        g_seats = p ? num(p + 1) : 2;
        if (g_seats > 8)
            g_seats = 8;
        memset(g_names, 0, sizeof g_names);
        s = 0;
        p = w[6];
        while (*p && s < 8) {
            char *e = strchr(p, ',');
            if (e)
                *e = 0;
            strncpy(g_names[s], p, 12);
            if (strcmp(p, me) == 0)
                g_seat = s;
            ++s;
            if (!e)
                break;
            p = e + 1;
        }
        return;
    }
}

/* Fetch the view: 1 if it came. */
static unsigned char fetch(void)
{
    unsigned char i, n, asked_g = !view_g;
    char *l;
    q_set("VUE");
    q_num(g_table);
    q_add(view_g ? "T" : "GT");
    if (!G || !G->no_m)
        q_cat("M");
    if (net_req() != R_OK) {
        if (r_final)
            g_message(err_text(r_final));
        return 0;
    }
    m_n = 0;
    mused = 0;
    stale = 0;
    g_bot = 0;
    have_clk = 0;
    g_status[0] = 0;
    g_started = 0;
    if (G)
        G->begin();
    for (i = 0; i < r_n; ++i) {
        l = r_line[i];
        if (l[0] == 'S' && l[1] == ' ') {
            strncpy(g_status, l + 2, 40);
            g_status[40] = 0;
            continue;
        }
        if (l[0] == 'M' && l[1] == ' ') {
            n = strlen(l + 2);
            if (m_n < MAXM && mused + n + 1 <= sizeof mbuf) {
                m_tok[m_n++] = mbuf + mused;
                strcpy(mbuf + mused, l + 2);
                mused += n + 1;
            }
            continue;
        }
        words(l);
        if (!wn)
            continue;
        if (is(w[0], "V")) {
            if (wn > 2)
                v_seq = num(w[2]);
            if (wn > 1 && strcmp(w[1], gtype) != 0)
                strncpy(gtype, w[1], 15);
            continue;
        }
        if (is(w[0], "W") && wn > 1) {
            g_started = 1;
            g_turn = (w[1][0] == '-') ? -1 : (signed char)num(w[1]);
        } else if (is(w[0], "CLK") && wn > 2) {
            g_started = 1;
            have_clk = 1;
            clk_seat = num(w[1]);
            clk_secs = 0;
            for (l = w[2]; *l >= '0' && *l <= '9'; ++l)
                clk_secs = clk_secs * 10 + (*l - '0');
            clk_at = ticks;
        } else if (is(w[0], "BOT")) {
            for (n = 1; n < wn; ++n)
                g_bot |= 1 << num(w[n]);
        } else if (is(w[0], "T") && wn > 2) {
            n = num(w[1]);
            if (n < 8)
                g_score[n] = snum(w[2]);
        }
        if (G)
            G->line();
    }
    g_over = strncmp(g_status, "Game over", 9) == 0 || (g_started && g_turn < 0);
    if (strcmp(g_status, "Waiting for players") == 0)
        g_started = 0;
    g_myturn = g_seat >= 0 && g_turn == g_seat && !g_over && g_started;
    /* The geometry (G) comes once the game has begun: until then, ask for it again; when it comes,
       draw everything (a board drawn before it lacks what the geometry says). */
    if (asked_g && g_started) {
        view_g = 1;
        g_full = 1;
    }
    /* A seat filled since the names were read: read them again (the view has no names). */
    if (g_started)
        for (n = 0; n < g_seats; ++n)
            if (!g_names[n][0] || g_names[n][0] == '-') {
                load_names();
                g_full = 1;
                break;
            }
    return 1;
}

static void show_status(void)
{
    static char b[56];                  /* the status (40) and " (65535d left)" */
    unsigned long left;
    strcpy(b, g_status);
    if (have_clk && !g_over) {
        left = clk_secs;
        if (left > 0) {
            strcat(b, g_status[0] ? " (" : "(");
            if (left >= 86400) {
                strcat(b, utoa10((unsigned int)(left / 86400)));
                strcat(b, "d");
            } else if (left >= 3600) {
                strcat(b, utoa10((unsigned int)(left / 3600)));
                strcat(b, "h");
            } else {
                strcat(b, utoa10((unsigned int)(left / 60)));
                strcat(b, "m");
            }
            strcat(b, " left)");
        }
    }
    ui_status(b);
}

void g_title(void)
{
    static char b[42];
    unsigned char s;
    ui_ink(F_BLACK, F_WHITE);
    strcpy(b, G ? G->name : (const char *)gtype);
    strcat(b, " ");
    strcat(b, utoa10(g_table));
    strcat(b, ":");
    for (s = 0; s < g_seats && strlen(b) < 26; ++s) {   /* (b: 25, " v " and 12 at most) */
        strcat(b, s ? " v " : " ");
        strncat(b, g_name(s), 40 - strlen(b));     /* names of 12 at 3 or 4 seats would overrun b */
    }
    b[32] = 0;                          /* then the connected time (clk_show) */
    ui_textn(0, 0, b, 40);
    ui_ink(F_WHITE, F_BLACK);
}

static void frame_buttons(void)
{
    ui_button(0, BUTTON_Y, "Lobby", B_LOBBY);
    if (g_seat >= 0 && g_started && !g_over)
        ui_button(8, BUTTON_Y, "Resign", B_RESIGN);
    ui_button(17, BUTTON_Y, "Chat", B_CHAT);
    ui_button(24, BUTTON_Y, "Keys", B_HELP);
}

static void draw_all(void)
{
    unsigned char keep = t_cur < t_n ? t_id[t_cur] : 255;
    ui_cls();
    g_title();
    g_full = 1;
    if (G && g_started)
        G->draw();
    else if (!G) {
        ui_text(2, 60, "This game is not on " MACHINE_NAME " yet.");
        ui_text(2, 72, "Play it on the web or by telnet.");
    } else
        ui_text(2, 60, "Waiting for players...");
    g_full = 0;
    frame_buttons();
    show_status();
    chat_show();
    if (keep != 255 && ui_find(keep) != 255)
        ui_point(ui_find(keep));
    else
        ui_point(0);
}

void g_redraw(void)
{
    draw_all();
}

static void redraw(void)
{
    ui_hide();
    if (G && g_started)
        G->draw();
    show_status();
    ui_show();
}

/* Fetch and draw again. */
static void refresh(void)
{
    unsigned char was = g_started, wasover = g_over;
    if (!fetch())
        return;
    if (g_full || was != g_started || wasover != g_over)
        draw_all();
    else
        redraw();
}

static void poll_events(void);

static unsigned char send_move(const char *verb, const char *tokens)
{
    unsigned char k;
    q_set(verb);
    q_num(g_table);
    if (tokens)
        q_add(tokens);
    k = net_req();
    g_reply_more = g_reply_done = 0;
    if (k == R_OK) {
        unsigned char i, passed = 0;
        unsigned int seq;
        if (strcmp(r_final, "OK more") == 0)
            g_reply_more = 1;
        if (strcmp(r_final, "OK done") == 0)
            g_reply_done = 1;
        /* a roll with no legal step passes the turn at once: say so (but not after a pass of our own) */
        for (i = 0; i < r_n && !(tokens && is(tokens, "PASS")); ++i) {
            words(r_line[i]);
            if (wn > 4 && is(w[0], "EVT") && is(w[3], "PASS") && num(w[4]) == g_seat)
                passed = 1;
        }
        g_message("");
        /* the events since the view, ours and any that came after them (the bot answers at once and
           the view may show its turn already), then the view: the game draws from both */
        stale = 1;
        poll_events();
        if (stale)
            refresh();
        if (passed)
            g_message("No legal move, so the turn passed.");
        return 1;
    }
    if (net_unsure) {
        /* the reply came broken: the events and the view say whether the move took */
        stale = 1;
        poll_events();
        if (stale)
            refresh();
    } else if (r_final)
        g_message(err_text(r_final));
    return 0;
}

unsigned char __fastcall__ g_move(const char *tokens)
{
    return send_move("MOV", tokens);
}

unsigned char g_end(void)
{
    return send_move("END", 0);
}

unsigned char g_undo(void)
{
    return send_move("UND", 0);
}

unsigned char __fastcall__ g_chk(const char *tokens)
{
    q_set("CHK");
    q_num(g_table);
    q_add(tokens);
    g_chkinfo[0] = 0;
    if (net_req() == R_OK) {
        strncpy(g_chkinfo, r_final[2] ? r_final + 3 : "", 40);
        return 1;
    }
    if (r_final)
        strncpy(g_chkinfo, err_text(r_final), 40);
    g_chkinfo[40] = 0;
    return 0;
}

/* SIN: keep chat, and fetch the view again if anything else happened since it. What a player plays with
   (CLIENT seat text, right after their JOIN or when it changes) goes into the chat too: "ann joined with
   GameMaster 1.0.0 on a Macintosh Plus". */
static void poll_events(void)
{
    unsigned char i, j, n, s, said = 0, join = 0, jseat = 255;
    char *b;
    const char *who, *tail, *jname;
    q_set("SIN");
    q_num(g_table);
    q_num(ev_seq);
    if (net_req() != R_OK)
        return;
    for (i = 0; i < r_n; ++i) {
        words(r_line[i]);
        if (wn < 4 || !is(w[0], "EVT"))
            continue;
        s = num(w[4]);
        who = g_name(s);
        tail = ":";
        if (is(w[3], "JOIN")) {
            jseat = s;      /* its name, for the CLIENT that follows: the names are read after these */
            jname = w[5];
        }
        if (is(w[3], "CLIENT")) {
            if (s == (unsigned char)g_seat)
                continue;
            tail = " now plays";
            if (s == jseat) {
                who = jname;
                tail = " joined";
            }
        } else if (is(w[3], "CHT")) {
            if (w[4][0] == 0x2D)
                who = "watcher";
        } else {
            if (!g_over && num(w[2]) > v_seq) {
                stale = 1;
                if (G && G->event)
                    G->event();
                if (is(w[3], "JOIN") || is(w[3], "LEAVE") || is(w[3], "GONE"))
                    join = 1;
            }
            continue;
        }
        if (wn < 6)
            continue;
        if (chat_n == CHAT_N) {
            memmove(chat_l[0], chat_l[1], (CHAT_N - 1) * CHAT_W);
            --chat_n;
            if (chat_tk)
                --chat_tk;
        }
        b = chat_l[chat_n++];
        strcpy(b, who);
        strcat(b, tail);
        for (j = 5; j < wn; ++j) {
            n = strlen(b);
            if (n >= CHAT_W - 2)
                break;
            b[n] = ' ';
            b[++n] = 0;
            strncat(b, w[j], CHAT_W - 1 - n);
        }
        ++said;
    }
    if (r_final[2])
        ev_seq = num(r_final + 3);
    if (said) {
        if (!chat_run || !chat_on) {
            chat_tk = said < chat_n ? chat_n - said : 0;
            chat_pg = 0;
        }
        chat_on = chat_run = 1;
        chat_at = ticks;
        if (!in_view)
            chat_show();
    }
    if (join) {
        load_names();
        g_full = 1;
    }
    if (stale && !in_view)
        refresh();
}

static unsigned char ask_yn(const char *s)
{
    unsigned char k;
    g_message(s);
    do
        k = kbd_get();
    while (!k);
    g_message("");
    return (k | 0x20) == 'y';
}

/* OA-T or Chat: what has been said at the table, the newest at the bottom, and a line to say more. */
static void chat(void)
{
    static char b[57];
    const char *note = 0;
    unsigned char i, y;
    in_view = 1;
    for (;;) {
        ui_cls();
        g_title();
        y = MSG_Y - 16;
        if (!chat_n)
            ui_text(0, y, "Nothing has been said here yet.");
        for (i = chat_n; i && y >= GAME_TOP;) {
            part_pg = 1;
            if (chat_part(--i) == 2) {
                ui_text(0, y, part);
                y -= 8;
                if (y < GAME_TOP)
                    break;
                part_pg = 0;
                chat_part(i);
            }
            ui_text(0, y, part);
            y -= 8;
        }
        ui_text(0, MSG_Y, "Say:");
        ui_status(note ? note : (const char *)(K6 "Return: say it. " K3 "Esc: back to the game."));
        note = 0;
        b[0] = 0;
        if (!ui_input(5, MSG_Y, 35, b, 56, 0) || !b[0])
            break;
        q_set("CHT");
        q_num(g_table);
        q_add(b);
        if (net_req() == R_OK)
            poll_events();
        else if (r_final)
            note = err_text(r_final);
    }
    in_view = 0;
    chat_run = 0;
    chat_on = chat_n;
    chat_tk = chat_n - 1;
    chat_pg = 0;
    if (stale)
        fetch();
    draw_all();
}

/* Keys: the table's keys; pressed again, the game's own. */
static unsigned char help_page;

static void keys_help(void)
{
    if (help_page && G && G->help) {
        g_message(G->help);
        ui_status("");
    } else {
        g_message(K6 "Arrows," K6 "Return:play " OA "T chat " K3 "Esc:lobby");
#if !MACHINE_SERIAL
        ui_status(OA "R resign " OA "D display");
#else
        ui_status(OA "R resign " OA "D display " OA "C console");
#endif
    }
    help_page ^= 1;
}

static void set_display(void)
{
    mono ^= 1;
    q_set("PRF SET a2disp");
    q_add(mono ? "mono" : "colour");
    net_req();
    draw_all();
}

/* Play at a table. Returns 1 for the console, 0 for the lobby. */
unsigned char table_play(unsigned int id)
{
    unsigned char k;
    unsigned int last;
    g_table = id;
    g_seat = -1;
    g_seats = 2;
    g_turn = -1;
    g_over = 0;
    g_started = 0;
    view_g = 0;
    ui_retkey = 0;
    chat_n = chat_on = chat_run = in_view = 0;
    ev_seq = v_seq = 0;
    gtype[0] = 0;
    G = 0;
    memset(g_names, 0, sizeof g_names);
    memset(g_score, 0, sizeof g_score);
    load_names();
    gr_mode();
    ui_cls();
    {
        const struct gameinfo *gi = game_for(gtype);
        if (gi) {
            ui_status("Loading the game...");
            if (ovl_load(gi->ovl)) {
                G = GAME;
                G->begin();
            }
        }
    }
    ui_status("Fetching the table...");
    fetch();
    draw_all();
    /* what was said lately (the bot says hello as it sits down, before this) */
    ev_seq = v_seq > 16 ? v_seq - 16 : 0;
    poll_events();
    last = ticks;
    for (;;) {
        if (net_lost)
            return 0;
        k = ui_poll();
        if (t_cur != hover_at) {
            hover_at = t_cur;
            if (G && G->hover)
                G->hover(t_cur < t_n ? t_id[t_cur] : 255);
        }
        if (k && chat_run)
            g_message("");
        if (k == U_CLICK) {
            k = ui_clicked;
            if (k == B_LOBBY)
                return 0;
            if (k == B_RESIGN) {
                if (ask_yn("Resign this game? (" K3 "Y/N)")) {
                    send_move("RES", 0);
                    draw_all();
                }
            } else if (k == B_CHAT)
                chat();
            else if (k == B_HELP)
                keys_help();
            else if (G && k < 240)
                G->click(k);
            last = ticks;
            continue;
        }
        if (k) {
            if (k == K_ESC)
                return 0;
            if (k == (U_OA | 'C'))
                return 1;
            if (k == (U_OA | 'Q'))
                return 2;
            if (k == (U_OA | 'R')) {
                if (g_seat >= 0 && !g_over && ask_yn("Resign this game? (" K3 "Y/N)")) {
                    send_move("RES", 0);
                    draw_all();
                }
            } else if (k == (U_OA | 'T'))
                chat();
            else if (k == (U_OA | 'D'))
                set_display();
            else if (k == (U_OA | 'L'))
                refresh();
            else if (G && G->key)
                G->key(k);
            last = ticks;
            continue;
        }
        if (chat_run && ticks - chat_at > 180)
            chat_step();
        /* While waiting for others, ask what happened every 3 seconds; on our own move, every 15. After
           the game, for what is said. */
        if (ticks - last > (g_myturn ? 900 : 180)) {
            last = ticks;
            poll_events();
            clk_sync();
        }
    }
}
