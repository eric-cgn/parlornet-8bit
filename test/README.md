# Tests

`test_disk.py` checks the disk tools: the ProDOS volume they write reads back as ProDOS would read it,
and a built client (`tools/build.sh`) fits its disk and, under ProDOS, ends below ProDOS's
global page. `test_nufx.py` reads the ShrinkIt archives back with NuLib2 (the `nulib2` package on Debian
and Ubuntu) and checks that they hold the same files as the ProDOS volumes, with the same types. Without
NuLib2 those tests are skipped. Run them all with `python3 -m unittest discover test`.

## Playing in MAME

`lab/` drives the client in MAME's Apple IIe (enhanced) against a lab parlor, takes screenshots and
plays the other side's moves where a plan asks for them. You need Docker, a ParlorNet checkout for the
lab parlor, and the Apple IIe ROM sets for MAME (apple2ee, a2ssc, a2mouse, m68705p3, a2diskiing),
which you supply: no ROMs are part of this repository.

```
PARLORNET=~/src/parlornet test/lab/parlor.sh
tools/build.sh
A2ROMS=~/roms test/lab/run.sh 16601 build/parlornet.dsk test/lab/plans/backgammon.txt test/out/backgammon
```

- `mame/run.lua` is the step runner MAME loads; it reads the plan (wait, waittext, title, type, key,
  arrows, click, point, oa, caps, snap, dumptext, set, hold, need, joy, stick, button, nostick, where,
  poke, watch, exit) one step per line. The title
  screen waits for a key, so every plan has a `title` step, which waits for the title screen and presses
  Return (`gsrun.lua` has the same step for the IIgs). `splash.txt` shows the title screen on a color
  monitor, and `splash_mono.txt` on a monochrome monitor, with `hold mono 1 900` keeping the program's
  mono setting on while it starts; in monochrome the title's blocks are white.
- `lab/orch.py` answers a plan's `need` lines: it signs in as the player on a connection of its own,
  reads the table's view and writes the clicks and keys of a legal move back to the runner.
- `lab/bridge.py` is the other end of the serial line: a plain relay to the parlor (port 16601) or a
  stand-in for a WiFi modem that answers AT commands and connects on ATDT (port 16600; `reversi.txt`
  dials it from the console, the `dial` plans with P). The stand-in also takes `+++` with a second's quiet
  on each side back to command mode, where ATH hangs up, and some numbers stand for a modem's other
  answers: one with BUSY in it gives BUSY, NOCARRIER gives NO CARRIER, SILENT gives nothing until the next
  byte (which stops the dial, with NO CARRIER), and STAY connects but does not notice the parlor hanging
  up. Everything it sees is in `work/srv/modem.log`. As a plain relay it can also lose a byte on the
  way to the Apple, once (its last argument, DROP: the O of the first OK line after that many bytes), so
  that a reply comes without its final line and the program asks for it again.
- P dials a modem first, which the plain relay cannot answer, so on port 16601, or with `DIRECT=1`,
  `run.sh` adds `hold cfg_number 0 30` after a plan's `title` step: the dialer's number is then empty, as
  a player on a direct line sets it, and P sends the opening at once.
- `mame/w5100.lua` is a test double of the Uthernet II's W5100, since MAME has no emulation of that
  card, and `lab/eth_relay.py` carries its TCP connection to the parlor. Like the chip, the double takes
  in only as much as its 2K receive buffer has room for, and the rest waits. Run a plan with `ETH=1` to
  use them (`uthernet.txt`).
- `lab/pngscale.py` scales MAME's snapshots (560 by 192) to 560 by 384.

`plans/waiting.txt` opens a table of each of the ten games with Wait for anyone, one after another, and shows its waiting table (`wt_NAME.png`): the game's empty board, with "Waiting for a second player." and "Type /invite bot to play the bot." under it. `plans/gs_waiting.txt` does the same on the IIgs, and `vice/plans/waiting.txt` on the C64.

