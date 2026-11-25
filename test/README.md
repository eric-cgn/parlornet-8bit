# Tests

`test_disk.py` checks the disk tools: the ProDOS volume they write reads back as ProDOS would read it,
and a built client (`tools/build_apple2.sh`) fits its disk and, under ProDOS, ends below ProDOS's
global page. Run it with `python3 -m unittest discover test`.

## Playing in MAME

`lab/` drives the client in MAME's Apple IIe (enhanced) against a lab parlor, takes screenshots and
plays the other side's moves where a plan asks for them. You need Docker, a ParlorNet checkout for the
lab parlor, and the Apple IIe ROM sets for MAME (apple2ee, a2ssc, a2mouse, m68705p3, a2diskiing),
which you supply: no ROMs are part of this repository.

```
PARLORNET=~/src/parlornet test/lab/parlor.sh
tools/build_apple2.sh
A2ROMS=~/roms test/lab/run.sh 16601 build/parlornet.dsk test/lab/plans/backgammon.txt test/out/backgammon
```

- `mame/run.lua` is the step runner MAME loads; it reads the plan (wait, waittext, title, type, key,
  arrows, click, point, oa, caps, snap, dumptext, set, hold, need, exit) one step per line. The title
  screen waits for a key, so every plan has a `title` step, which waits for the title screen and presses
  Return (`gsrun.lua` has the same step for the IIgs). `hold mono 1 900` keeps the program's mono setting
  on while it starts, so `splash_mono.txt` shows the title screen in monochrome; `splash.txt` shows it in
  color.
- `lab/orch.py` answers a plan's `need` lines: it signs in as the player on a connection of its own,
  reads the table's view and writes the clicks and keys of a legal move back to the runner.
- `lab/bridge.py` is the other end of the serial line: a plain relay to the parlor (port 16601) or a
  stand-in for a WiFi modem that answers AT commands and connects on ATDT (port 16600; `reversi.txt`
  dials it from the console, the `dial` plans with P). The stand-in also takes `+++` with a second's quiet
  on each side back to command mode, where ATH hangs up, and some numbers stand for a modem's other
  answers: one with BUSY in it gives BUSY, NOCARRIER gives NO CARRIER, SILENT gives nothing until the next
  byte (which stops the dial, with NO CARRIER), and STAY connects but does not notice the parlor hanging
  up. Everything it sees is in `work/srv/modem.log`.
- P dials a modem first, which the plain relay cannot answer, so on port 16601, or with `DIRECT=1`,
  `run.sh` adds `hold cfg_number 0 30` after a plan's `title` step: the dialer's number is then empty, as
  a player on a direct line sets it, and P sends the opening at once.
- `mame/w5100.lua` is a test double of the Uthernet II's W5100, since MAME has no emulation of that
  card, and `lab/eth_relay.py` carries its TCP connection to the parlor. Like the chip, the double takes
  in only as much as its 2K receive buffer has room for, and the rest waits. Run a plan with `ETH=1` to
  use them (`uthernet.txt`).
- `lab/pngscale.py` scales MAME's snapshots (560 by 192) to 560 by 384.

`plans/table_entry.txt` starts each of the ten games against the bot from the lobby, one after another on
one connection, and shows its first view (`te_NAME.png`); the lobby comes back after each. A crash would
show as the title screen and clicks with no target in `run.log`. `plans/table_entry_long.txt` does the same
for a player whose table list is longer than the program's reply buffer (3328 bytes), as for a player with
many tables and invitations on a busy parlor; make that player first with `lab/longlist.py olga 16501`. A
reply that overran the buffer used to lose its final OK, and the program took the connection as lost, in
the lobby or on entering a new table; now it keeps what fits, and a table asks for its own line
only (`LST table=id`), since a new table's line comes last among the player's own.

