/* ParlorNet's template machine: every function and variable of the machine interface (core/machine.h) that
   src/common does not give, stubbed, so that the core and the ten games compile and link against it
   (tools/build.sh builds it as build/template, which is not a program anyone runs). To start a port, copy
   src/template to src/<your machine>, then fill these in, one group at a time, using src/c64 as the model.
   The comments say what each must do; machine.h says more. */
#include <string.h>
#include "machine.h"
#include "app.h"
#include "net.h"
#include "ovl.h"

/* ---------------------------------------------------------------------------------------------------
   The link */

/* The line's receive ring. A real machine fills it from an interrupt (the C64's NMI, the Apple's 6551 IRQ),
   so that no byte is lost while the program draws; ser_get only takes from it. */
static unsigned char ring[256];
static unsigned char rhead, rtail;

int ser_get(void)
{
    if (rhead == rtail)
        return -1;
    return ring[rtail++];
}

void __fastcall__ ser_put(unsigned char c)
{
    (void)c;                            /* to the line: the UART, or a ring sent by interrupt */
}

void ser_drop(void)
{
    rtail = rhead;
}

void serial_open(void)
{
    /* open the line at the settings' speed and format (cfg_* of the machine's own) */
}

const char *line_desc(void)
{
    return "2400 8N1";
}

/* The reply reader, in C: what src/common/6502/reply.inc does in assembly on the 6502 machines. A machine
   whose ser_get keeps up (an interrupt-driven ring, a fast processor) can use it as it is. */
char *sr_buf;
unsigned int sr_max, sr_ticks, sr_gap;
unsigned char sr_any, sr_final;

static unsigned char is_final(const char *kw)
{
    if (kw[0] == 'O')
        return kw[1] == 'K' && (kw[2] == 0 || kw[2] == ' ');
    if (kw[0] == 'E')
        return kw[1] == 'R' && kw[2] == 'R';
    if (kw[0] == 'A')
        return kw[1] == 'S' && kw[2] == 'K';
    if (kw[0] == 'N')
        return kw[1] == 'O' && kw[3] == 'C';
    return 0;
}

unsigned int ser_reply(void)
{
    static char kw[4];
    unsigned int cnt = 0, t0;
    unsigned char len = 0;
    int c;
    sr_final = 0;
    t0 = ticks;
    for (;;) {
        c = ser_get();
        if (c < 0) {
            tick_poll();
            if ((unsigned int)(ticks - t0) > (cnt ? sr_gap : sr_ticks))
                return cnt;
            continue;
        }
        t0 = ticks;
        if (c == 13 || c == 10) {
            if (!len)
                continue;               /* empty lines are skipped */
            if (cnt + 1 < sr_max)
                sr_buf[cnt++] = 0;
            if (len < 4)
                memset(kw + len, 0, 4 - len);
            len = 0;
            if (is_final(kw)) {
                sr_final = kw[0];
                return cnt;
            }
            if (sr_any)
                return cnt;
            continue;
        }
        c &= 0x7F;
        if (c < 32 || len >= 80)
            continue;
        if (len < 4)
            kw[len] = c;
        ++len;
        if (cnt + 1 < sr_max)
            sr_buf[cnt++] = c;
    }
}

/* A network connection: none here (eth_connect, in signin_net.c, says so). */
unsigned char eth_on;
unsigned char cfg_conn;

void eth_close(void)
{
    eth_on = 0;
}

/* ---------------------------------------------------------------------------------------------------
   Input */

unsigned char kbd_get(void)
{
    return 0;                           /* a key from the keyboard's queue or latch, or 0 */
}

unsigned char kbd_mods(void)
{
    return 0;                           /* bit 0: the command key was held with the last key */
}

/* ---------------------------------------------------------------------------------------------------
   The text screen (40 by 24) */

unsigned char txt_x, txt_y, txt_inv, txt_top, txt_bot = 24;
unsigned char txt_mt = 1;

void txt_mode(void)
{
}

void gr_mode(void)
{
}

void txt_clear(void)
{
    txt_x = 0;
    txt_y = txt_top;
}

void __fastcall__ txt_goto(unsigned char x, unsigned char y)
{
    txt_x = x;
    txt_y = y;
}

void __fastcall__ txt_putc(char c)
{
    if (c == 13 || ++txt_x >= 40) {     /* (a real one scrolls the window at its foot) */
        txt_x = 0;
        if (++txt_y >= txt_bot)
            txt_y = txt_bot - 1;
    }
}

void __fastcall__ txt_puts(const char *s)
{
    while (*s)
        txt_putc(*s++);
}

void txt_clreol(void)
{
}

void __fastcall__ txt_clrrow(unsigned char row)
{
    (void)row;
}

void __fastcall__ txt_cursor(unsigned char code)
{
    (void)code;
}

unsigned char txt_peek(void)
{
    return ' ';
}

/* ---------------------------------------------------------------------------------------------------
   Drawing. src/common/ui_targets.c gives the variables, the targets and ui_wait_key. A machine with the
   Apple's Hi-Res primitives (hgr.h) can take ui_text, ui_textn, ui_box and ui_button_draw from
   src/common/ui_hgr.c; this one writes them. */

void __fastcall__ ui_fill(unsigned char f)
{
    (void)f;
}

void __fastcall__ ui_ink(unsigned char fg, unsigned char bg)
{
    (void)fg;
    (void)bg;
}

void ui_rect(unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char f)
{
    (void)x; (void)y; (void)w; (void)h; (void)f;
}

void ui_text(unsigned char x, unsigned char y, const char *s)
{
    (void)x; (void)y; (void)s;
}

