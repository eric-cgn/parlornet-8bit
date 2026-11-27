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
