# Backlog

## The Apple II and II+

The program runs on the Apple IIe with 128K. Most of it would run on a II+ (or an original Apple II) as it is: the screens use the program's own font, and the Super Serial Card, the Uthernet II and the AppleMouse II all fit those machines. These are the parts that would have to change.

- **Drawing.** The IIe program draws in Double Hi-Res, which needs the IIe's auxiliary 64K. A II+ has neither, so it would need drawing of its own in Hi-Res, as release 2.0.7 had.

- **Moving the pointer.** The II+ keyboard has only the left and right arrows. The pointer, the lobby and the menus need four directions, so a II+ mode would add I, J, K and M (or another set chosen on the start screen) for up, left, right and down.
- **The Open-Apple shortcuts.** The II+ has no Open-Apple key. The program reads Open-Apple at $C061, which on a II+ is joystick button 0, so a joystick's button already works. Without a joystick, Esc followed by the letter would stand for Open-Apple and the letter. Esc today goes back to the lobby, so that move would need another key in II+ mode.
- **Capitals only.** The II+ keyboard types only capitals. Chat goes out in capitals, and a password with lower-case letters cannot be typed. A new password can still meet the parlor's rules with capitals, digits and symbols, and the guide would say so. There is no Delete key; the left arrow would rub out instead.
- **The line console.** The console uses the text screen, and the II+ character generator has no lower case. In II+ mode the console would show lower case as capitals.
- **Timing.** The program waits for the vertical blank by reading $C019 (src/sys.s), which only the IIe has. On a II+ that read returns whatever is on the bus, so II+ mode would use a counted delay instead.
- **Memory.** The games are overlays loaded at $D000 in language card RAM, and the program's interrupt vector lives there too. A II+ needs a 16K language card (Apple's, or any 16K RAM card in slot 0), which makes 64K. A 48K machine cannot run the program.
- **Booting.** The disk needs a 16-sector Disk II controller (the P5A boot ROM that came with DOS 3.3). An original Apple II without the Autostart ROM boots it from the monitor with `6` then Ctrl-P.
- **Telling the machines apart.** The program would check the ROM's machine ID byte at $FBB3 when it starts and set II+ mode by itself, with a setting to override it.

The ProDOS version (PARLOR.SYSTEM) stays IIe-only, since it keeps its games in the auxiliary 64K.

Testing would be in MAME's apple2p machine with a language card, the same lab scripts and the same cards.

## Games in the IIe's auxiliary 64K

On the IIe a game overlay has 12,032 bytes in the language card, since paged replies let the reply buffer shrink and a table's moves and chat moved to low memory. Running the games from the auxiliary 64K instead would give each one most of a 32K space. The switch between the two halves costs only a few cycles, so the work is in what the games share with the main program. This plan is for when a game outgrows 10K. It waits until the C64 and the IIgs program have an answer too, since the games share their rules and the room matters only if every machine can use it.

In 2.0.8 Cannon Fire, Backgammon and Grid Words filled 92 to 98 percent of the overlay space on all three machines. With paged replies the overlays have 12,032 bytes on the IIe and the IIgs and 11,232 on the C64, so the fullest of them, the C64's Cannon Fire, has 1,932 bytes left and the IIgs program's Backgammon 1,795. The rules those games share are 1.9K, 4.5K and 5.7K, and the rest of each overlay is the machine's own drawing.

On the C64 the resident program is 34K and ends where the overlays begin. Making the lobby an overlay of its own would free about 6K for the games, at the cost of a disk load each time a player goes back to the lobby. Part of the 4K of RAM under the I/O at $D000 holds only sprite shapes and could hold data. On the IIgs the games would need 65816 code to run in another bank, with long calls and the data bank set, and memory from the GS/OS Memory Manager. A IIgs also runs the IIe disk, which would get the IIe plan as it is.

Every game would run this way, the small ones too, so that there is one mechanism and the code stays simple.

The IIe build of 2.0.8 shows what the games use from the main program.

| Game | Runtime and libc routines | Our functions | Our variables |
|---|---|---|---|
| Reversi | 19 | 12 | 9 |
| Mancala | 15 | 16 | 10 |
| Five in a Row | 21 | 12 | 8 |
| Checkers | 24 | 19 | 15 |
| Dots and Boxes | 26 | 17 | 11 |
| Naval Combat | 26 | 14 | 7 |
| Rummy Tile | 33 | 19 | 11 |
| Tutorial | 32 | 21 | 9 |
| Grid Words | 40 | 26 | 16 |
| Cannon Fire | 43 | 16 | 8 |
| Backgammon | 47 | 27 | 33 |

Together the games use 93 runtime and libc symbols, 47 of our functions and 45 of our variables. These are the parts of the plan.

- **The runtime.** The games use 75 of cc65's runtime and libc modules, about 2K of code. A second copy is linked into the auxiliary 64K.
- **Drawing.** The Double Hi-Res kit already runs in the auxiliary 64K, so games call it directly with no switch.
- **The other functions.** About 25 calls for the table, the moves, the clock and the keyboard go through a jump table in the language card, which both halves can see. One shared routine switches the halves, calls the function and switches back.
- **Variables.** The table's game state, the parsed reply words and the reply line move to memory that doesn't switch, which is the language card or zero page. The kit's parameter page at $0200 switches with the halves, so it moves too.
- **Pointers.** A pointer passed from a game to the main program, such as the string in a call to draw text, points into the wrong half. Such calls copy their data into a shared buffer first. The reply words point the other way and are covered by moving the reply line.
- **The C stack.** cc65 passes arguments on its own stack. Either the jump table copies each function's arguments between two stacks, or the C stack moves into the language card for about 1K.

Until then, a big game can keep its art and tables in the auxiliary 64K, as Backgammon does, or be split into two overlays.