void ui_textn(unsigned char x, unsigned char y, const char *s, unsigned char w)
{
    (void)x; (void)y; (void)s; (void)w;
}

void ui_box(unsigned int px, unsigned char y, unsigned int w, unsigned char h)
{
    (void)px; (void)y; (void)w; (void)h;
}

void ui_button_draw(unsigned char x, unsigned char y, const char *s)
{
    (void)x; (void)y; (void)s;
}

/* A button and its target: the target's center is half the pixel x, as every target's is. */
void ui_button(unsigned char x, unsigned char y, const char *s, unsigned char id)
{
    unsigned char n = strlen(s) + 2;
    ui_button_draw(x, y, s);
    ui_target(x * 4 + n * 2, y + 4, n * 2, 4, id);
}

void ui_cls(void)
{
    clk_shown = 255;                    /* (the time signed in drawn again: clk_show) */
    ui_targets_clear();
}

void ui_status(const char *s)
{
    ui_textn(0, STATUS_Y, s, 40);
}

void ui_msg(const char *s)
{
    ui_textn(0, MSG_Y, s, 40);
}

void ui_targets_clear(void)
{
    t_n = 0;
    t_cur = 255;
}

void __fastcall__ ui_point(unsigned char i)
{
    if (i < t_n)
        t_cur = i;                      /* and the pointer drawn there */
}

void ui_hide(void)
{
}

void ui_show(void)
{
}

/* The screen's input: the arrows move the pointer (ui_nearest), Return clicks the target under it. */
unsigned char ui_poll(void)
{
    unsigned char k = kbd_get(), n;
    clk_show();
    if (!k)
        return 0;
    if (k == K_LEFT || k == K_RIGHT || k == K_UP || k == K_DOWN) {
        n = ui_nearest(k);
        if (n != 255)
            ui_point(n);
        return 0;
    }
    if (k == K_RET && !ui_retkey && t_cur < t_n) {
        ui_clicked = t_id[t_cur];
        return U_CLICK;
    }
    return (kbd_mods() & 1) ? U_OA | k : k;
}

void ui_mouse_init(void)
{
    ui_hasmouse = 0;
}

unsigned char ui_input(unsigned char x, unsigned char y, unsigned char w, char *buf, unsigned char max,
                       unsigned char secret)
{
    unsigned char k, n = strlen(buf);
    (void)x; (void)y; (void)w; (void)secret;
    for (;;) {
        k = kbd_get();
        if (k == K_RET)
            return 1;
        if (k == K_ESC)
            return 0;
        if (k == K_DEL && n)
            buf[--n] = 0;
        else if (k >= 32 && k < 127 && n < max) {
            buf[n++] = k;
            buf[n] = 0;
        }
    }
}

/* ---------------------------------------------------------------------------------------------------
   Overlays: load overlay k (its file, or its place on the disk) to OVL_ADDR. Here none is ever loaded. */

unsigned char __fastcall__ ovl_load(unsigned char k)
{
    (void)k;
    return 0;
}

/* ---------------------------------------------------------------------------------------------------
   The clock: ticks from the machine's 60 Hz interrupt, and the time signed in. src/common/6502/clock.inc
   does the time signed in in assembly on the 6502 machines; this is the same in C, with TIM left out. */

unsigned int ticks;
unsigned char clk_t[3];
unsigned char clk_sub = 60;
unsigned char clk_shown = 255;
unsigned char clk_tim_off, clk_tim_at;
char clk_buf[9];

void tick_poll(void)
{
}

void __fastcall__ clk_set(const char *secs)
{
    unsigned long n = 0;
    while (*secs >= '0' && *secs <= '9')
        n = n * 10 + (*secs++ - '0');
    clk_t[0] = n;
    clk_t[1] = n >> 8;
    clk_t[2] = n >> 16;
}

static unsigned long clk_secs(void)
{
    return clk_t[0] | ((unsigned int)clk_t[1] << 8) | ((unsigned long)clk_t[2] << 16);
}

unsigned char clk_min(void)
{
    return clk_secs() / 60 % 60;
}

/* The units of n (under 100) before *x, and its tens when it has them. */
static void clk_num(unsigned char n, unsigned char *x)
{
    clk_buf[--*x] = '0' + n % 10;
    if (n >= 10)
        clk_buf[--*x] = '0' + n / 10;
}

const char *clk_text(void)
{
    unsigned long m = clk_secs() / 60;
    unsigned char x = 8;
    memset(clk_buf, ' ', 8);
    clk_shown = m % 60;
    clk_buf[--x] = 'm';
    clk_num(m % 60, &x);
    if (m >= 60) {
        x -= 2;
        clk_buf[x + 1] = ' ';
        clk_buf[x] = 'h';
        clk_num(m / 60, &x);
    }
    return clk_buf;
}

void clk_sync(void)
{
    clk_tim_off = 1;                    /* (TIM: q_set("TIM"), net_req, clk_set from its reply) */
}

void clk_show(void)
{
    if (!signed_in || clk_min() == clk_shown)
        return;
    ui_textn(32, 0, clk_text(), 8);
}

/* ---------------------------------------------------------------------------------------------------
   Start-up and quitting */

void sys_init(void)
{
    /* the machine's memory and interrupts, the screen, the settings (SETUP->load after ovl_load(OVL_SETUP)),
       the mouse (ui_mouse_init) and the line (serial_open) */
    serial_open();
}

void sys_quit(void)
{
    for (;;)
        ;                               /* back to the machine's own system, or a reset */
}