The step `settle` waits until the program waits for a key with everything drawn, which is when for half a second it has read the keyboard all along and the screen has not changed. The plans that save a screenshot of each step settle before each snap, so that a screen still being drawn (after a load from the 1541, say) is not taken.

`plans/table_entry.txt` starts each of the ten games against the bot from the lobby, one after another on
one connection, and shows its first view (`te_NAME.png`); the lobby comes back after each. A crash would
show as the title screen and clicks with no target in `run.log`. `plans/table_entry_long.txt` does the same
for a player whose table list is longer than the program's reply buffer (1024 bytes), as for a player with
many tables and invitations on a busy parlor; make that player first with `lab/longlist.py olga 16501`. The
parlor sends such a reply in pages that fit the buffer (PAG), and the program asks for each next page (CNT)
until the lobby's list is full, then drops the rest. A table asks for its own line only (`LST table=id`). `plans/long_list.txt`, for the same player, shows the lobby and More, then
`/list`, the parlor's whole list, as pages of text: Right turns them, and at the last one the program asks
the parlor for the reply's next page.

`plans/lobby_names.txt` shows a lobby longer than one screen, of every game, for a player made first with `lab/manytables.py nora 16501` (4 tables of their own, 2 invitations and 18 tables waiting for a player): the first screenful and More (`ln_lobby`), the rest and Back to the start (`ln_more`), the first again (`ln_back`), then the lobby after the player's Backgammon table has been opened and left (`ln_after_table`) and the list of New game (`ln_newgame`). Every name is a game's. Loading a game from the self-booting disk used to read the disk's first sector over the lobby's game names, which then showed as stray characters until the player signed in again.

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
board on the right and on the left. `plans/fingers.txt` opens Five in
a Row, Naval Combat and Grid Words and shows the pointer on the middle of a point or square, at the first one
and one along and one down from it.

