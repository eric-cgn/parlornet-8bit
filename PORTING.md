# Porting ParlorNet to another 8-bit machine

ParlorNet's 8-bit client runs on two families of machines: the Apple II family (the IIe, and the IIgs, which runs the same code with its own start and network) and the Commodore 64. A Commodore 128 runs the C64 version, and boots it by itself from the disk. This page explains how the source is split between what every machine shares and what each machine provides, how to start a new machine, and what a port needs before it is merged. Ports are welcome, and the list at the end says which machines we would most like to see.

## The architecture

The program is a shared core and a machine layer. The core knows the parlor's protocol and the games' rules but nothing about any screen, keyboard or disk. The machine layer knows its hardware and nothing about the protocol.

```
            src/core                                  src/<machine>
  +-------------------------------+          +-------------------------------+
  | protocol (net.c)              |          | start-up and quitting         |
  | console, dialer, sign-in      |          | the line: serial, network     |
  |   (parlor.c)                  |  calls   | keyboard, joystick, mouse     |
  | main loop (main.c)            | -------> | text screen                   |
  | lobby (lobby.c)               |machine.h | graphics screen kit (ui_*)    |
  | table, chat, clock (table.c)  |          | overlays from the disk        |
  | the ten games' rules          | includes | settings file, title screen   |
  |   (games/NAME.c)  ------------+--------> | each game's board             |
  +-------------------------------+          |   (draw/NAME.c)               |
                                             +-------------------------------+
            src/common: code that more than one machine uses
```

Everything the core asks of a machine is declared in one header, `src/core/machine.h`, and the core includes nothing else of a machine's. The core has no tests for particular machines. Where machines differ in what they can do, the machine's `machdefs.h` says so with a capability, such as `MACHINE_SERIAL` (0 on the IIgs, which has no serial line and so no console or dialer).

A game is split the same way. `src/core/games/reversi.c` reads the parlor's view of the table into its own variables and turns the player's clicks into moves. At its end it includes `draw/reversi.c` from the machine's folder, which draws the board from those variables. The two are compiled as one file into the game's overlay.

## Choosing a toolchain

The Apple II and the C64 are built with cc65 2.19, from a Debian image pinned by digest (`Dockerfile`). cc65 also builds for the Atari 8-bit computers, the Commodore 128 and others, so a 6502 port starts with the compiler, the linker configuration and the overlay build the other machines already use.

For a Z80 machine (the ZX Spectrum, the MSX, the Amstrad CPC, CP/M) the likely route is z88dk, and for the 6809 (the Tandy Color Computer) the CMOC compiler. Neither has been tried, and these are the things to check first.

- **Integer sizes.** The core assumes 16-bit `int` and 8-bit `char`, as all three compilers have, and uses `unsigned char` wherever the sign matters. `char` is unsigned in cc65; check how a plain `char` behaves in the new compiler where the core compares characters.
- **Byte order.** The 6502 and the Z80 are little-endian and the 6809 is big-endian. The core keeps the time signed in as three bytes in `clk_t`, low byte first, and the 6502 machines' assembly reads it that way. A big-endian machine has to keep those bytes in the same order.
- **cc65's keywords and pragmas.** The core marks some functions `__fastcall__`, which another compiler can ignore if `machdefs.h` defines it as nothing. The core places data with `#pragma bss-name` and `#pragma rodata-name` (the table's buffers, each overlay's header), which another compiler does with its own section names, and builds with `-Cl` (local variables static), which only matters for speed.
- **The memory model and overlays.** The core and one overlay at a time must fit in the machine's memory, with about 32K for the resident program and about 10K for the largest overlay. cc65's linker writes each overlay as its own file to load at `OVL_ADDR`. z88dk and CMOC have their own ways of building code to load at a fixed address, and on a machine with banked memory (a Spectrum 128, an MSX2) the overlays could live in banks instead.
- **The 6502 assembly in `src/common/6502`.** The reply reader and the clock are assembly that both 6502 machines include. A machine on another processor writes them itself, and `src/template/template.c` has both in C to start from.

## The layout

