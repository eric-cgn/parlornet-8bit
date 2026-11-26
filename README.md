# ParlorNet for 8-bit computers

parlornet-8bit builds ParlorNet's own programs for 8-bit computers: the Apple II family (the IIe, and the IIgs) and the Commodore 64, whose program a Commodore 128 runs too. Each machine has its own screen, input, storage and connection code, around a shared core: the protocol, the sign-in, the lobby, the table, the chat, the dialer, the time signed in and the ten games' rules.

Ports to other 8-bit machines are welcome. [PORTING.md](PORTING.md) explains how the core and the machine layers fit together, how to start a new machine from the template, which machines we would most like to see, and what a port needs before it is merged.

## The Apple IIe and IIgs

A program for the Apple IIe that plays the games of a ParlorNet parlor: Backgammon, Cannon Fire, Checkers, Dots and Boxes, Five in a Row, Grid Words, Mancala, Naval Combat, Reversi and Rummy Tile Classic, in Hi-Res color or monochrome. It works with any ParlorNet parlor, since it speaks the parlor's native protocol, PARLORNET/1, on the parlor's native port (6501); the protocol is described in [docs/protocol.md](https://github.com/eric-cgn/parlornet/blob/main/docs/protocol.md) in the ParlorNet repository (https://github.com/eric-cgn/parlornet). The parlor deals, judges every move and runs the bot, so the program only shows what the parlor sends and sends what the player chooses.

It reaches the parlor over a Super Serial Card (through a bridge, a WiFi modem, or a Unix shell with netcat) or an Uthernet II card, with our own driver for its W5100. It boots from its own disk, with no operating system, or runs from the player's own ProDOS. The player's guide is docs/apple2.md in the ParlorNet repository.

The same program also comes as a GS/OS application for the Apple IIgs, which connects through Marinetti, the IIgs TCP/IP stack, over whatever network Marinetti is set up for: Ethernet, a SLIP or PPP line on a serial port, or MacIP over AppleTalk. See The Apple IIgs, below. On a IIgs the IIe disk and PARLOR.SYSTEM also work as they are, with a Super Serial Card or an Uthernet II in a slot.

## Building

You need Docker and Python 3. Run:

```
tools/build.sh
```

It builds the toolchain image (cc65 2.19 on Debian, pinned by digest, from `Dockerfile`), compiles and links the programs in it (`build.sh`), and makes the disks in `build/`:
- `parlornet.dsk`, a 140K disk in DOS 3.3 sector order that boots by itself;
- `prodos/` with `PARLOR.SYSTEM` and `PARLOR.DAT` for ProDOS;
- `gs/` with the IIgs application, `PARLORNET`, its `PARLOR.DAT`, and `parlornet-gs.po`, an 800K disk holding the two;
- `c64/parlornet.d64`, the C64's 1541 disk.

The same source always gives the same disks. `APPLE2_IMAGE` names another image with cc65 2.19 and Python 3 to use instead.

## Versions and releases

The version is in `VERSION`, one for the whole repository. Changes go into `dev` by pull request, squashed; `main` takes only merges from `dev`, which is how a release is made. Every pull request and every push to `main` builds and runs `python3 -m unittest discover test` (`.github/workflows/ci.yml`); the MAME and VICE labs need ROMs and run only by hand (test/README.md). A pull request into `main` fails unless it comes from `dev`. Every push to `dev` publishes a `dev` pre-release that replaces the last one, with files named for the version, `-dev-` and the commit, such as `parlornet-1.0-dev-46a0304.dsk` (`.github/workflows/dev.yml`).

A push to `main` whose version has no release yet runs the release (`.github/workflows/release.yml`): it builds, runs the tests, tags the commit `v<version>` and publishes a GitHub Release with `parlornet.dsk` (the Apple IIe's self-booting disk), `parlor-files.po` (the ProDOS files, on a ProDOS volume), `parlornet-gs.po` (the Apple IIgs disk) and `parlornet.d64` (the Commodore 64's disk), with their SHA-256 and MD5 sums. `tools/release.py` gathers those files from `build/` into `build/release/`.

## Connecting

The program starts with its title screen, PARLOR over NET in letters of square blocks with a black gap between them, as on the C64, each row of blocks a color of its own: PARLOR white, orange and violet, NET white, green, blue and violet. It is the same with the display set to monochrome, and on a monochrome monitor the colored blocks show as fine stripes. It says "Press a key" and waits for one, then goes on to the start screen. Under ProDOS and on the IIgs the settings can turn the title screen off (T), and the program remembers that in PARLOR.CFG.

Every screen that names the Open-Apple key shows it as its apple and the letter after it, in the key legends, the console, the start screen, the sign-in and the help alike. On the Hi-Res screens it is a glyph of the program's own font, so every machine shows it; on the text screens it is MouseText's apple on an enhanced IIe, a IIc or a IIgs, and "OA-" on an older IIe, which has no MouseText (the program tells them apart by the processor: the enhanced machines have a 65C02 or a 65816). In Grid Words, Open-Apple-V shows the points of every letter and the blank, as the table's rules give them, until the next key. In monochrome, where the text is thin, the keys in the legends are in bold: the apple and its letter, Return, Esc and a game's own letters. In color all the text is bold already.

In Backgammon each step glides its checker from where it was to where it goes, yours as you play them and the other player's one after another when their turn comes in, a hit checker to the bar first. A point with more than five checkers shows four and +n for the rest, and the bar more than three as two and +n. With checkers of yours on the bar, the pointer for the bar sits on them, below the cube.

The lobby lists your own games, then your invitations, then the tables waiting for a player. When they do not all fit on the screen, a More button under the list shows the next screenful, and on the last one it reads "Back to the start". The lobby keeps up to 24 tables and 6 invitations, in the order the parlor lists them, which puts your own tables first.

At a table, what the players say shows on the line above the status line as it comes. A line too long for the screen shows in two parts, three seconds apart, until you press a key, and lines that come together show one after another. The last line said stays there whenever the game has nothing else to say. Open-Apple-T or the Chat button opens the table's chat, with the last twelve lines said there, your own among them, and a line to type in. Return says what you typed and leaves the chat open, and Esc (or Return on an empty line) goes back to the board. When another player sits down, the same line says what they play with, as the parlor tells it, such as "bob joined with GameMaster 1.0.0 + ParlorNet 0.5 on a Macintosh Plus", and it stays in the chat with what was said. If they come back to the game from another program, the line says so: "bob now plays with RSCARDS 5.02 on an IBM PC". Where the player's program named nothing, or the table is private and you are only watching, nothing is shown.

The start screen offers P (connect and sign in), C (a plain line console, for setting up the line by hand) and S (the settings: the serial card's slot, speed and format and the modem's dialing, or the Uthernet II's slot, addresses and the parlor's name and port).

An Apple joystick moves the pointer from one place to the next, as the arrow keys do, once the settings turn it on (J). It starts off, since many Apples have no joystick. Pushing the stick past its middle gives the arrow key for that way, and holding it there repeats it as the keys repeat. The joystick's two buttons are the same lines as the Open-Apple and Closed-Apple keys, so a button pressed and let go with no key gives Return, while a button held with a key is that Apple key, as always. With the joystick on, the Open-Apple key pressed alone gives Return too. An empty game port is noticed and left alone. The IIgs program has the same setting, and under ProDOS and on the IIgs PARLOR.CFG keeps it. The AppleMouse II, or the IIgs's own mouse, works with the joystick on or off.

On the Super Serial Card the program takes the line to be a modem, and P dials it. It sends the init string, `ATZ` as the settings start, and waits for OK, then dials `ATDT` and the number (`ATDP` with Pulse dial) and waits up to a minute for CONNECT. On CONNECT it sends its first line at once, so the parlor's 20 seconds for it are never a race, and signs in. The screen shows each command and what the modem answers. If the modem says NO CARRIER, BUSY, NO DIALTONE, NO ANSWER or ERROR, or nothing comes within the minute, the program says so and offers to dial again, the console (Open-Apple-C) or the start screen (Esc). Esc while it dials sends a Return, which stops a modem's dial, and goes back to the start screen.

The settings for the modem are three lines under the serial card's:

- **Init string**, `ATZ` at first: a command sent before the dial, and answered with OK, such as `ATZ` to reset the modem, `AT&F` or `ATZ` and its options on one line. Key 6 changes it. An empty init string sends nothing before the dial.
- **Number**, `parlornet.org:6501` at first, which the program dials as `ATDTparlornet.org:6501`. Key 7 changes it. An empty number means a line with no modem, and P then sends the first line straight away.
- **Pulse dial**, off at first. Key 8 turns it on, and the program then dials with `ATDP` in place of `ATDT`, for a phone line that takes only pulse dialing.

A number that starts with AT is sent as it is, as the whole dial command, whatever Pulse dial says. A Zimodem (the firmware of many WiFi modems for the C64 and others) needs this: its ATDT asks for a telnet connection, which garbles the parlor's native port, and a host name after its ATD has to be in quotes. Set the number to `ATD"parlornet.org:6501"`, quotes and all, and the modem connects plainly. Another way is the number `"parlornet.org:6501"`, in quotes, with the init string `ATS62=0`, which turns the modem's telnet handling off (or save that setting in the modem once with `ATS62=0&W` and keep `ATZ`). Leave Pulse dial off with a Zimodem, since its ATDP means PETSCII translation, not pulse dialing.



Settings kept by an older version of the program are reset to the defaults.

Under ProDOS the program keeps these in PARLOR.CFG with the other settings. The self-booting disk keeps its settings only until the Apple is switched off, as it does the others.

When you close the connection (Open-Apple-Q), the program says goodbye to the parlor, which hangs up, and the modem says NO CARRIER. A modem that does not notice gets the Hayes escape, `+++` with a second of quiet before and after it, then `ATH`. That escape is sent only then, never during a session. Then the start screen comes back. If the connection is lost while you play, the program says so and Return dials again. A reply that comes back with a byte missing (noise on the line, say) is not a lost connection: the program asks again, or looks at the table again after a move, and goes on.

- **A WiFi modem or a modem emulator** (tcpser, or a computer's modem emulation): press P.
- **A Unix shell:** see Through a Unix shell, below.
- **A bridge** (ser2net, socat, or a cable to a machine that relays to the parlor's port 6501): empty the number in the settings, then press P.
- **An Uthernet II:** set the connection to Uthernet II in the settings, with this Apple's address, netmask, gateway, DNS server and the parlor's name and port, then press P.

The console stays a plain terminal: what you type goes to the line and what comes back is shown. Open-Apple-P there goes into ParlorNet mode on the line as it is, without dialing, for a connection you have made by hand. Going to the console while the program is still connected to the parlor, from the name prompt, the lobby, a table or the start screen, closes that connection first, as Open-Apple-Q does: the parlor hangs up and a dialed line is hung up, so the console starts with the line back at the modem or the shell. To play again, dial with P on the start screen (Open-Apple-Q from the console), or make the connection by hand again and press Open-Apple-P.

Once signed in, the program shows the time signed in, as terminal programs show the time online, in whole minutes: "0m" in the first minute, then "12m", and "1h 5m" after an hour. It starts when sign-in completes, and no screen shows it before then: the sign-in screen has none. It changes once a minute, so the pointer is left alone in between. It is at the right end of the title row in the lobby and at a table. Signing out ends it, and the next sign-in starts again from 0m. Most Apple IIs have no clock, so the parlor gives the time signed in (its TIM request, asked about every 30 seconds along with the program's other requests), and the program counts the seconds in between. The program's own count since sign-in comes first: it takes TIM's time when that is less, or no more than half a minute over it. With a parlor that does not know TIM, the program counts on its own, from the screen's 60 frames a second; that count falls behind by a second or two each time a game is loaded from the disk, and by about five seconds an hour besides.

### Through a Unix shell

When the serial line leads to a Unix shell (a login on a Linux machine or a Raspberry Pi, say), netcat can open the connection. Press C for the console, log in, and type:

```
stty raw -echo; nc parlornet.org 6501; stty sane
```

Then press Open-Apple-P instead of Return. Open-Apple-P sends the Return for the command and goes straight into ParlorNet mode, whose first line follows it at once; the parlor closes a connection that has not sent its first line within 20 seconds, so pressing Return first and Open-Apple-P later can be too slow. `stty raw -echo` passes every byte through and stops the shell echoing what the Apple sends, nc connects to the parlor's native port, and `stty sane` puts the terminal back when the connection ends. Closing the connection from the Apple (Open-Apple-Q in the lobby) makes the parlor hang up, nc ends, and the console is back at the shell prompt. OpenBSD's nc (the usual one on Debian and Ubuntu) and Nmap's ncat notice the hang-up only at the next key you press, which is lost; the traditional nc and `busybox nc` end at once. OpenBSD nc's `-N` and `-q` options do not change this.

The shell can also play the modem with ATZ, a short program in the ParlorNet repository (`tools/ATZ`), and then P dials through it with no console step and no Open-Apple-P race. Install it on the Unix machine under the name ATZ in a directory on your PATH, such as `~/.local/bin`. Then, with the line logged in at a shell prompt, press P: the program's `ATZ` reaches the shell, which echoes it and runs ATZ, and ATZ answers OK as a modem would. The program dials the number in the settings, ATZ connects to the host and port the number names and says CONNECT, and the program signs in, so the number `parlornet.org:6501` reaches parlornet.org. After Open-Apple-Q, ATZ says NO CARRIER and waits for the next dial, and P dials again. Like a real modem it keeps the Hayes escape: a second's quiet, `+++` and a second's quiet bring back its commands, and `ATH` hangs up. To get the shell prompt back, press C for the console and Control-C. ParlorNet's docs page on serial connections (http://docs.parlornet.org/docs/serial.html) covers ATZ, the other ways over a serial line and the cable.

If the parlor goes away while you play (it restarts, say), nc ends too, and the program says "The connection to the parlor was lost." on the program's next request. When P dialed the line, through ATZ or a modem, Return dials again. On a line connected by hand, trying again then reaches only the shell, and the program says there is no answer from the parlor. Press Open-Apple-C for the console, type the nc command again and press Open-Apple-P. While the program connects and on those questions, Open-Apple-C (or C alone) goes to the console; at the name prompt it is Open-Apple-C.

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

The IIgs program is the IIe program's own 6502 code, built from the same source with `IIGS` defined. PARLORNET (`src/apple2/gsshell.s`) starts the Tool Locator, the Memory Manager and the Miscellaneous Tool Set, asks the Memory Manager for a whole 64K bank, copies the program into it at the addresses it uses on the IIe, reserves the Hi-Res page in bank $E0, and jumps to the program in the 65816's native mode with 8-bit registers and the data bank set to the new bank. Absolute addresses then land in that bank, while GS/OS keeps banks $00, $01, $E0 and $E1, its direct pages and its interrupts. The program draws on the IIgs's Apple II text and Hi-Res screens in bank $E0, reached with long addresses (`src/apple2/a2gs.inc`), and turns Super Hi-Res off while it runs. The few places where C code draws on the screen itself (the title screen's letters, Grid Words' tiles) read and write it through `gs_scr_get` and `gs_scr_put` there.

Marinetti is called as tool set 54 through the Tool Locator (`src/apple2/mt.c`), with call numbers and parameters from the Marinetti 3.0 Programmers' Guide: TCPIPStartUp, TCPIPConnect when needed, TCPIPDNRNameToIP, TCPIPLogin, TCPIPOpenTCP, TCPIPWriteTCP and TCPIPReadTCP, with TCPIPPoll while it waits. Nothing of Marinetti's is part of the program. The connection has the Uthernet II driver's interface, so net.c, the lobby and the games are unchanged. The files go through GS/OS: `src/apple2/gsos.c` turns the few ProDOS calls of `src/apple2/prodos.c` into GS/OS calls, so the games come from PARLOR.DAT and the settings from PARLOR.CFG as they do under ProDOS.

## The Commodore 64

The C64 program plays the same games of a ParlorNet parlor, with the same lobby, tables, chat and dialer, on the C64's own screen: a 320 by 200 bitmap, with color in each 8 by 8 cell, and sprites for the pointer and for what moves. It runs on any C64, PAL or NTSC, and on a C128, which starts it in 64 mode by itself. It reaches the parlor through a modem: a SwiftLink-compatible one on the expansion port (a SwiftLink or Turbo232 cartridge, or the modem of a Commodore 64 Ultimate or an Ultimate 64), or a Supra 2400-compatible one on the user port (most WiFi modems for the C64).

So far it has been played in VICE (x64sc, and x128 for the C128's start), with VICE's user-port RS-232 and SwiftLink emulation (test/README.md), not yet on a real C64 or a real modem.

### Starting it

`parlornet.d64` is a 1541 disk image. On a C64, type `LOAD"*",8,1`: the program starts by itself, with no RUN. `LOAD"*",8` and then `RUN` start it as well, and so does `LOAD"PARLORNET",8,1` from a disk with other files on it. On a C128 the disk boots by itself when the machine is switched on with it in drive 8 (or with `BOOT`): the C128 says BOOTING PARLORNET..., goes to 64 mode and starts the program, with no keys to press. Emulators' and cartridges' "run disk" (VICE's autostart, the Ultimate 64's Run, and the like) start it too, as they load the first file.

The disk can be written to a real 1541 or 1571 with OpenCBM and a ZoomFloppy or a similar cable, or used as it is on an SD2IEC, a Pi1541, a 1541 Ultimate or an Ultimate 64. The program needs the disk in the drive while it runs, since each game is loaded from it when it is played, and the drive it was loaded from is the one it uses.

A 1541 is slow, so the files on the disk are packed, and the start-up loads only what the title, the start screen, the settings and the console need. On a stock 1541 the title screen is up about 37 seconds after `LOAD"*",8,1`. The start screen and settings then load for about 10 seconds, with their bar on the title's bottom row, and the title says "Press a key" once they have loaded. The lobby and the tables load once, in about 30 seconds, the first time P or C is pressed, and the sign-in follows in about 11; a game loads in 4 to 17 seconds. A bar on the screen's bottom row fills as each loads. On an SD2IEC, a Pi1541 or a drive with a faster DOS it takes less.

### Keys, joystick and mouse

The Commodore key does what Open-Apple does on the Apple II, and every screen that names it shows its logo and the letter after it: Commodore-C goes to the console, Commodore-Q closes the connection, Commodore-T opens a table's chat, and so on. The left-arrow key and RUN/STOP are Esc, INST/DEL deletes, and the cursor keys move the pointer from one place to the next, as the arrows do on the Apple II. A joystick in either port moves the pointer too, and its fire button is Return. A 1351 mouse in port 1 moves the pointer freely and its left button clicks; it is on when the settings say so (M), since an empty port 1 reads as noise. Commodore-Q on the start screen ends the program and starts the C64 afresh.

### Connecting

The start screen offers P (dial and sign in), C (the line console), S (the settings) and D (color or monochrome, for a monochrome monitor or a black-and-white TV, where the colors become grays). The dialer works as on the Apple II: P sends the init string, `ATZ` at first, and waits for OK, dials `ATDT` (or `ATDP`) and the number, `parlornet.org:6501` at first, and on CONNECT signs in at once. The settings are numbered in the order they show, so with the SwiftLink compatible modem, which has an address, they are 1 to 8, and with the Supra 2400 compatible modem 1 to 7, Speed being 2. With the SwiftLink they are:

- **1 Modem**, pressed to switch between the two:
  - **SwiftLink compatible**, as the settings start: a 6551 on the expansion port. That's a SwiftLink or Turbo232 cartridge, or the modem built into a Commodore 64 Ultimate or an Ultimate 64 (its ACIA mapping set to DE00/NMI or DF80/NMI). It runs at 300, 1200, 2400, 4800, 9600, 19200 or 38400 baud, and choosing it sets 19200.
  - **Supra 2400 compatible**: a modem on the user port, wired as the C64's own RS-232 is, which is how most WiFi modems for the C64 connect. The program has its own driver, which sends and receives at once without losing bits, at 300, 1200 or 2400 baud; choosing it sets 2400. Set the modem to the same speed.
- **2 Address**, shown for the SwiftLink compatible modem only: $DE00 as the settings start, $DF00 or $DF80, pressed to go round them. Set it to where the modem is: a cartridge's jumper gives $DE00 or $DF00, and the Ultimate's ACIA mapping $DE00 or $DF80 ($DF80 keeps clear of an REU, a GeoRAM or an IDE64). The program does not look for the modem; it uses the address set here.
- **3 Speed**, pressed to go round the speeds the modem can do.
- **4 Flow**: no flow control, or RTS/CTS for a modem that holds its data while the program cannot take it.
- **5 Echo** typing in the console.
- **6 Init string**, `ATZ` at first: a command sent before the dial and answered with OK, such as `ATZ` to reset the modem or `AT&F`. An empty init string sends nothing.
- **7 Number**, `parlornet.org:6501` at first, dialed as `ATDTparlornet.org:6501`. An empty number means a line with no modem, where P sends the first line straight away.
- **8 Pulse dial**, off at first: on, the program dials with `ATDP` in place of `ATDT`.

Each setting's value is on its own line beside its name; a number too long for the line shows its start and `>`, and while it is typed the line shows its end.

A number that starts with AT is sent as the whole dial command; with a Zimodem, set the number to `ATD"parlornet.org:6501"` and leave Pulse dial off, as Connecting, above, explains.
- **D Display**, **T** the title screen at start, and **M** a 1351 mouse in port 1.

The settings are kept in PARLOR.CFG on the program's disk, which is written only when they change (so the disk must not be write-protected for them to be kept). Settings kept by an older version of the program, or by one before the two modems, are reset to the defaults.

The console, the line lost and dialed again, the time signed in and the chat work as they do on the Apple II (Connecting, above). An Ethernet connection (ip65 with an RR-Net or an ETH64, or the Ultimate 64's own network) is not there yet; it would go in `signin_net.c` and behind the serial calls, as the machine interface's network calls describe (`src/core/machine.h`).

### How it works

The C64 program is the core's C (`src/core`) built with cc65 for the C64's own machine layer (`src/c64`), which switches BASIC and the KERNAL out and keeps the KERNAL only for the disk. The text screen and the bitmap are in the VIC's bank 3; the character set is the program's own font, the Apple II's, made bold for the C64's screen (`tools/c64_art.py`), with the Commodore logo for the command key.

Each game's rules are in `src/core/games`, as on the Apple II, and its board is drawn for the C64 in `src/c64/draw` (included by the core's file for the game). The boards are made of whole 8 by 8 cells wherever two colors meet: Reversi's squares in two greens, Checkers in green and buff with a sprite frame round the chosen piece, Backgammon's points as lanes of red and green with a sprite for the checker that glides, Grid Words a letter to a cell, Naval Combat's sea and ships, and Cannon Fire's arena with a sprite for the shell. A C64 pixel is near enough square on a PAL screen (about 0.94 as wide as it is tall) and narrower on NTSC (about 0.75), so the squares are square in pixels and a little tall on NTSC.

The user port's driver (`src/c64/serial.s`) receives by CIA 2's NMI: the start bit's edge on FLAG starts timer B, whose underflows sample each bit near its middle. It sends by CIA 1's timer A on the IRQ, each bit made ready before its time and sent 0.4 of a bit early, with two stop bits, so that an interrupt held up by the other one still puts each bit on the line in time. The SwiftLink driver reads its 6551 by NMI. A 60 Hz tick (on PAL too) comes from the raster interrupt, which also reads the keyboard, the joysticks and the mouse and moves the sprites.

| Address | What |
|---|---|
| $0400-$07FF | the serial line's receive and send rings, the C stack |
| $0800-$14FF | the reply buffer (the loader before it) |
| $1500-$92FF | the program (PN.MAIN), its variables, then the lobby and the tables (PN.PARLOR) |
| $9300-$98FF | a table's moves and chat, or the lobby's lists |
| $9900-$BFFF | one overlay: the sign-in, a game, the start screen and settings, or the title screen |
| $C000-$C7FF | the character set, and the same inverted |
| $C800-$CFFF | the text screen and the bitmap's colors (the loader, PN.BOOT, at start-up) |
| $D000-$DFFF | the sprites' shapes, under the I/O |
| $E000-$FF3F | the bitmap, under the KERNAL |

The disk (`tools/c64_disk.py`) holds:
- the C128's boot sector;
- PARLORNET, which starts the rest;
- PN.BOOT, the loader;
- PN.CHARS, the character set's first half;
- PN.MAIN, the program: what the title, the start screen, the settings and the console need;
- PN.PARLOR, the lobby, the tables and the parlor's protocol, loaded once, before the first sign-in;
- one PN.<NAME> file for each overlay;
- PARLOR.CFG with the settings as they start.

The program and the overlays are packed with the program's own LZ packing (`tools/c64_pack.py`, about 30% smaller), loaded at the top of their memory and unpacked in place.

## What is here

- `src/core`: what every machine shares.
  - `main.c`: the line console and the program's main loop.
  - `parlor.c` (an overlay): connecting, the dialer and signing in.
  - `lobby.c`: the lobby.
  - `table.c`: the table screen every game shares, with its chat.
  - `net.c`: PARLORNET/1 requests and replies.
  - `games.c`: the catalog.
  - `games/`: one file per game, its rules (the view's lines, the moves it sends), each an overlay. It includes the machine's drawing of its board, `draw/NAME.c`.
  - `machine.h`: the machine interface, everything the core asks of a machine (the line, input, the text screen, the graphics screen kit, the games' drawing, overlays, the clock, start-up), each with what the Apple II and the C64 do. Each machine's `machdefs.h` sets its constants and capabilities.
- `src/common`: what more than one machine uses.
  - `ui_targets.c`: the screen kit's targets, the nearest one for the arrow keys, and waiting for a key, in plain C.
  - `ui_hgr.c`: text, outlines and buttons, for a machine whose drawing has the Apple's Hi-Res primitives (the Apple II's and the C64's).
  - `6502/reply.inc`, `6502/clock.inc`: the reply reader and the time signed in, in 6502 assembly that each machine's `sys.s` includes.
- `src/apple2`: the Apple IIe and IIgs (`apple2.h` declares the machine's own calls).
  - `boot.s`: the boot block. The Disk II boot ROM loads its six sectors at $0800; it reads the rest of the program with its own 6-and-2 sector reader, switches in the language card's RAM and starts the program. The reader stays in memory to load the overlays.
  - `crt0.s`, `sys.s`, `hgr.s`, `disk.s`: the start-up, the Super Serial Card (its 6551 read by interrupt when the card's interrupt switch is on, and polled when it is off), the text screen, the keyboard, the AppleMouse II card's firmware calls, and Hi-Res drawing.
  - `line.c`: the serial line's settings and opening, start-up and quitting.
  - `setup.c` (an overlay): the start screen and the settings.
  - `splash.c` (an overlay): the title screen.
  - `ui.c`: text, fills, the pointer and keys on the Hi-Res screen.
  - `joy.s`: the Apple joystick, read as arrow keys and Return. On the IIe it is in the vectors' page at $FF00, which the start-up loads with them.
  - `draw/`: each game's board.
  - `eth.c`, `w5100.s`, `signin_net.c`: our driver for the Uthernet II's W5100 (TCP on socket 0, UDP for DNS on socket 1) and the sign-in's DNS lookup. While a connection is up, the serial calls in sys.s go to it, so everything above them works the same over either.
  - `prodsys.s`: PARLOR.SYSTEM, which starts the program under ProDOS.
  - `prodos.c`, `mli.s`: ProDOS calls from the program (reading the games from PARLOR.DAT, the settings in PARLOR.CFG, quitting).
  - `gsshell.s`, `gs.s`, `gsos.c`, `mt.c`, `a2gs.inc`: the IIgs program's start (PARLORNET), its calls into the toolbox and GS/OS, its files and mouse, Marinetti, and the macros that let `sys.s` and `hgr.s` build for either machine.
- `src/c64`: the Commodore 64 (The Commodore 64, above; `c64.h` declares the machine's own calls).
  - `stub.s`, `boot128.s`, `loader.s`: PARLORNET, the C128's boot sector and the loader.
  - `crt0.s`, `sys.s`: the start-up, the interrupts, the keyboard, joysticks and mouse, and the text screen.
  - `serial.s`: the user port's and the SwiftLink's drivers.
  - `gfx.s`, `ui.c`: the bitmap and sprites.
  - `disk.s`, `ovl.c`: the KERNAL's disk, the overlays and the progress bar.
  - `line.c`, `setup.c`, `splash.c`: the line's settings, the start screen and the title screen.
  - `draw/`: each game's board.
- `src/template`: a machine with every call of the interface stubbed, which the build compiles and links with the core and every game, as the start of a port (PORTING.md).
- `art/`: the font, the title screen's letters (`splash.txt`, each a grid of blocks) and each machine's pictures (`art/apple2`, `art/c64`), as text. `tools/apple2_art.py` and `tools/c64_art.py` turn them into assembly (the title letters as a byte a row of blocks; the C64's character set made bold).
- `Dockerfile`, `build.sh`: the pinned toolchain (cc65 2.19, whose ca65 also assembles the 65816 parts) and the build of every program.
- `VERSION`, `tools/release.py`: the version, and the release files with their sums.
- `tools/`:
  - `build.sh` runs the build;
  - `apple2_disk.py` and `apple2gs.py` make the Apple's disks;
  - `c64_disk.py` and `c64_pack.py` make the C64's disk.
- `test/`: tests of the disk tools, and runners that play the programs in MAME (the Apple II) and VICE (the C64) against a lab parlor (test/README.md).

## Memory

The Apple IIe's (the C64's is under The Commodore 64, above):

| Address | What |
|---|---|
| $0800-$0DFF | the boot block and sector reader |
| $0E00-$1FFF | the serial ring, the reply buffer, the C stack |
| $2000-$3FFF | Hi-Res page 1 |
| $4000-$BFFF | the program |
| $D000-$FEFF | one overlay: the title screen, the start screen and settings, the sign-in, or a game (language card RAM, bank 2) |
| $FF00-$FFFF | the joystick's code (`joy.s`) and the 6502 vectors (the interrupt handler is ours) |

Under ProDOS the program keeps out of ProDOS's way: it ends below $BF00, the boot block's page holds only the overlay table that PARLOR.SYSTEM fills in, $0900-$0CFF is ProDOS's buffer for an open file and $0D00 a page read at a time. The program runs with the auxiliary zero page, stack and language card switched in (ALTZP), so the overlays and the vectors are in the auxiliary language card while ProDOS stays in the main one; each MLI call switches back to ProDOS's side for the call. That needs 128K, and the /RAM disk is disconnected while the program runs.

## Disks and files

`tools/build.sh` builds everything into `build/`: `parlornet.dsk`, the self-booting disk (DOS 3.3 sector order, 140K, with every game), and `prodos/` with `PARLOR.SYSTEM`, `PARLOR.DAT` and `parlor-files.po`, a ProDOS volume holding just those two files. ProDOS is not part of this program: the player copies the two files to a disk that has their own ProDOS. `gs/` has the IIgs program: `PARLORNET`, `PARLOR.DAT` and `parlornet-gs.po`, an 800K ProDOS volume holding just those two. GS/OS and Marinetti are not on it. `c64/` has the C64's disk, `parlornet.d64` (The Commodore 64, above).

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
