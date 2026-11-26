/* ParlorNet's template machine: the setup overlay (ovl.h's struct setup): the start screen, the settings
   screen and reading the settings at start-up. src/c64/setup.c is the model: its start screen offers P, C, S
   and D, its settings show the line and the dialer's init string, number and Pulse dial (setting_line, app.h),
   and its settings file starts with a version byte; a file of another version or size is left alone. */
#include "machine.h"
#include "app.h"
#include "ovl.h"

/* The settings screen; the core opens the line again after it. */
static void settings_screen(void)
{
    txt_mode();
    txt_clear();
    title_bar("ParlorNet settings", "");
    setting_line(2, "6", "Init string: ", cfg_init[0] ? cfg_init : "none");
    setting_line(3, "7", "Number: ", cfg_number);
    setting_line(4, "8", "Pulse dial: ", cfg_pulse ? "on" : "off");
    ui_wait_key();
}

/* The start screen, which the core shows and then reads a key for (P, C, S, D, OA-Q). */
static void start_screen(void)
{
    txt_mode();
    txt_clear();
    title_bar("ParlorNet", "");
    txt_goto(0, 4);
    sayln("P  ParlorNet: connect and sign in");
    sayln("C  Console: set up the line by hand");
    sayln("S  Settings");
}

/* The settings kept on the disk, read at start-up (sys_init). */
static void load_settings(void)
{
}

#pragma rodata-name (push, "OVHDR")
const struct setup setup_header = {settings_screen, start_screen, load_settings};
#pragma rodata-name (pop)