- `src/core` holds the protocol, the dialer and sign-in, the lobby, the table, and the games' rules. `machine.h` is the machine interface, and `app.h`, `net.h`, `game.h`, `ovl.h` and `games.h` are the core's own headers, which the machine's setup and drawing also use.
- `src/common` holds what more than one machine uses: `ui_targets.c` (the screen kit's targets, the arrow keys' search for the nearest one, and waiting for a key, in plain C for any machine), `ui_hgr.c` (text, outlines and buttons for a machine whose drawing has the Apple's Hi-Res primitives, which the C64's does), and `6502/` (the reply reader and the clock, in assembly).
- `src/apple2` and `src/c64` are the machines. Each has `machdefs.h` (the constants the interface leaves to it) and its own private header (`apple2.h`, `c64.h`), which only its own files and its drawing include.
- `src/template` is a machine with every call stubbed. It compiles and links with the core and all ten games on every build, so it always matches the interface.
- `art/` holds the font, the title screen's letters and each machine's pictures as text, and `tools/` turns them into assembly and makes the disks.
- `test/` holds the unit tests and the emulator labs.

## The interface

`src/core/machine.h` is the reference: it declares each function and variable with what it does, when the core calls it, and what the Apple II and the C64 do. It has these groups.

- **The link.** `ser_get`, `ser_put`, `ser_drop`, `serial_open`, `line_desc`, the reply reader `ser_reply` with its `sr_` variables, and the network connection (`eth_on`, `cfg_conn`, `eth_connect`, `eth_close`). A network connection takes over the serial calls while it is up, so nothing above them changes.
- **Input.** `kbd_get` and `kbd_mods`, with the key codes in `machdefs.h`.
- **The text screen**, 40 by 24, for the console, the sign-in and the dialer: the `txt_` calls and variables.
- **Drawing.** The graphics screen kit, the `ui_` calls: text, fills, buttons, the status and message lines, the pointer, targets, input on the graphics screen, and line editing.
- **The games' drawing.** The contract for `draw/NAME.c` is in `machine.h`, and each game's rules file says in its first comment which functions its drawing defines and which target ids it uses. `src/template/draw/` has a stub for each.
- **Storage and settings.** There are no calls here, since the setup overlay reads and writes the settings file itself.
- **Overlays and memory.** `ovl_load`, `OVL_ADDR`, and the two flags `connected` and `signin_note`.
- **The progress bar.** There are no calls here either. A machine with a slow disk shows the load's progress itself, inside `ovl_load`.
- **The clock.** `ticks` (60 a second), `tick_poll`, and the time signed in, the `clk_` calls.
- **Start-up and shutdown.** `sys_init` and `sys_quit`.

The overlays the machine writes itself are the setup overlay (`setup.c`, the start screen and the settings), the title screen (`splash.c`) and the sign-in's network part (`signin_net.c`). Their headers are in `src/core/ovl.h`.

## Memory and overlays

Only the resident program and one overlay are in memory at a time. The overlays are the sign-in, each of the ten games, the start screen with the settings, and the title screen. The core calls `ovl_load` before it uses one, and the machine loads it to `OVL_ADDR`.

On the Apple IIe the program is at $4000-$BFFF and the overlays run in the language card's RAM at $D000, bank 2, with the command line and the dialogs in bank 1; the Double Hi-Res drawing runs in the auxiliary 64K. The self-booting disk has its own sector reader in the boot block, and under ProDOS the overlays come from PARLOR.DAT. The reply buffer and the serial ring are in low memory.

On the C64, BASIC and the KERNAL are switched out. The program is at $1400, the overlays at $9B00-$BFFF, the character set at $C000, the screens in the VIC's bank 3, and the bitmap under the KERNAL at $E000. The KERNAL is switched back in only to load from the disk. The overlays are packed files (`tools/c64_pack.py`), loaded at the top of their space and unpacked in place, and a bar on the bottom row fills as each loads. The start-up loads only what the title, the start screen and the settings need (PN.MAIN). The lobby, the tables and the protocol are linked after the program's variables and cut off into a file of their own (PN.PARLOR), which is loaded once, before the first sign-in. A machine with a slow drive can do the same with `PLATE` in `build.sh`. The README has both machines' full memory maps.

## Connecting