`parlor.sh` takes `MOTD=file` for the parlor's message of the day. `plans/motd.txt` shows the live one
(`MOTD=deploy/motd.txt`, from the ParlorNet checkout): all 19 lines, a " More: press a key " pause on the
bottom row since the name prompt does not fit below them, then the name prompt on a fresh screen.
`plans/motd_short.txt` shows a message of a few lines going on to the name prompt with no pause, and
`plans/motd_long.txt` one of 20 long lines (`plans/motd_long.motd`) wrapped over two screens.

`parlor.sh` enrolls the bot's key in the lab parlor's key directory (test/work/srv/keys) before it starts the
bot, which signs in as a server extension.

`plans/chat.txt` plays Reversi against the bot and shows the table's chat: the bot's greeting on the message
line as the table opens, in two parts three seconds apart (`chat_hello`, `chat_hello2`), the chat view
(Open-Apple-T) with it, a line said there and shown in the view (`chat_said`), the board again with that line
on the message line, kept through a move (`chat_board`, `chat_moved`), and the same in monochrome.
`plans/chat_backgammon.txt` and `plans/chat_gridwords.txt` show the greeting and a line said on the two
busiest boards. The bot sits down before the table's first view is fetched, so these check that the program
reads the last events (SIN from its own mark, a little before the view's) when it opens a table.
`plans/join.txt` shows what a player who sits down plays with. pat waits at a Reversi table, and
`lab/joinopp.py` (started just before the plan: `python3 test/lab/joinopp.py 16501 30 &`) signs up bob, names
GameMaster on a Macintosh Plus, and sits down: the message line says "bob joined with GameMaster 1.0.0 +
ParlorNet 0.5 on a Macintosh Plus" in two parts (`join_1`, `join_2`), and the chat view has it (`join_view`).
Half a minute later bob chats from another connection that names RSCARDS on an IBM PC, and the chat says
"bob now plays with RSCARDS 5.02 on an IBM PC" before his line (`change_*`).
`plans/keys_mono.txt` shows the key legends in monochrome, where the keys are bold: the lobby, the list of
games, Backgammon's two pages of Keys, the question before resigning and the chat view, then the table's
keys in color.

`plans/bg_anim.txt` takes snapshots three frames apart while Backgammon's checkers glide: two steps of the
player's (`a01` on, `b01` on), the bot's turn after Done (`c01` on), and a step in monochrome with the home
board on the left (`d01` on). The orchestrator's `bgsnapX` plays the first legal step and takes the
snapshots `X01` to `X30`. `plans/bg_bar.txt` plays until a checker is on the bar (`bgplay`, which hits),
then until one of the player's is (`bgbarme`, which does not), and each time points at the bar from point
7 with the up arrow: the pointer sits on the player's checkers on the bar, below the cube, with the home
board on the right and on the left. Tall stacks show four checkers and +n. `plans/fingers.txt` opens Five in
a Row, Naval Combat and Grid Words and shows the pointer on the middle of a point or square, at the first one
and one along and one down from it.

`plans/clock.txt` shows the connected time: on the sign-in's title row ("ParlorNet  0:05"), at the right
end of the lobby's and a table's title row, and from 0:00 again after Open-Apple-Q and a new connection. The
time comes from the parlor's TIM request (`TIM secs now`) when the parlor has it; a parlor without it
answers ERR UNKNOWN and the program counts the ticks alone.

`plans/keys.txt` shows the Open-Apple key in the console and the sign-in (MouseText on the enhanced IIe), in
the lobby and at the table (the font's glyph), Grid Words' letter points (Open-Apple-V), and its tiles in
color and in monochrome. MAME's apple2e, the older IIe without MouseText, shows "OA-" on the text screens
instead.

`plans/reversi_discs.txt` plays a move of Reversi against the bot and shows the board in color, in monochrome
on a monochrome monitor, in monochrome on a color monitor and in color on a monochrome monitor. On a monitor a
Hi-Res pixel is about 7/6 as tall as it is wide (280 pixels at 7.16 MHz across a line, 192 lines of a 240-line
field), so a square is 21 pixels by 18 lines and the discs are 14 pixels by 12 lines. The snapshots scaled to
560 by 448 (each line 7/6 of a pixel's width) show the squares and discs as a monitor does; `pngscale.py`'s
560 by 384 shows square pixels, where they look a little tall.

`plans/cannonfire_shot.txt` plays Cannon Fire against the bot: the tanks' barrels and the player's lead at
the opening aim and at a second one (`cfs_aim1`, `cfs_aim2`), then a shot every 5 frames from Fire on
(`cfs_shot_*`: the shell along its path, the burst, the crater's ground dissolving), the bot's answering
shot the same way (`cfs_bot_*`), and a third aim and a shot in monochrome on a monochrome monitor
(`cfs_mono_*`), where each tank is cut out of the ground with a black edge and a hollow hull. The bot answers before the client has finished showing the player's shot, so this also
checks that the bot's shot is shown, from the events after the move's reply, before the view that already
has it.

## Through a Unix shell

`lab/shell/` is a container with a shell on a pty behind a TCP port, as a login over a serial line
would be, with nc. `plans/unixshell.txt` types `stty raw -echo; nc host.docker.internal 16501; stty
sane` in the console, presses Open-Apple-P, signs in, closes the connection from the lobby and checks
that it is back at the shell prompt:

```
docker build -t parlornet-8bit-shell test/lab/shell
docker network create a2lab
docker run -d --name a2shell --network a2lab parlornet-8bit-shell
NET=a2lab LINE=a2shell:2323 A2ROMS=~/roms test/lab/run.sh 0 build/parlornet.dsk test/lab/plans/unixshell.txt test/out/shell
```

`plans/reconnect.txt`, on the same line, is the parlor going away under a signed-in player, as when it
restarts. It runs nc under `timeout 90`, so nc ends while the player is in the lobby; the lobby's next
request finds the shell, and the program says "The connection to the parlor was lost." (`rc_lost`). Trying
again (Return) sends the opening to the shell, which echoes it: the echo is not a greeting, and the shell
does not answer PRL as a parlor would, so the program says there is no answer (`rc_retry`) rather than
"Connected to parlor". Then nc to port 16509, where nothing listens, stands for the parlor still being
down (`rc_noanswer`), Open-Apple-C goes to the console from there, and nc to the parlor again signs the
player back in (`rc_back_lobby`).

## The dialer

P on the start screen dials the modem: ATZ (when the settings' Reset first is on), the init string, then
ATDT and the number, and on CONNECT the opening. These plans dial the WiFi-modem stand-in on port 16600:

```
A2ROMS=~/roms test/lab/run.sh 16600 build/parlornet.dsk test/lab/plans/dial.txt test/out/dial
```

- `plans/dial.txt` dials with the settings as they start (`dial_settings`): ATZ, OK, `ATDTparlornet.org:6501`
  (the stand-in connects any number to the lab parlor), CONNECT 9600 and the name prompt (`dial_dialed`).
  It signs up, closes the connection with Open-Apple-Q, which the stand-in ends with NO CARRIER when the
  parlor hangs up (so no `+++` in `modem.log`), and is back at the start screen (`dial_hungup`), where P
  dials again and signs in.
- `plans/dial_init.txt` turns Reset first off and sets the init string `AT&F`: P sends AT&F and dials, with
  no ATZ (`di_dialed`). The init string `X1`, which the stand-in answers with ERROR, stops the dial: "The
  modem says ERROR to X1." (`di_error`). Then both on: ATZ, AT&F, the dial (`di_both`). Last, the number
  `ATD"parlornet.org:6501"`, which starts with AT, is sent as the whole dial command (`di_atd_settings`,
  `di_atd`), as a Zimodem needs for a plain connection.
- `plans/dial_errors.txt` dials BUSY (`de_busy`), and Return dials again; NOCARRIER (`de_nocarrier`); and
  SILENT, which ends after a minute in "No CONNECT from the modem in a minute." (`de_timeout`). Return dials
  SILENT again (`de_dialing`), and Escape sends a Return, which stops the dial (the stand-in's NO CARRIER),
  and goes back to the start screen (`de_cancelled`).
- `plans/dial_stay.txt` dials STAY: after Open-Apple-Q no NO CARRIER comes, so the program sends `+++`, gets
  OK and sends ATH; `modem.log` shows "escape" and "hangup", and P dials again (`ds_again`).

Through a Unix shell the dialer needs no console step when pndial (`tools/pndial` in the ParlorNet
repository) is installed as `ATZ`: the program's ATZ reaches the shell prompt, the shell echoes it and runs
pndial, which answers OK. Install it in the shell container, then run the plans with the line on it:

```
docker cp "$PARLORNET/tools/pndial" a2shell:/usr/local/bin/pndial
docker exec -u root a2shell sh -c 'chmod 755 /usr/local/bin/pndial; ln -sf pndial /usr/local/bin/ATZ'
NET=a2lab LINE=a2shell:2323 A2ROMS=~/roms test/lab/run.sh 0 build/parlornet.dsk test/lab/plans/dial_pndial.txt test/out/pndial
```

- `plans/dial_pndial.txt` sets the number to `host.docker.internal:16501` (`pn_settings`) and presses P:
  ATZ, OK from pndial, the dial, CONNECT and the name prompt (`pn_dialed`). It signs up, closes the
  connection and dials again (`pn_again`). The shell has OpenBSD's nc, which notices the parlor's hang-up
  only when the program sends `+++`; pndial then says NO CARRIER and waits for the next dial. At the end the
  console shows pndial answering AT (`pn_console`).
- `plans/dial_pndial_lost.txt` runs in a second shell container started with `-e NC='timeout 75 nc'`, so
  that the connection ends 75 seconds after the dial, as when the parlor restarts. The lobby's next request
  finds NO CARRIER: "The connection to the parlor was lost." (`pl_lost`), and Return dials again and signs
  in (`pl_redialed`, `pl_lobby2`).

## The IIgs program in MAME

`lab/gs.sh` plays the IIgs program in MAME's Apple IIgs (ROM 03) with System 6.0.1 and Marinetti, on a Docker
network of its own that has no way out. MAME's modem port line goes to `lab/slirp/`, a container with slirp,
which speaks SLIP in user space and needs no privileges; slirp connects on to the lab parlor, a container
named `parlor` with parlord, the bot and the orchestrator. Marinetti looks the parlor's name up through slirp's
DNS, so a plan types `parlor` as the parlor's name.

You supply the ROM sets (apple2gs and a2cffa2) and a 32MB hard disk image with System 6.0.1 and Marinetti 3.0
(3.0b1 and the 3.0b11 TCPIP update), set up in the TCP/IP control panel for the Direct Connect link layer
(plain SLIP) on the modem port at 9600 baud, with the address 10.0.2.15 and the DNS server 10.0.2.3. None of
these are part of this repository. The plan runs on a copy of the disk. MAME's IIgs loses incoming packets
at 19200 baud, so the line runs at 9600.

```
tools/build_apple2.sh
GSROMS=~/roms GSHD=~/gs/marinetti.2mg PARLORNET=~/src/parlornet test/lab/gs.sh test/lab/plans/gs_reversi.txt test/out/gs
```

- `mame/gsrun.lua` is the IIgs step runner. Besides the IIe runner's steps it drives the ADB mouse on the
  desktop (`mto`, `click`, `dclick`, `drag`) and the ADB keys (`key cmd+q`), and finds the program's bank by
  its first bytes, so that `click ID` can point at a target as the arrow keys would.
- `plans/gs_reversi.txt` opens the program from the Finder, signs up, plays Reversi against the bot to the
  end, closes the connection and quits to the Finder. `plans/gs_splash.txt` shows the title screen in
  color, then in monochrome and then not at all, each set in the settings and kept in PARLOR.CFG across a
  quit and a new start. `plans/gs_gridwords.txt` plays some words of Grid Words, in color and monochrome.