`plans/dhr_backgammon.txt` shows the IIe's Double Hi-Res screens: the title screen (`dhr_title`), the lobby
(`dhr_lobby`) and a new Backgammon board in color (`dhr_color`, and `dhr_color_ptr` with the pointer on point 6),
then in monochrome on MAME's green, white and amber monitors (`dhr_green`, `dhr_green_ptr`, `dhr_white`,
`dhr_amber`), and a step gliding (`a01` to `a30`). MAME's monitor is set with `set :a2video:a2_video_config
Monitor type N`: 0 color, 4 white, 5 green, 6 amber.

`plans/dhr_timing.txt` times Backgammon's drawing. Build with `-D DHR_TIMING` added to `A2FLAGS` in build.sh,
and draw/backgammon.c marks each drawing at $FEFF (in the language card, which the kit's switches to the auxiliary memory do not move), which the plan's `watch FEFF` logs frame by frame:
full boards in color and in monochrome (the plan sets the display with `hold mono`, and closing help draws the whole table again),
a step of the player's and the bot's turn. `python3 test/lab/marks.py OUTDIR/run.log` prints the frames each
took. The machine runs at 1.023 MHz, 60 frames a second, so a frame is about 17,000 cycles. The `poke` step
writes bytes into the program's memory (into an overlay's, it waits for a frame where the language card's bank
2 is in, since the Double Hi-Res kit runs with bank 1 in), so a position can be put into Backgammon's `pt[]`
and drawn by the display toggle.

`plans/k64.txt` boots the self-booting disk on a IIe without the auxiliary 64K
(`MAMEARGS="-ramsize 64K -aux std80"`, or `-aux ""` for none) and shows the boot block's sentence, "ParlorNet
needs an Apple IIe with 128K, or a IIc or IIgs." (`k64`, `k64_text`). `MACH=apple2c` runs any plan on a IIc,
whose modem port takes the serial line, and which needs MAME's apple2c ROM set.

`plans/clock.txt` shows the time signed in, in whole minutes. The sign-in screen has no clock (`ck_name`); it
is "0m" at the right end of the lobby's title row right after sign-in (`ck_lobby`), "1m" by the table's
snapshots, and gone after signing out (`ck_out`). The next sign-in starts again from 0m (`ck_lobby2`), and so
does the sign-in on a new connection after Open-Apple-Q (`ck_name3`, `ck_lobby3`). The time comes from the
parlor's TIM request (`TIM secs now`, the time signed in) when the parlor has it; a parlor without it answers
ERR UNKNOWN and the program counts the ticks alone.

The machine has the Apple joystick in its game port (`-gameio joy`), and `mame/joy.lua` sets it for the
runners. `joy BITS [N]` holds it for N frames as the VICE runner's `joy` does (1 up, 2 down, 4 left, 8 right,
16 button 0, 32 button 1), with the stick all the way over, then lets go. `stick X Y` holds the stick
anywhere (0 to 255 on each axis, 128 the middle), and `button B V` holds a button. `nostick 1` makes the port
read as an empty one does, whose paddle timers never run out. `where` logs the id of the target the pointer
is on, so the run's log shows where each push took it. `plans/joystick.txt` shows the setting (J, off at
first, then on) and the joystick in the lobby: a push right and left moves the pointer one place, a push held
for a second and a half repeats, the stick within its middle does nothing, Open-Apple-D changes the display
with the joystick on and gives no Return, and button 0 is Return. `plans/joystick_game.txt` chooses Reversi
and the bot with the stick and both buttons, places a disc with the stick and button 0, opens the chat with
Open-Apple-T, and then, with the port empty, shows that the stick and the Open-Apple key alone do nothing.

`plans/keys.txt` shows the Open-Apple key in the console's title row and sentences and in the sign-in
(MouseText on the enhanced IIe), in the lobby and at the table (the font's glyph), Grid Words' letter points
(Open-Apple-V), and its tiles in color and in monochrome. MAME's apple2e, the older IIe without MouseText,
shows "OA-" on the text screens instead.

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

`plans/cannonfire_incoming.txt` is for a lab parlor whose bot thinks as the deployed one does
(`THINK=0.5,2 test/lab/parlor.sh`; the bot otherwise moves at once): the player fires, and the bot answers
while the player's shot is still being shown, so the view fetched after it already has the bot's shot. The
snapshots every 6 frames from Fire on (`cfi_*`) show the bot's shell flying in along its path, its burst and
its crater dissolving, before the view: the program sees that the view is newer than the events it has
read, reads those first and fetches the view again. `cfi_after` and `cfi_mono` show each tank's health over
its letter, in color and in monochrome.

`plans/command_line.txt` shows the command line. In the lobby `/who` answers on the message line, and the buttons are unlit while the line is open (`cl_typing`). At a Reversi table that waits for a player, `/invite bot` seats the bot (`cl_invited`, `cl_seated`) and the typed move `/d3` plays a disc (`cl_typed_move`). `/resign` opens the resign dialog, which starts on No (`cl_resign_dialog`), and Esc answers No (`cl_resign_no`). An unknown command says so (`cl_unknown`), and `/leave` goes back to the lobby. `plans/gs_command_line.txt` does the same on the IIgs, and `vice/plans/command_line.txt` on the C64.

`plans/offers_backgammon.txt` shows offers and confirmations in dialogs. `lab/offers.py` plays ann beside it (`python3 test/lab/offers.py 16501 backgammon olly "Swordfish 1997"`): she opens a Backgammon table and invites olly, and once he sits down she doubles at the start of her turn, playing olly's first turn for him on a connection of his own if it comes first. The invitation is a row of the lobby, and Return on it opens its dialog, starting on Accept (`of_invitation`). The double's dialog starts on Accept (`of_double`). Esc puts it away, and the status line says "ann doubles to 2." with "Press Return to answer." (`of_away`). Return brings it back (`of_again`), and Y accepts (`of_taken`). The Resign button opens the resign dialog (`of_resign`), and N answers No. `vice/plans/offers_backgammon.txt` does the same on the C64, and `plans/gs_offers_checkers.txt` shows a draw offer on the IIgs (`offers.py 6501 checkers gwen "Swordfish 1997"`), whose dialog starts on Decline.

`plans/letters.txt` shows one meaning per letter, with Open-Apple, against the bot: the lobby's Open-Apple-N opens New game (`lt_newgame`); Mancala's 3 moves the pointer to pit 3 (`lt_mancala_3`); Open-Apple-H says it does nothing in Reversi (`lt_nothing`); Cannon Fire's < and + aim by 10, with the pointer on Fire (`lt_cannon`); Esc lets go of a picked dot in Dots and Boxes (`lt_dots_picked`, `lt_dots_dropped`) and goes back to the lobby when nothing is picked; Grid Words' Open-Apple-X starts an exchange (`lt_exchange`); Rummy Tile's Open-Apple-O sorts the rack and Open-Apple-C says whether the turn would count (`lt_sorted`, `lt_check`); Naval Combat's Open-Apple-G places the fleet at random (`lt_random`); and Backgammon's panel shows Roll and Double over Chat, Help, Resign and Lobby (`lt_backgammon`). `vice/plans/letters.txt` does the same on the C64.

`plans/sentences.txt` shows the sentence line: the status line says whose turn it is and the clock, and the message line says what Return does on the place under the pointer or the other player's move as it comes. It shows a Reversi table waiting for a player (`se_waiting`), Reversi against the bot on a square where no disc goes and on d3 (`se_reversi_a1`, `se_reversi_d3`), the bot's answer in words (`se_reversi_bot`), Five in a Row (`se_five`, `se_five_bot`), Mancala (`se_mancala`), Naval Combat while the fleet is placed (`se_naval`), Rummy Tile (`se_rummy`), Checkers (`se_checkers`) and Backgammon (`se_backgammon`). `vice/plans/sentences.txt` does the same on the C64.

`plans/help.txt` shows ? and Esc in every game: the lobby's help (`hp_lobby`), then each game against the bot, where ? opens page 1 of its help (`hp_NAME_1`), Right turns to page 2 (`hp_NAME_2`), Esc closes it (`hp_NAME_closed`), and Esc again, with nothing to cancel, goes back to the lobby (`hp_lobby_again`). `vice/plans/help.txt` does the same on the C64, with Space.

The first sign-in offers the tutorial in a dialog, so the plans that sign up a player and are about something else skip it: the IIe's hold `cfg_tutor` at 1 once the start screen shows (`hold cfg_tutor 1 30`, which `run.sh` finds in the map; one hold at a time, so it comes after run.sh's hold of `cfg_number`), the C64's set it (`set cfg_tutor 1`), and the IIgs's answer No (`type n`). `plans/tutorial.txt` plays the tutorial end to end. The first sign-in offers it in a dialog starting on Yes (`tu_offer`), and Y starts it. Each of the 18 steps is done as its instruction says (`tu_01` to `tu_17`): Return, the arrows to b2, Return, c1, Open-Apple-U, c1 again, /, time, a line cleared with Esc, the typed move /c3, Open-Apple-T, hello, Esc, /say good game, ? and Esc, Open-Apple-R and Esc, and c2, which wins; then Esc goes back to the lobby (`tu_lobby`). Signing out and in again does not offer it a second time (`tu_again`), and /tutorial in the lobby starts it (`tu_command`). `plans/gs_tutorial.txt` plays it on the IIgs, and `vice/plans/tutorial.txt` on the C64. `plans/tutorial_mono.txt` plays it in monochrome on the IIe, from the start screen without signing in, to show the marks drawn in single dots.

## Through a Unix shell

`lab/shell/` is a container with a login shell (bash 5.2, with TERM=vt100) on a pty behind a TCP port, as a
login over a serial line would be, with nc and python3. `plans/unixshell.txt` types `stty raw -echo; nc
host.docker.internal 16501; stty sane` in the console, presses Open-Apple-P, signs in, closes the connection
from the lobby and checks that it is back at the shell prompt:

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

P on the start screen dials the modem: the init string (ATZ as the settings start; nothing when it is empty), then
ATDT and the number, and on CONNECT the opening. These plans dial the WiFi-modem stand-in on port 16600:

```
A2ROMS=~/roms test/lab/run.sh 16600 build/parlornet.dsk test/lab/plans/dial.txt test/out/dial
```

- `plans/dial.txt` dials with the settings as they start (`dial_settings`): ATZ, OK, `ATDTparlornet.org:6501`
  (the stand-in connects any number to the lab parlor), CONNECT 9600 and the name prompt (`dial_dialed`).
  It signs up, closes the connection with Open-Apple-Q, which the stand-in ends with NO CARRIER when the
  parlor hangs up (so no `+++` in `modem.log`), and is back at the start screen (`dial_hungup`), where P
  dials again and signs in.
- `plans/dial_init.txt` changes the init string from ATZ to `AT&F`: P sends AT&F and dials, with no ATZ
  (`di_dialed`). The init string `X1`, which the stand-in answers with ERROR, stops the dial: "The modem says
  ERROR to X1." (`di_error`). An empty init string sends nothing before the dial (`di_none`). Then Pulse dial on
  (`di_pulse_settings`): P dials `ATDPparlornet.org:6501` (`di_pulse`). Last, the number
  `ATD"parlornet.org:6501"`, which starts with AT, is sent as the whole dial command even with Pulse dial on
  (`di_atd_settings`, `di_atd`), as a Zimodem needs for a plain connection.
- `plans/dial_errors.txt` dials BUSY (`de_busy`), and Return dials again; NOCARRIER (`de_nocarrier`); and
  SILENT, which ends after a minute in "The modem did not say CONNECT in a minute." (`de_timeout`). Return dials
  SILENT again (`de_dialing`), and Escape sends a Return, which stops the dial (the stand-in's NO CARRIER),
  and goes back to the start screen (`de_cancelled`).
- `plans/dial_stay.txt` dials STAY: after Open-Apple-Q no NO CARRIER comes, so the program sends `+++`, gets
  OK and sends ATH; `modem.log` shows "escape" and "hangup", and P dials again (`ds_again`).

Through a Unix shell the dialer needs no console step when ATZ (`tools/ATZ` in the ParlorNet repository) is
installed on the shell's PATH: the program's ATZ reaches the shell prompt, the shell echoes it and runs ATZ,
which answers OK. Install it in the shell container, then run the plans with the line on it:

```
docker exec -i -u root a2shell sh -c 'cat > /usr/local/bin/ATZ && chmod 755 /usr/local/bin/ATZ' < "$PARLORNET/tools/ATZ"
NET=a2lab LINE=a2shell:2323 A2ROMS=~/roms test/lab/run.sh 0 build/parlornet.dsk test/lab/plans/dial_atz.txt test/out/atz
```

- `plans/dial_atz.txt` sets the number to `host.docker.internal:16501` (`atz_settings`) and presses P: ATZ,
  OK from ATZ, the dial, CONNECT and the name prompt (`atz_dialed`). It signs up, closes the connection and
  dials again (`atz_again`): when the parlor hangs up, ATZ says NO CARRIER at once and waits for the next
  dial. Then the console shows ATZ answering AT (`atz_console`) and checks the Hayes escape: a dial, CONNECT,
  a second's quiet, `+++` and a second's quiet give OK with the line up (`atz_escape`), ATH hangs up with OK
  (`atz_ath`), and Control-C ends ATZ with exit status 130 (`atz_exit`).
- `plans/dial_atz_lost.txt` dials port 16602, a bridge that hangs up the line 75 seconds after it connects to
  the parlor, as when the parlor restarts. Start it beside the lab parlor (`pkill -f bridge.py` stops it, as
  it does the lab parlor's other bridges):

  ```
  nohup python3 test/lab/bridge.py 16602 127.0.0.1 16501 bridge test/work/srv/cut.log 75 > /dev/null 2>&1 &
  ```

  The player signs up and starts a game of Backgammon, the first in the list, against the bot (`al_game`).
  When the line goes, ATZ says NO CARRIER, and the game's next request finds it: "The connection to the
  parlor was lost." (`al_lost`). Return dials again and signs in, where the game is still listed
  (`al_redialed`, `al_lobby2`).
- `plans/console_close.txt`, in the same shell: going to the console closes the parlor connection first.
  Open-Apple-C from the lobby sends BYE, the parlor hangs up and ATZ says NO CARRIER, so AT in the console
  gets ATZ's OK (`cc_lobby_at`) and Control-C (the plan's `ctrl` step) gives the shell prompt
  (`cc_lobby_shell`). Open-Apple-C at the name prompt does the same (`cc_name_at`), and so does C on the
  start screen after Esc at the name prompt has left the connection open (`cc_start_at`, `cc_start_shell`).

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
tools/build.sh
GSROMS=~/roms GSHD=~/gs/marinetti.2mg PARLORNET=~/src/parlornet test/lab/gs.sh test/lab/plans/gs_reversi.txt test/out/gs
```