The dialer is in the core and behaves the same on every machine with a serial line. P on the start screen sends the init string (`ATZ` as the settings start, and nothing when it is empty) and waits for OK. It then dials `ATDT` and the number, or `ATDP` with the Pulse dial setting, and waits up to a minute for CONNECT. On CONNECT it sends the opening line, `PARLORNET/1`, at once, since the parlor closes a connection that has not sent it within 20 seconds. A number that starts with AT is sent as the whole dial command, whatever Pulse dial says. That rule is for modems that need a dial command of their own, such as a Zimodem's `ATD"parlornet.org:6501"`; the README documents it, and the program never mentions it on the screen. When the session ends the parlor hangs up, and a modem that has not noticed gets `+++` with a second of quiet on each side, then `ATH`.

The serial driver is where most of a port's trouble will be. These are the lessons from the two machines so far.

- **Receive by interrupt.** The reply reader runs in a tight loop, but the program also draws, loads from the disk and waits for keys, and bytes keep coming meanwhile. Every driver puts received bytes into a ring from an interrupt. The Super Serial Card's 6551 interrupts the Apple when its switch is on, and the program polls it in the reader's loop when it is off.
- **Don't trust the C64 KERNAL's RS-232.** It cannot keep up at 2400 baud in both directions while the program does anything else, so the C64 has its own driver. It receives with CIA 2's NMI, where the start bit's falling edge on FLAG starts timer B, which then samples each bit near its middle. It sends with CIA 1's timer A on the IRQ, each bit made ready before its time and sent 0.4 of a bit early, with two stop bits.
- **Interrupt registers lose events when read at the wrong moment.** A CIA's interrupt flag that sets in the same cycle the register is read can be lost. That is why sending is on the IRQ, which is taken on a level, and not on a second NMI, which is taken on an edge.
- **Start a byte atomically.** When the sender is idle, the first byte's start bit and the timer that times the bits after it are set up with interrupts held off. A raster interrupt between them would stretch the start bit, and the other end would read the bytes after it wrong.
- **Expect a lost byte now and then.** A reply that comes but has no final line has lost a byte on the way (noise, a modem's buffer, an interrupt held off too long), while a line that has gone says NO CARRIER or nothing at all. The core's `net_req` waits for the line to be quiet and sends a request that changes nothing (SIN, LST, VUE, TIM and the like) again, up to twice, with nothing on the screen. A request that changes something (MOV, CHT, NEW and the like) is not sent again, since it may have taken; the table or the lobby reads the view or the list again instead. Once part of a reply has come, four seconds without a byte end it, so this takes seconds and not the 30 that the parlor is given to answer.
- **The disk stops the line.** The KERNAL's disk routines hold interrupts off for long stretches, and they read CIA 1's interrupt flags. The C64 drops RTS around every disk call so the modem holds its data, and sets its interrupts up again afterwards.
- **A 6551 on the expansion port is the easy case.** The C64's SwiftLink-compatible modem, a cartridge's or the one built into a Commodore 64 Ultimate, interrupts by NMI for each byte and runs at 19200 without trouble. Most new machines and modem cartridges speak the 6551 or a UART like it, and a port should start there.

## Drawing

- **Square-looking boards.** Make squares and discs look square and round on the machine's own monitor. An Apple II Hi-Res pixel is about 7/6 as tall as it is wide, so a Reversi square there is 21 pixels by 18 lines. A C64 pixel is nearly square on PAL and narrower on NTSC, where the squares are square in pixels.
- **Work with the color limits.** The C64 has two colors in each 8 by 8 cell, so its boards are laid out on whole cells wherever two colors meet. The Apple IIe's Double Hi-Res has 16 colors at 140 pixels a line, each pixel four dots, and seven pixels are four whole bytes of the screen, so its boards are laid out in groups of 7 pixels, drawn a byte at a time. (The IIgs program's Hi-Res has its own color rules within each 7-pixel byte.) Plan a board around these limits first.
- **Keep pieces round and checkers readable.** Backgammon shows four checkers on a tall point and +n for the rest (two and +n on the bar).
- **Keep the frame's lines visible.** The message line shows the bot's greeting when a table opens, what the players say, and who joined with which program ("bob joined with GameMaster 1.0.0 + ParlorNet 0.5 on a Macintosh Plus"). A board must not draw over it.
- **Use the same target ids.** The lab's plans and the orchestrator click by id, so they work on every machine.
- **Offer a monochrome look.** D on the start screen and the lobby's Display button switch to it, for monochrome monitors and black-and-white TVs.

## Text on the screen

