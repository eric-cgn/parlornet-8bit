# ParlorNet for 8-bit computers

parlornet-8bit builds ParlorNet's own programs for 8-bit computers: today the Apple IIe and IIgs, with the Commodore 64 being added. Each machine has its own screen, input, storage and connection code, around a shared core: the protocol, the sign-in, the lobby, the table, the chat, the dialer and the ten games.

## The Apple IIe and IIgs

A program for the Apple IIe that plays the games of a ParlorNet parlor: Backgammon, Cannon Fire, Checkers, Dots and Boxes, Five in a Row, Grid Words, Mancala, Naval Combat, Reversi and Rummy Tile Classic, in Hi-Res color or monochrome. It works with any ParlorNet parlor, since it speaks the parlor's native protocol, PARLORNET/1, on the parlor's native port (6501); the protocol is described in [docs/protocol.md](https://github.com/eric-cgn/parlornet/blob/main/docs/protocol.md) in the ParlorNet repository (https://github.com/eric-cgn/parlornet). The parlor deals, judges every move and runs the bot, so the program only shows what the parlor sends and sends what the player chooses.

It reaches the parlor over a Super Serial Card (through a bridge, a WiFi modem, or a Unix shell with netcat) or an Uthernet II card, with our own driver for its W5100. It boots from its own disk, with no operating system, or runs from the player's own ProDOS. The player's guide is docs/apple2.md in the ParlorNet repository.

The same program also comes as a GS/OS application for the Apple IIgs, which connects through Marinetti, the IIgs TCP/IP stack, over whatever network Marinetti is set up for: Ethernet, a SLIP or PPP line on a serial port, or MacIP over AppleTalk. See The Apple IIgs, below. On a IIgs the IIe disk and PARLOR.SYSTEM also work as they are, with a Super Serial Card or an Uthernet II in a slot.

## Building

You need Docker and Python 3. Run:

```
tools/build_apple2.sh
```

It builds the toolchain image (cc65 2.19 on Debian, pinned by digest, from `Dockerfile`), compiles and links the program in it (`build.sh`), and makes the disks in `build/`: `parlornet.dsk`, a 140K disk in DOS 3.3 sector order that boots by itself, `prodos/` with `PARLOR.SYSTEM` and `PARLOR.DAT` for ProDOS, and `gs/` with the IIgs application, `PARLORNET`, its `PARLOR.DAT`, and `parlornet-gs.po`, an 800K disk holding the two. The same source always gives the same disks. `APPLE2_IMAGE` names another image with cc65 2.19 and Python 3 to use instead.

## Versions and releases

The version is in `VERSION`, one for the whole repository. Changes go into `dev` by pull request, squashed; `main` takes only merges from `dev`, which is how a release is made. Every pull request and every push to `main` builds and runs `python3 -m unittest discover test` (`.github/workflows/ci.yml`); the MAME lab needs ROMs and runs only by hand (test/README.md). A pull request into `main` fails unless it comes from `dev`. Every push to `dev` publishes a `dev` pre-release that replaces the last one, with files named for the version, `-dev-` and the commit, such as `parlornet-1.0-dev-46a0304.dsk` (`.github/workflows/dev.yml`).

A push to `main` whose version has no release yet runs the release (`.github/workflows/release.yml`): it builds, runs the tests, tags the commit `v<version>` and publishes a GitHub Release with `parlornet.dsk` (the Apple IIe's self-booting disk), `parlor-files.po` (the ProDOS files, on a ProDOS volume) and `parlornet-gs.po` (the Apple IIgs disk), with their SHA-256 and MD5 sums. `tools/release.py` gathers those files from `build/` into `build/release/`.

## Connecting

The program starts with its title screen, PARLOR over NET in outlined letters, green on a color monitor and white in monochrome. It says "Press a key" and waits for one, then goes on to the start screen. Under ProDOS and on the IIgs the settings can turn the title screen off (T), and the program remembers that in PARLOR.CFG.

The key legends show the Open-Apple key as its apple. On the Hi-Res screens it is a glyph of the program's own font, so every machine shows it; on the text screens it is MouseText's apple on an enhanced IIe, a IIc or a IIgs, and "OA-" on an older IIe, which has no MouseText (the program tells them apart by the processor: the enhanced machines have a 65C02 or a 65816). In Grid Words, Open-Apple-V shows the points of every letter and the blank, as the table's rules give them, until the next key. In monochrome, where the text is thin, the keys in the legends are in bold: the apple and its letter, Return, Esc and a game's own letters. In color all the text is bold already.

In Backgammon each step glides its checker from where it was to where it goes, yours as you play them and the other player's one after another when their turn comes in, a hit checker to the bar first. A point with more than five checkers shows four and +n for the rest, and the bar more than three as two and +n. With checkers of yours on the bar, the pointer for the bar sits on them, below the cube.

At a table, what the players say shows on the line above the status line as it comes. A line too long for the screen shows in two parts, three seconds apart, until you press a key, and lines that come together show one after another. The last line said stays there whenever the game has nothing else to say. Open-Apple-T or the Chat button opens the table's chat, with the last twelve lines said there, your own among them, and a line to type in. Return says what you typed and leaves the chat open, and Esc (or Return on an empty line) goes back to the board. When another player sits down, the same line says what they play with, as the parlor tells it, such as "bob joined with GameMaster 1.0.0 + ParlorNet 0.5 on a Macintosh Plus", and it stays in the chat with what was said. If they come back to the game from another program, the line says so: "bob now plays with RSCARDS 5.02 on an IBM PC". Where the player's program named nothing, or the table is private and you are only watching, nothing is shown.

The start screen offers P (connect and sign in), C (a plain line console, for setting up the line by hand) and S (the settings: the serial card's slot, speed and format and the modem's dialing, or the Uthernet II's slot, addresses and the parlor's name and port).

On the Super Serial Card the program takes the line to be a modem, and P dials it. It resets the modem with `ATZ` and waits for OK, sends the init string if there is one and waits for OK again, then dials `ATDT` and the number and waits up to a minute for CONNECT. On CONNECT it sends its first line at once, so the parlor's 20 seconds for it are never a race, and signs in. The screen shows each command and what the modem answers. If the modem says NO CARRIER, BUSY, NO DIALTONE, NO ANSWER or ERROR, or nothing comes within the minute, the program says so and offers to dial again, the console (Open-Apple-C) or the start screen (Esc). Esc while it dials sends a Return, which stops a modem's dial, and goes back to the start screen.

The settings for the modem are three lines under the serial card's:

- **Reset first (ATZ)**, on at first. Key 6 turns it on and off.
- **Init string**, empty at first: a command sent after the reset and before the dial, such as `AT&F` or `ATE0`. Key 7 changes it.
- **Number**, `parlornet.org:6501` at first, which the program dials as `ATDTparlornet.org:6501`. Key 8 changes it. A number that starts with AT is sent as the whole dial command, for a modem whose ATDT means a telnet connection: a Zimodem dials the native port plainly with `ATD"parlornet.org:6501"`. An empty number means a line with no modem, and P then sends the first line straight away.

Under ProDOS the program keeps these in PARLOR.CFG with the other settings. The self-booting disk keeps its settings only until the Apple is switched off, as it does the others.

When you close the connection (Open-Apple-Q), the program says goodbye to the parlor, which hangs up, and the modem says NO CARRIER. A modem that does not notice gets the Hayes escape, `+++` with a second of quiet before and after it, then `ATH`. That escape is sent only then, never during a session. Then the start screen comes back. If the connection is lost while you play, the program says so and Return dials again.

- **A WiFi modem or a modem emulator** (tcpser, or a computer's modem emulation): press P.
- **A Unix shell:** see Through a Unix shell, below.
- **A bridge** (ser2net, socat, or a cable to a machine that relays to the parlor's port 6501): empty the number in the settings, then press P.
- **An Uthernet II:** set the connection to Uthernet II in the settings, with this Apple's address, netmask, gateway, DNS server and the parlor's name and port, then press P.

The console stays a plain terminal: what you type goes to the line and what comes back is shown. Open-Apple-P there goes into ParlorNet mode on the line as it is, without dialing, for a connection you have made by hand.

Once connected, the program shows how long the connection has been up, as terminal programs show the time online: "0:12", then "1:05:33" after an hour. It is on the sign-in screen's title row after "ParlorNet", and at the right end of the title row in the lobby and at a table, and it starts from 0:00 with each new connection. Most Apple IIs have no clock, so the parlor gives the time (its TIM request, asked about every 30 seconds along with the program's other requests), and the program counts the seconds in between. With a parlor that does not know TIM yet, the program counts on its own, from the screen's 60 frames a second; that count falls behind by a second or two each time a game is loaded from the disk, and by about five seconds an hour besides.

### Through a Unix shell

When the serial line leads to a Unix shell (a login on a Linux machine or a Raspberry Pi, say), netcat can open the connection. Press C for the console, log in, and type:

```
stty raw -echo; nc parlornet.org 6501; stty sane
```

Then press Open-Apple-P instead of Return. Open-Apple-P sends the Return for the command and goes straight into ParlorNet mode, whose first line follows it at once; the parlor closes a connection that has not sent its first line within 20 seconds, so pressing Return first and Open-Apple-P later can be too slow. `stty raw -echo` passes every byte through and stops the shell echoing what the Apple sends, nc connects to the parlor's native port, and `stty sane` puts the terminal back when the connection ends. Closing the connection from the Apple (Open-Apple-Q in the lobby) makes the parlor hang up, nc ends, and the console is back at the shell prompt. OpenBSD's nc (the usual one on Debian and Ubuntu) and Nmap's ncat notice the hang-up only at the next key you press, which is lost; the traditional nc and `busybox nc` end at once. OpenBSD nc's `-N` and `-q` options do not change this.

The shell can also play the modem with pndial, a short script in the ParlorNet repository (`tools/pndial`), and then P dials through it with no console step and no Open-Apple-P race. Install pndial on the Unix machine so that the command `ATZ` runs it, either with a link beside it, such as `ln -s pndial ~/.local/bin/ATZ` with pndial in `~/.local/bin` on your PATH, or with `alias ATZ='pndial -z'` in the shell's startup file. Then, with the line logged in at a shell prompt, press P: the program's `ATZ` reaches the shell, which echoes it and runs pndial, and pndial answers OK as a modem would. The program dials the number in the settings, pndial says CONNECT and joins the line to the parlor with nc, and the program signs in. pndial dials whatever host and port the number names, so the number `parlornet.org:6501` reaches parlornet.org. After Open-Apple-Q pndial waits for the next dial, and P dials again; with OpenBSD's nc it notices the parlor's hang-up when the program sends `+++`. To get the shell prompt back, press C for the console and Control-C. ParlorNet's docs page on serial connections (http://docs.parlornet.org/docs/serial.html) covers pndial, the other ways over a serial line and the cable.

If the parlor goes away while you play (it restarts, say), nc ends too, and the program says "The connection to the parlor was lost." on the program's next request. When P dialed the line, through pndial or a modem, Return dials again. On a line connected by hand, trying again then reaches only the shell, and the program says there is no answer from the parlor. Press Open-Apple-C for the console, type the nc command again and press Open-Apple-P. While the program connects and on those questions, Open-Apple-C (or C alone) goes to the console; at the name prompt it is Open-Apple-C.

## The Apple IIgs

On a IIgs, ParlorNet can use the machine's own networking through Marinetti, the IIgs TCP/IP stack. This version is a GS/OS application, PARLORNET, with the same games, screens and keys as the IIe program.

### What you need

- An Apple IIgs (ROM 01 or ROM 03) with System 6.0.1.
- Marinetti 3.0 (https://www.apple2.org/marinetti/), installed and set up in its TCP/IP control panel for your network: an Ethernet card such as the Uthernet, a SLIP or PPP line on a serial port, or MacIP over AppleTalk. Connecting from the control panel once is a good test. ParlorNet doesn't include Marinetti; you install it yourself.
- Enough memory for System 6.0.1 and Marinetti, plus 64K free in one bank for ParlorNet.

So far it has been played in MAME's IIgs (ROM 03) with System 6.0.1 and Marinetti 3.0b11 over SLIP (test/README.md), not yet on a real IIgs or a ROM 01 machine.

### Installing

`parlornet-gs.po` is an 800K ProDOS disk with two files, `PARLORNET` (the application) and `PARLOR.DAT` (the games). Run PARLORNET from that disk, or copy both files into one folder on your hard disk and run it from there. The settings are kept in `PARLOR.CFG` in the same folder, so the disk must not be write-protected if you want them kept.

### Connecting

Open PARLORNET from the Finder. After the title screen, the start screen shows the parlor it will connect to; press S to change the parlor's name or port (the native port, 6501, unless the parlor's owner says otherwise), the display, or whether the title screen is shown. Press P to connect and sign in. ParlorNet starts Marinetti, connects to the network if Marinetti isn't connected yet (as the control panel's Connect button would), looks up the parlor's name and opens the connection. From there it plays exactly as on the IIe, with the mouse or the keys. Open-Apple-Q in the lobby closes the connection to the parlor, and Open-Apple-Q on the start screen quits to the Finder. ParlorNet leaves Marinetti's network connection as it is when it quits; the TCP/IP control panel disconnects it.

If something goes wrong, ParlorNet says what Marinetti answered. "Marinetti did not load" means the TCPIP init isn't installed (or the IIgs was started without it). "Marinetti could not connect" comes with Marinetti's error number, and the TCP/IP control panel is the place to look. A name that can't be looked up usually means Marinetti has no DNS server set.

### How it works

The IIgs program is the IIe program's own 6502 code, built from the same source with `IIGS` defined. PARLORNET (`src/gsshell.s`) starts the Tool Locator, the Memory Manager and the Miscellaneous Tool Set, asks the Memory Manager for a whole 64K bank, copies the program into it at the addresses it uses on the IIe, reserves the Hi-Res page in bank $E0, and jumps to the program in the 65816's native mode with 8-bit registers and the data bank set to the new bank. Absolute addresses then land in that bank, while GS/OS keeps banks $00, $01, $E0 and $E1, its direct pages and its interrupts. The program draws on the IIgs's Apple II text and Hi-Res screens in bank $E0, reached with long addresses (`src/a2gs.inc`), and turns Super Hi-Res off while it runs. The few places where C code draws on the screen itself (the title screen's letters, Grid Words' tiles) read and write it through `gs_scr_get` and `gs_scr_put` there.

Marinetti is called as tool set 54 through the Tool Locator (`src/mt.c`), with call numbers and parameters from the Marinetti 3.0 Programmers' Guide: TCPIPStartUp, TCPIPConnect when needed, TCPIPDNRNameToIP, TCPIPLogin, TCPIPOpenTCP, TCPIPWriteTCP and TCPIPReadTCP, with TCPIPPoll while it waits. Nothing of Marinetti's is part of the program. The connection has the Uthernet II driver's interface, so net.c, the lobby and the games are unchanged. The files go through GS/OS: `src/gsos.c` turns the few ProDOS calls of `src/prodos.c` into GS/OS calls, so the games come from PARLOR.DAT and the settings from PARLOR.CFG as they do under ProDOS.

## What is here

- `src/boot.s`: the boot block. The Disk II boot ROM loads its six sectors at $0800; it reads the rest of the program with its own 6-and-2 sector reader, switches in the language card's RAM and starts the program. The reader stays in memory to load the overlays.
- `src/crt0.s`, `src/sys.s`, `src/hgr.s`, `src/disk.s`: the start-up, the Super Serial Card (its 6551 read by interrupt when the card's interrupt switch is on, and polled when it is off), the text screen, the keyboard, the AppleMouse II card's firmware calls, and Hi-Res drawing.
- `src/main.c`: the line console and the program's main loop. `src/setup.c` (an overlay): the start screen and the settings. `src/splash.c` (an overlay): the title screen. `src/parlor.c` (an overlay): connecting (with a DNS lookup over the Uthernet II) and signing in. `src/lobby.c`: the lobby. `src/table.c`: the table screen every game shares. `src/ui.c`: text, fills, the pointer and keys on the Hi-Res screen. `src/net.c`: PARLORNET/1 requests and replies.
- `src/eth.c`, `src/w5100.s`: our driver for the Uthernet II's W5100 (TCP on socket 0, UDP for DNS on socket 1). While a connection is up, the serial calls in sys.s go to it, so everything above them works the same over either.
- `src/prodsys.s`: PARLOR.SYSTEM, which starts the program under ProDOS. `src/prodos.c`, `src/mli.s`: ProDOS calls from the program (reading the games from PARLOR.DAT, the settings in PARLOR.CFG, quitting).
- One C file per game, each an overlay loaded at $D000 when it is played: `reversi.c`, `backgammon.c`, `gridwords.c`, `checkers.c`, `mancala.c`, `fiveinarow.c`, `dotsandboxes.c`, `navalcombat.c`, `cannonfire.c`, `rummytile.c`.
- `src/gsshell.s`, `src/gs.s`, `src/gsos.c`, `src/mt.c`, `src/a2gs.inc`: the IIgs program's start (PARLORNET), its calls into the toolbox and GS/OS, its files and mouse, Marinetti, and the macros that let `sys.s` and `hgr.s` build for either machine. `tools/apple2gs.py` writes the OMF load file and the 800K disk.
- `art/`: the font, the games' pictures and the title screen's letters (`splash.txt`, each a grid of blocks), as text; `tools/apple2_art.py` turns them into assembly, tracing the letters' double outlines into short runs.
- `Dockerfile`, `build.sh`: the pinned toolchain (cc65 2.19, whose ca65 also assembles the 65816 parts) and the build of both programs.
- `VERSION`, `tools/release.py`: the version, and the release files with their sums.
- `test/`: tests of the disk tools, and a runner that plays the client in MAME against a lab parlor (test/README.md). `tools/build_apple2.sh` runs them and `tools/apple2_disk.py` makes the disk images in `build/`.

## Memory

| Address | What |
|---|---|
| $0800-$0DFF | the boot block and sector reader |
| $0E00-$1FFF | the serial ring, the reply buffer, the C stack |
| $2000-$3FFF | Hi-Res page 1 |
| $4000-$BFFF | the program |
| $D000-$FEFF | one overlay: the title screen, the start screen and settings, the sign-in, or a game (language card RAM, bank 2) |
| $FF00-$FFFF | the 6502 vectors (the interrupt handler is ours) |

Under ProDOS the program keeps out of ProDOS's way: it ends below $BF00, the boot block's page holds only the overlay table that PARLOR.SYSTEM fills in, $0900-$0CFF is ProDOS's buffer for an open file and $0D00 a page read at a time. The program runs with the auxiliary zero page, stack and language card switched in (ALTZP), so the overlays and the vectors are in the auxiliary language card while ProDOS stays in the main one; each MLI call switches back to ProDOS's side for the call. That needs 128K, and the /RAM disk is disconnected while the program runs.

## Disks and files

`tools/build_apple2.sh` builds everything into `build/`: `parlornet.dsk`, the self-booting disk (DOS 3.3 sector order, 140K, with every game), and `prodos/` with `PARLOR.SYSTEM`, `PARLOR.DAT` and `parlor-files.po`, a ProDOS volume holding just those two files. ProDOS is not part of this program: the player copies the two files to a disk that has their own ProDOS. `gs/` has the IIgs program: `PARLORNET`, `PARLOR.DAT` and `parlornet-gs.po`, an 800K ProDOS volume holding just those two. GS/OS and Marinetti are not on it.

On the IIgs the program's bank has the same layout as the IIe's memory, with nothing at $2000-$3FFF (the screen is in bank $E0) and no vectors at the top; its zero page is in the direct page GS/OS gives the application.

## License

MIT; see LICENSE. The program links the runtime library of the cc65 compiler, whose license asks for this notice:

> This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
>
> Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
>
> 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
> 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
> 3. This notice may not be removed or altered from any source distribution.

The cc65 runtime is copyright Ullrich von Bassewitz, Oliver Schmidt and the cc65 contributors (https://github.com/cc65/cc65).