- `mame/gsrun.lua` is the IIgs step runner. Besides the IIe runner's steps it drives the ADB mouse on the
  desktop (`mto`, `click`, `dclick`, `drag`) and the ADB keys (`key cmd+q`), and finds the program's bank by
  its first bytes, so that `click ID` can point at a target as the arrow keys would.
- `plans/gs_reversi.txt` opens the program from the Finder, signs up, plays Reversi against the bot to the
  end, closes the connection and quits to the Finder. `plans/gs_splash.txt` shows the title screen with
  the display set to color, then to monochrome (where the IIgs shows the same title screen in black and
  white) and then not at all, each set in the settings and kept in PARLOR.CFG across a quit and a new
  start. `plans/gs_gridwords.txt` plays some words of Grid Words, in color and monochrome.
  `plans/gs_clock.txt` shows the time signed in as `plans/clock.txt` does on the IIe: no clock on the sign-in
  screen, "0m" in the lobby right after sign-in, none after signing out, and 0m again on the next sign-in and
  on a new connection.
- The IIgs has the Apple joystick in its game port too, and `gsrun.lua` has the IIe runner's joystick steps.
  `plans/gs_joystick.txt` turns the joystick on in the settings, moves in the lobby, chooses Reversi and the
  bot, places a disc, opens the chat with Open-Apple-T, and shows an empty port doing nothing.