What the program says on the screen stays in the voice of its period. It talks about modems, lines, disks and parlors as a program of its time would, and it says nothing about modern setups, emulators, WiFi bridges or the internet. Things a modern setup needs (a Zimodem's quoted number, `ATS62=0`, netcat through a Unix shell) go in the README and the player's guide, not on the screen. Messages are plain sentences, and key legends follow the machine's own names for its keys. Wherever a screen names the command key, in a legend or in a sentence, it shows the key's symbol (`OA` in `machdefs.h`, a character of the machine's font) and the letter after it, never the key's name spelled out.

## Settings files

The setup overlay keeps the settings in a file on the program's disk (PARLOR.CFG). The file starts with a version byte. A file of another version or size is left alone, and the settings start as they always do until the player saves them again. There is no conversion of old files. The settings the core shares are in `app.h`, and the machine adds its own line settings.

## The build

`tools/build.sh` builds the toolchain image and runs `build.sh` in it, which builds every machine into `build/`. To add a machine:

- add its section to `build.sh`, with the `program` function for the resident part and the overlays and a linker configuration of its own (`src/c64/parlor.cfg` is the model);
- add its disk tool in `tools/`, written so that the same input always gives the same image (no timestamps, a fixed order of files, no random IDs);
- add its disk to `FILES` in `tools/release.py`, so that the dev pre-release and the releases carry it, and to the README's lists;
- add another compiler to the `Dockerfile`, pinned by version and the image by digest.

The output must be deterministic, so that two clean builds give byte-identical disks. Check that by building twice from clean checkouts and comparing the files.

## Testing

Each machine has an emulator lab that plays the program against a lab parlor in Docker, with no network outside the lab: `test/lab` for the Apple II in MAME, and `test/vice` for the C64 in VICE (`test/README.md`). A plan is a list of steps (wait, type, click a target by id, take a screenshot, ask the orchestrator for the next move), and every plan starts with the `title` step, which waits for the title screen and presses a key. The orchestrator (`test/lab/orch.py`) signs in as the player on its own connection, reads the table, and writes the clicks for a legal move. `GAME+` plays on until the game is over, taking a screenshot in the middle (`mid.png`) and showing the screen (`stuck.png`) if the same move is offered again and again.

A new machine needs a lab of its own on the same pattern: its emulator in a container, a step runner that reads the program's variables by the linker's labels and puts keys in its queue, and the plans. ROMs are never part of the repository. The person running the lab supplies them, or the emulator's source includes them, as VICE's does.

## Before a port is merged

- All ten games played to the end against the bot in the emulator lab, with screenshots of the start, the middle and the end of each.
- The dialer working as described above: the init string, `ATDT` and `ATDP`, CONNECT and the opening, hanging up, and a number that starts with AT.
- A deterministic build in the Docker toolchain, and `python3 -m unittest discover test` passing.
- The Apple II and C64 programs still building and playing; a change to the core is checked on both.
- No ROMs and no third-party code in the repository. cc65's runtime library is linked as its license allows, and the README carries its notice.
- On-screen text in the period's voice, as above.
- The README updated with the machine's section, memory map and disk.

## Ports we would welcome

- **The Atari 8-bit computers** (400, 800, XL and XE). They have the same 6502, and cc65 builds for them. FujiNet's network device would give a direct TCP connection to the parlor, and the 850 interface or an SIO modem would give the dialer a line.
- **The Commodore 128 in its own 128 mode.** It could use the 80-column VDC screen, 2 MHz and its own keyboard, with a boot sector that goes straight into the program. Today a C128 runs the C64 version in 64 mode.
- **The ZX Spectrum**, including the Spectrum Next with its WiFi. It would be a Z80 build with z88dk, with boards designed around the Spectrum's 8 by 8 color cells as the C64's are.
- **The MSX and the Amstrad CPC**, both Z80 machines that z88dk builds for.
- **The Tandy Color Computer.** It would be a 6809 build with CMOC, and FujiNet supports the CoCo too.
- **CP/M.** A text-mode build with z88dk would reach many machines at once, with the boards drawn in characters on an 80 by 24 terminal.

## Pull requests

Pull requests go to the `dev` branch, where they are squashed. `main` takes only merges from `dev`, which is how a release is made. A port can come in steps, such as the machine layer and one game first, as long as each step builds and the Apple II and C64 programs still pass.
