-- gsrun.lua: plan runner for MAME's Apple IIgs (apple2gs), one step per line in the plan file
-- (GSPLAN env, default /scripts/plan.txt). Frames are 1/60 s of emulated time. Output goes to /out.
--   wait N               N frames
--   snap NAME            /out/NAME.png (MAME's native 704x231 frame; GS pixel (x,y) is at (x+32, y+16))
--   mto X Y              move the pointer to GS screen coordinates (0..639, 0..199), closed loop
--   mrel DX DY           move by raw ADB counts (about 0.6 pixel each)
--   mhome                pin the pointer to the top-left corner
--   mdown / mup          mouse button
--   click [X Y]          (move there and) click
--   dclick [X Y]         (move there and) double-click
--   drag X Y             button down here, move to X Y, button up (menus: mto title, drag item)
--   key K[+K...]         press keys together (ADB key names or aliases: cmd opt shift ctrl ret esc tab
--                        del space up down left right, or a single character such as q)
--   type TEXT            type text through MAME's natural keyboard (\r is Return)
--   pos                  log the pointer position
--   setport TAG FIELD V  set an input field (e.g. :modem:null_modem:RS232_TXBAUD Baud 13)
--   ports                write every input field and its settings to /out/ports.txt
--   log TEXT             write TEXT to the log
--   exit
-- and for ParlorNet (test/lab/gs.sh; /xchg/addr.txt has the program's first bytes and its targets):
--   waittext TEXT [N]    until the 40-column text screen (bank $E0) shows TEXT, at most N frames
--   title [N]            the title screen (Hi-Res, not Super Hi-Res), which waits for a key: Return each
--                        second while it is up, until it is gone; at most N frames
--   dumptext NAME        the text screen into /out/NAME.txt
--   click ID             make the pointer's target ID current, as the arrow keys would, and press Return
--   point ID             make it current without a key
--   arrows RLUD...       arrow keys, one after another
--   oa C                 Open-Apple and the key C
--   need WHAT            ask the orchestrator (orch.py) for the steps of a move, and run them next
--   findpn               log the bank the program runs in
--   dump HEX N NAME      N bytes from address HEX into /out/NAME.bin; dumppn: the address in the program's bank
--   dbg COMMAND          a MAME debugger command (when MAME runs with -debug)
--   joy BITS [N]         the Apple joystick held (bits: 1 up, 2 down, 4 left, 8 right, 16 button 0,
--                        32 button 1) for N frames (default 10), then let go, and 10 frames more
--   stick [X Y], button B V, nostick V, where: as in run.lua
local plan_path = os.getenv("GSPLAN") or "/scripts/plan.txt"
local steps = {}
for line in io.lines(plan_path) do
  line = line:gsub("%s+$", "")
  if line:match("%S") and not line:match("^%s*#") then steps[#steps + 1] = line end
end
local log = io.open("/out/run.log", "w")
local function L(s) log:write(s .. "\n"); log:flush() end
local space = manager.machine.devices[":maincpu"].spaces["program"]
local ports = manager.machine.ioport.ports

-- The pointer position as the GS firmware keeps it (bank E1: x word at $00E9, y word at $00EB).
local function rd16(a) return space:read_u8(a) | (space:read_u8(a + 1) << 8) end
local function getpos()
  local x, y = rd16(0xE100E9), rd16(0xE100EB)
  if x >= 0x8000 then x = x - 0x10000 end
  if y >= 0x8000 then y = y - 0x10000 end
  return x, y
end

local function field(tag, name) local p = ports[tag]; return p and p.fields[name] end
local fx = field(":macadb:MOUSE1", "Mouse X")
local fy = field(":macadb:MOUSE2", "Mouse Y")
local fb = field(":macadb:MOUSE0", "Mouse Button 0")
local MX, MY = 0, 0              -- the ADB mouse counters we feed (8-bit, wrap)

-- Keys: every ADB key field by its first label, plus aliases.
local keys = {}
for tag, p in pairs(ports) do
  if tag:match("^:macadb:KEY") then
    for name, f in pairs(p.fields) do
      keys[name] = f
      local first = name:match("^(%S+)%s%s")
      if first then keys[first] = f end
    end
  end
end
local alias = { cmd = "Command / Open Apple", opt = "Option / Solid Apple", shift = "Shift", ctrl = "Control",
  ret = "Return", esc = "Esc", tab = "Tab", del = "Backspace", space = "Space",
  up = "Up Arrow", down = "Down Arrow", left = "Left Arrow", right = "Right Arrow" }
local function keyfield(k) return keys[alias[k] or k] or keys[k:lower()] end

-- ParlorNet additions: the 40-column text page in bank E0, the program's bank, its targets.
local rowbase = {}
for r = 0, 23 do rowbase[r] = 0xE00400 + (r % 8) * 0x80 + math.floor(r / 8) * 0x28 end
local function screen_text()
  local out = {}
  for r = 0, 23 do
    local t = {}
    for c = 0, 39 do
      local b = space:read_u8(rowbase[r] + c) & 0x7F
      if b < 0x20 then b = b + 0x40 end
      t[#t + 1] = string.char(b)
    end
    out[#out + 1] = table.concat(t)
  end
  return table.concat(out, "\n")
end
ADDR = {}
do
  local af = io.open("/xchg/addr.txt", "r")
  if af then
    for line in af:lines() do
      local name, hex = line:match("^(%S+)%s+(%x+)$")
      if name then ADDR[name] = tonumber(hex, 16) end
    end
    af:close()
  end
end
local PNBANK = nil
local function findpn()
  local sig = ADDR.sig
  if not sig then return nil end
  for bank = 2, 0x7F do
    local a = bank * 0x10000 + 0x4000
    local ok = true
    for k = 0, 3 do
      if space:read_u8(a + k) ~= ((sig >> (8 * (3 - k))) & 0xFF) then ok = false; break end
    end
    if ok then return bank end
  end
  return nil
end
local deadline = nil
local joy = dofile("/scripts/joy.lua")    -- the Apple joystick (gs.sh gives MAME -gameio joy)

local i, wait, frame = 1, 0, 0
local task = nil                 -- a coroutine-like table {fn=function() return done end}
local held = {}

local function start_move(tx, ty, timeout)
  local settle, t0, phase = 0, frame, 0
  return function()
    phase = phase + 1
    if phase % 2 == 1 then return false end -- move every second frame, let the ADB poll catch up
    local x, y = getpos()
    local ex, ey = tx - x, ty - y
    if ex == 0 and ey == 0 then
      settle = settle + 1
      if settle >= 4 then return true end
      return false
    end
    settle = 0
    local function step(e)
      if e == 0 then return 0 end
      local s = math.floor(math.abs(e) * 1.2 + 0.5)
      if s < 1 then s = 1 end
      if s > 12 then s = 12 end
      return e > 0 and s or -s
    end
    MX = MX + step(ex); MY = MY + step(ey)
    if frame - t0 > (timeout or 900) then L(frame .. " mto TIMEOUT at " .. x .. "," .. y); return true end
    return false
  end
end

local function seq(list) -- run a list of task functions one after another
  local k = 1
  return function()
    while list[k] do
      if not list[k]() then return false end
      k = k + 1
    end
    return true
  end
end
local function delay(n) local c = 0; return function() c = c + 1; return c >= n end end
local function act(fn) return function() fn(); return true end end
local function button(v) return act(function() if v then fb:set_value(1) else fb:clear_value() end end) end

local function dump_ports()
  local f = io.open("/out/ports.txt", "w")
  for tag, p in pairs(ports) do
    for name, fl in pairs(p.fields) do
      f:write(string.format("%s\t%s\tdef=%d\n", tag, name, fl.defvalue))
      local ok, st = pcall(function() return fl.settings end)
      if ok and st then
        for v, n in pairs(st) do f:write(string.format("\t\t%d = %s\n", v, n)) end
      end
    end
  end
  f:close()
end

sub = emu.add_machine_frame_notifier(function()
  frame = frame + 1
  if task then
    if task() then task = nil end
  elseif wait > 0 then
    wait = wait - 1
  else
    local s = steps[i]
    if s then
      i = i + 1
      local cmd, rest = s:match("^(%S+)%s*(.*)$")
      local a, b = rest:match("^(%-?%d+)%s+(%-?%d+)")
      a, b = tonumber(a), tonumber(b)
      if cmd == "wait" then wait = tonumber(rest)
      elseif cmd == "snap" then
        manager.machine.screens[":screen"]:snapshot("/out/" .. rest .. ".png"); L(frame .. " snap " .. rest)
      elseif cmd == "mto" then task = start_move(a, b)
      elseif cmd == "mrel" then MX = MX + a; MY = MY + b; wait = 6
      elseif cmd == "mhome" then
        local n = 0
        task = function() n = n + 1; MX = MX - 8; MY = MY - 8; return n >= 160 end
      elseif cmd == "mdown" then fb:set_value(1); wait = 6
      elseif cmd == "mup" then fb:clear_value(); wait = 6
      elseif cmd == "click" and rest:match("^%d+$") then
        a = tonumber(rest)
        PNBANK = PNBANK or findpn()
        local base = (PNBANK or 0) * 0x10000
        local n = space:read_u8(base + ADDR.t_n)
        local found = nil
        for k = 0, n - 1 do if space:read_u8(base + ADDR.t_id + k) == a then found = k end end
        if found then
          space:write_u8(base + ADDR.t_cur, found)
          local f = keyfield("ret")
          task = seq({ act(function() f:set_value(1) end), delay(4), act(function() f:clear_value() end), delay(10) })
          L(frame .. " click " .. a)
        else L(frame .. " NO TARGET " .. a) end
      elseif cmd == "need" then
        -- ask the lab's orchestrator for steps (xchg/need), then run the lines of xchg/steps.txt next
        local asked = frame
        local fo = io.open("/xchg/need", "w"); fo:write(rest); fo:close()
        task = function()
          local f = io.open("/xchg/steps.txt", "r")
          if f then
            local new = {}
            for line in f:lines() do if line:match("%S") then new[#new + 1] = line end end
            f:close(); os.remove("/xchg/steps.txt")
            L(frame .. " steps for " .. rest .. ": " .. #new)
            for k = #new, 1, -1 do table.insert(steps, i, new[k]) end
            return true
          end
          if frame - asked > 3600 then L(frame .. " TIMEOUT need " .. rest); return true end
          return false
        end
      elseif cmd == "arrows" then
        local names = { R = "right", L = "left", U = "up", D = "down" }
        local l = {}
        for c in rest:gmatch(".") do
          local f = keyfield(names[c])
          l[#l + 1] = act(function() f:set_value(1) end); l[#l + 1] = delay(4)
          l[#l + 1] = act(function() f:clear_value() end); l[#l + 1] = delay(10)
        end
        task = seq(l)
      elseif cmd == "oa" then
        local f1, f2 = keyfield("cmd"), keyfield(rest:lower())
        task = seq({ act(function() f1:set_value(1) end), delay(3), act(function() f2:set_value(1) end), delay(6),
          act(function() f2:clear_value() end), delay(3), act(function() f1:clear_value() end), delay(20) })
      elseif cmd == "caps" then
        -- (the IIgs types capitals with Shift)
      elseif cmd == "click" or cmd == "dclick" then
        local l = {}
        if a then l[#l + 1] = start_move(a, b) end
        local n = (cmd == "dclick") and 2 or 1
        for k = 1, n do
          l[#l + 1] = button(true); l[#l + 1] = delay(5); l[#l + 1] = button(false); l[#l + 1] = delay(6)
        end
        l[#l + 1] = delay(10)
        task = seq(l)
      elseif cmd == "drag" then
        task = seq({ button(true), delay(8), start_move(a, b), delay(8), button(false), delay(10) })
      elseif cmd == "key" then
        local fs = {}
        for k in rest:gmatch("[^+]+") do
          local f = keyfield(k)
          if f then fs[#fs + 1] = f else L("no key " .. k) end
        end
        local l = {}
        for _, f in ipairs(fs) do l[#l + 1] = act(function() f:set_value(1) end); l[#l + 1] = delay(2) end
        l[#l + 1] = delay(6)
        for k = #fs, 1, -1 do local f = fs[k]; l[#l + 1] = act(function() f:clear_value() end) end
        l[#l + 1] = delay(8)
        task = seq(l)
      elseif cmd == "type" then
        manager.machine.natkeyboard:post_coded((rest:gsub("\\r", "\r")))
        local n = 0
        task = function() n = n + 1; return n > 10 and manager.machine.natkeyboard.empty end
      elseif cmd == "pos" then local x, y = getpos(); L(frame .. " pos " .. x .. " " .. y)
      elseif cmd == "setport" then
        local tag, name, v = rest:match("^(%S+)%s+(.-)%s+(%d+)$")
        local f = field(tag, name)
        if f then f.user_value = tonumber(v); L("set " .. tag .. " " .. name .. " " .. v)
        else L("no field " .. rest) end
      elseif cmd == "ports" then dump_ports()
      elseif cmd == "log" then L(frame .. " " .. rest)
      elseif cmd == "dbg" then
        local d = manager.machine.debugger
        if d then d:command(rest); L(frame .. " dbg " .. rest) else L("no debugger") end
      elseif cmd == "waittext" then
        local text, n = rest:match("^(.-)%s+(%d+)$")
        if not text then text, n = rest, 3600 end
        local t0 = frame
        task = function()
          if screen_text():find(text, 1, true) then L(frame .. " found " .. text); return true end
          if frame - t0 > tonumber(n) then L(frame .. " TIMEOUT waiting for " .. text); return true end
          return false
        end
      elseif cmd == "title" then
        local t0, n, pressed = frame, tonumber(rest) or 1800, nil
        task = function()
          local up = space:read_u8(0xE0C01A) < 0x80 and space:read_u8(0xE0C01D) >= 0x80
              and space:read_u8(0xE0C029) < 0x80
          if up and (not pressed or frame - pressed >= 60) then
            if not pressed then L(frame .. " title") end
            manager.machine.natkeyboard:post("\r"); pressed = frame
            return false
          end
          if not up and pressed then L(frame .. " title gone"); return true end
          if frame - t0 > n then L(frame .. (pressed and " title stays" or " no title")); return true end
          return false
        end
      elseif cmd == "dumptext" then
        local f = io.open("/out/" .. rest .. ".txt", "w"); f:write(screen_text()); f:close()
      elseif cmd == "dump" then
        local ad, n, name = rest:match("^(%x+)%s+(%d+)%s+(%S+)$")
        ad = tonumber(ad, 16)
        local f = io.open("/out/" .. name .. ".bin", "wb")
        for k = 0, tonumber(n) - 1 do f:write(string.char(space:read_u8(ad + k))) end
        f:close(); L(frame .. " dump " .. name)
      elseif cmd == "findpn" then
        PNBANK = findpn(); L(frame .. " program bank " .. tostring(PNBANK))
      elseif cmd == "dumppn" then
        -- dumppn HEXADDR LEN NAME: memory in the program's bank
        PNBANK = PNBANK or findpn()
        local ad, n, name = rest:match("^(%x+)%s+(%d+)%s+(%S+)$")
        ad = (PNBANK or 0) * 0x10000 + tonumber(ad, 16)
        local f = io.open("/out/" .. name .. ".bin", "wb")
        for k = 0, tonumber(n) - 1 do f:write(string.char(space:read_u8(ad + k))) end
        f:close(); L(frame .. " dumppn " .. name)
      elseif cmd == "target" or cmd == "point" then
        -- make the target with this id current (as the arrow keys would); target also presses Return
        PNBANK = PNBANK or findpn()
        local base = (PNBANK or 0) * 0x10000
        local id = tonumber(rest)
        local n = space:read_u8(base + ADDR.t_n)
        local found = nil
        for k = 0, n - 1 do if space:read_u8(base + ADDR.t_id + k) == id then found = k end end
        if found then
          space:write_u8(base + ADDR.t_cur, found)
          if cmd == "target" then
            local f = keyfield("ret")
            task = seq({ act(function() f:set_value(1) end), delay(4), act(function() f:clear_value() end), delay(8) })
          end
          L(frame .. " " .. cmd .. " " .. id)
        else L(frame .. " NO TARGET " .. id) end
      elseif cmd == "joy" then
        local bits, n = rest:match("^(%d+)%s*(%d*)$")
        L(frame .. " joy " .. rest)
        task = seq({ act(function() joy.bits(tonumber(bits)) end), delay(tonumber(n) or 10), act(joy.release),
          delay(10) })
      elseif cmd == "stick" then joy.stick(a, b)
      elseif cmd == "button" then joy.button(a, b)
      elseif cmd == "nostick" then joy.nostick(space, rest == "1", 0xE0C064)
      elseif cmd == "where" then
        PNBANK = PNBANK or findpn()
        local base = (PNBANK or 0) * 0x10000
        local c, n = space:read_u8(base + ADDR.t_cur), space:read_u8(base + ADDR.t_n)
        L(frame .. " where " .. (c < n and tostring(space:read_u8(base + ADDR.t_id + c)) or "none"))
      elseif cmd == "exit" then L(frame .. " exit"); log:close(); manager.machine:exit()
      else L("unknown step " .. s) end
    end
  end
  fx:set_value(MX & 0xff); fy:set_value(MY & 0xff)
end)