## The C64 program in VICE

`vice/` plays the C64 program in VICE's x64sc (or x128) against a lab parlor, in the same way: plans of steps,
screenshots, and the orchestrator for the other side's moves. Everything runs in Docker on a network of its own
with no way out (`c64lab`, made `--internal`), every container with its capabilities dropped. You build the VICE
image yourself from VICE's upstream source tarball, which has the C64's, the C128's and the 1541's ROMs in it
(`vice/Dockerfile` says where to put the tarball and checks its SHA-256); no ROMs are part of this repository.

```
docker build -t parlornet-8bit-vice test/vice
PARLORNET=~/src/parlornet test/vice/parlor.sh
tools/build.sh
test/vice/run.sh build/c64/parlornet.d64 test/vice/plans/reversi.txt test/out/c64-reversi
```

- `vice/parlor.sh` starts the lab parlor as the container `c64-parlor`: parlord on port 16501 (listening on the lab
  network), the bot, and `lab/orch.py`, which answers the plans' `need` steps through `test/work/c64xchg`.
- `vice/run.sh` starts VICE under Xvfb, with its binary monitor on 127.0.0.1:6502 inside the container, and the
  serial line to `lab/bridge.py` in the same container: the WiFi-modem stand-in (`LINE=modem`, the default) or a
  plain relay (`LINE=direct`, with the dialer's number emptied after the title). `LINE=host:port` sends the line to
  another container on the lab network (a shell with ATZ, say). `IFACE=swift` (the default, at `BAUD`, 19200 at first) or `IFACE=user` picks a SwiftLink at $DE00 (at $DF00 with
  `ACIA=0xdf00`), as the program's settings start (SwiftLink compatible), or the user port's RS-232 (Supra 2400
  compatible).
  `ATTACH=1` only puts the disk in drive 8, for plans that type the LOAD themselves; `C128=1` runs x128, which boots
  the disk. `NTSC=1` runs an NTSC machine. `KEYMAP=1` runs VICE's positional keymap for the keys a plan presses
  (`KEYMAP=0`, the symbolic one, is VICE's default). VICE emulates the 1541 in full, at its own speed. VICE works on a copy
  of the disk, left in the output directory as `disk-after.d64`.
- `vice/run.py` is the step runner. It speaks VICE's binary monitor: it reads the text screen and the program's
  variables (by the linker's label file, `build/c64/parlor.lbl`), puts keys in the program's key queue as its
  keyboard scan does, moves the joystick in port 2 through VICE's simulated port, and takes screenshots. Its steps
  are those of `mame/run.lua` (wait, waittext, title, type, key, oa, arrows, click, point, snap, dumptext, hold,
  set, need, exit), plus `waittitle` (until the title screen is up, after the load), `basic` (typing into BASIC
  before the program runs), `joy`, `peek`, `bitmap` (until the bitmap is up, as the lobby is after a sign-in),
  `log`, `memfind` (where a text is in the program's memory, such as a password typed into its field), and
  `track` (sprite 2's place, read from the VIC each frame, with screenshots while it is on).
- The keys that `type`, `key`, `oa`, `arrows`, `click` and `title` put in the key queue never go through the
  program's keyboard scan, so a plan made of them alone cannot show a fault in the scan. `press` and `xtype` press
  real keys on VICE's window with xdotool, as a player does: `press` takes chords of X key names
  (`Shift_L+a`, `Shift_R+slash`, `Caps_Lock` for Shift Lock), and `xtype` types a text as on a US keyboard, with
  the left Shift for capitals and shifted symbols. VICE's symbolic keymap (its default) turns them into the C64's
  keys, and the scan reads them from the keyboard's matrix. VICE's simulated device in port 2 holds port A's lines
  5 to 7 low, which the scan would read as three columns always chosen, so `run.sh` plugs it in only for a plan
  with `joy` steps.

The plans in `vice/plans`:
- `stay_signed_in.txt` (with a parlor that has sign-in keys, and `lab/keys.py` beside it: `keys.py 16501 kim "Swordfish 1997"`): Stay signed in turned on in the settings (`ks_setting`); kim signs up with the password, which gets this computer's key (`ks_lobby`); the connection is closed from the console and P signs in again with the key, asking nothing (`ks_keyed`). keys.py then forgets kim's keys from a connection of its own, which ends the session (`ks_ended`); the next sign-in tries the key, the parlor refuses it with ERR NOKEY, and the password is asked for (`ks_nokey`), which signs in and gets a new key (`ks_password`). Signing out forgets the key, so the name is asked for again (`ks_signed_out`).
- `title.txt`, `start.txt`: the title screen after the load at a 1541's speed, the start screen and the settings.
- `load81.txt`, `load8.txt` (with `ATTACH=1`): `LOAD"*",8,1`, which starts the program with no RUN; and
  `LOAD"*",8` with RUN.
- `boot128.txt` (with `C128=1`): a C128 boots the disk, goes to 64 mode and starts the program.
- `console.txt`: the console through the modem stand-in at 2400 baud, AT and ATDT, then Commodore-P to the name
  prompt.
- `keys_shift.txt`: the Shift keys pressed for real. In the console, capitals, `?` and shifted symbols with the
  left Shift, `?` and a capital with the right Shift, and capitals with Shift Lock, echoed by the modem stand-in
  (`shift_console`); capitals and `?` at the name prompt (`shift_name`); a sign-up whose password is typed the same
  way (`shift_password`, where `memfind` finds the password in the field before Return), and a second sign-in
  with that password put in the key queue as text, which reaches the lobby (`shift_lobby2`) only if the field
  had it exactly. In VICE the right Shift holds both of the C64's Shift keys, and with both down Z and / are
  ghosts of each other in the matrix; the program takes / (? with Shift), as the KERNAL does, so in VICE a
  capital Z needs the left Shift.
- `dial.txt`: the dialer (ATZ, ATDT, CONNECT), signing up, closing the connection and dialing again.
- `dial_init.txt`: Pulse dial, which dials `ATDP`, and a number that starts with AT, sent as the whole dial
  command (`ATD"parlornet.org:6501"`).
- The settings' keys are numbered in the order of their lines: with the SwiftLink compatible modem 1 Modem, 2
  Address, 3 Speed; with the Supra 2400 compatible modem 1 Modem, 2 Speed.
- `swift.txt`: the SwiftLink compatible modem at 19200, as the settings start (`swift_settings`); key 2 going round
  its addresses, $DF00, $DF80 and $DE00 (`swift_addr_*`); key 3 going round the speeds (`swift_speed`); key 1 making the modem Supra 2400 compatible, at 2400 (`swift_to_supra`), and
  SwiftLink compatible again, back at 19200 (`swift_back`); then the console, the dialer, signing up and the
  lobby.
- `swift_df00.txt` (`ACIA=0xdf00`): the SwiftLink compatible modem at $DF00, set with key 2, through the console, the
  dialer, signing up and the lobby. VICE's C64 has the 6551 only at $DE00 or $DF00, so $DF80 is not run.
- `swift38.txt`: the SwiftLink compatible modem's 38400, set with key 3 and kept in PARLOR.CFG.
- `supra.txt` and `supra300.txt` (`IFACE=user`, with `BAUD=2400` and `BAUD=300`): the Supra 2400 compatible modem
  on the user port at 2400 and at 300, through the console, the dialer, signing up and the lobby.
- `progress.txt`: the progress bars, at the start, for the lobby (PN.PARLOR) and the sign-in when the console first
  opens, and for a game.
- `clock.txt`: the time signed in, as `plans/clock.txt` in `lab/` shows it on the Apple IIe: no clock on the
  sign-in screen, "0m" in the lobby right after sign-in, "1m" by the table's snapshots, none after signing
  out, and 0m again on the next sign-in and after the line is closed and dialed again.
- `reversi.txt`: a game of Reversi against the bot, with moves by the cursor keys, the orchestrator and the joystick,
  and the board in monochrome.
- `table_entry.txt`: the first view of each of the ten games, as `plans/table_entry.txt` does for the Apple II, by
  the same target ids.
- `cannonfire_incoming.txt` (`THINK=0.5,2` for `parlor.sh`): the bot's shot flying in after the player's, as
  `plans/cannonfire_incoming.txt` shows on the Apple II.
- `cannonfire_path.txt`: the shell against its path. The player fires five shots, high and low, to the right
  and to the left, and one nearly straight up that leaves the top of the screen, and the bot answers each from
  the other side; `track` follows the shell's sprite through
  each pair (`cfp_N.csv`, with a screenshot every 12 frames, `cfp_N_NNN.png`). Then `vice/cftrack.py OUTDIR`
  works out from the parlor's lines in `modem.log` where the program should put the shell at each step of each
  shot, as `draw/cannonfire.c` does, and checks every place the VIC showed it in: each flight gets a line, and a
  drawing of the shot's steps and the sprite's places (`cfp_N_F_path.png`). It exits with 1 when the shell
  was more than a pixel off its path.
- `end_GAME.txt`, one for each of the ten games: signed up, a new game against the bot from the lobby, and the
  orchestrator's moves (`GAME+`) until the game is over, with the first view (`end_GAME_start`) and the end
  (`end_GAME_over`). In `GAME+` the orchestrator also takes a snapshot on its twelfth turn (`mid.png`), and when it
  would make the same move a fifth time in a row on a table whose view has not moved on, which means the program
  did not take it, it shows the screen (`stuck.png`, `stuck.txt`) and stops on the tenth. Each plan peeks
  `net_lost` and `net_again` before its end shot. On the user port (`IFACE=user`), VICE now and then (about once in a
  thousand replies at `FAST` speed, most often while the program polls after a game) loses the first byte of a
  reply: no start bit reaches the program, and the reply has no final line. The same polling over the
  SwiftLink (`IFACE=swift`) loses nothing. The program sends such a request again (`net_again` counts them),
  so the game goes on. Run them with `FAST=1`, which runs VICE at full speed.
