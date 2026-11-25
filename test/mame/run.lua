-- A small test runner for MAME: reads /scripts/plan.txt, one step per line.
--   wait N             N frames (60 a second)
--   waittext TEXT [N]  until the 40-column text screen shows TEXT (at most N frames, default 1800)
--   title [N]          the title screen (Hi-Res, at start-up), which waits for a key: Return each second
--                      while it is up, until it is gone; at most N frames (default 1800)
--   type TEXT          type it (post_coded: {ENTER}, {LEFT}, {UP} ... and \r for Return)
--   oa C               Open-Apple and the key C
--   ctrl C             Control and the key C
--   snap NAME          save the screen as /out/NAME.png
--   set TAG FIELD V    set an input field's value (DIP switches, configs)
--   hold NAME V N      keep the byte at the program's variable NAME (from the link map) at V for the
--                      next N frames, while the steps go on (the program's settings before it draws)
--   dumptext NAME      write the text screen to /out/NAME.txt
--   exit
-- Addresses of the pointer's targets, from the link map (/xchg/addr.txt: name hex per line).
-- The Uthernet II test double, when the lab asks for it.
do
  local m = io.open("/xchg/use_w5100", "r")
  if m then m:close(); dofile("/scripts/w5100.lua") end
end
ADDR = {}
local af = io.open("/xchg/addr.txt", "r")
if af then
  for line in af:lines() do
    local name, hex = line:match("^(%S+)%s+(%x+)$")
    if name then ADDR[name] = tonumber(hex, 16) end
  end
  af:close()
end
local steps = {}
for line in io.lines("/scripts/plan.txt") do
  if line:match("%S") and not line:match("^#") then table.insert(steps, line) end
end
local i, waiting, deadline, release_oa = 1, 0, nil, nil
local hold_addr, hold_v, hold_until = nil, 0, 0
local log = io.open("/out/run.log", "w")
local space = manager.machine.devices[":maincpu"].spaces["program"]
local rowbase = {}
for r = 0, 23 do rowbase[r] = 0x400 + (r % 8) * 0x80 + math.floor(r / 8) * 0x28 end

local function screen_text()
  local out = {}
  for r = 0, 23 do
    local s = {}
    for c = 0, 39 do
      local b = space:read_u8(rowbase[r] + c) & 0x7F
      if b < 0x20 then b = b + 0x40 end
      s[#s + 1] = string.char(b)
    end
    out[#out + 1] = table.concat(s)
  end
  return table.concat(out, "\n")
end

local function field(tag, name)
  local port = manager.machine.ioport.ports[tag]
  return port and port.fields[name]
end

local frame = 0
sub = emu.add_machine_frame_notifier(function()
  frame = frame + 1
  if hold_addr and frame < hold_until then space:write_u8(hold_addr, hold_v) end
  if release_oa and frame >= release_oa then
    field(":keyb_special", "Open Apple"):clear_value()
    release_oa = nil
  end
  if release_ctrl and frame >= release_ctrl then
    field(":keyb_special", "Control"):clear_value()
    release_ctrl = nil
  end
  if release_caps and frame >= release_caps then
    field(":keyb_special", "Caps Lock"):clear_value()
    release_caps = nil
  end
  if waiting > 0 then waiting = waiting - 1 return end
  local step = steps[i]
  if not step then return end
  local cmd, rest = step:match("^(%S+)%s*(.*)$")
  if cmd == "wait" then
    waiting = tonumber(rest); i = i + 1
  elseif cmd == "waittext" then
    local text, n = rest:match("^(.-)%s+(%d+)$")
    if not text then text, n = rest, 1800 end
    if not deadline then deadline = frame + tonumber(n) end
    if screen_text():find(text, 1, true) then
      log:write(frame .. " found " .. text .. "\n"); deadline = nil; i = i + 1
    elseif frame > deadline then
      log:write(frame .. " TIMEOUT waiting for " .. text .. "\n"); deadline = nil; i = i + 1
    end
  elseif cmd == "title" then
    -- the title screen: graphics ($C01A bit 7 clear) in Hi-Res ($C01D bit 7 set). Return every second
    -- while it is up (one pressed while the program still loads is not taken), until text mode is back.
    if not deadline then deadline = frame + (tonumber(rest) or 1800); title_key = nil end
    local up = space:read_u8(0xC01A) < 0x80 and space:read_u8(0xC01D) >= 0x80
    if up and (not title_key or frame - title_key >= 60) then
      if not title_key then log:write(frame .. " title\n") end
      manager.machine.natkeyboard:post("\r"); title_key = frame
    elseif not up and title_key then
      log:write(frame .. " title gone\n"); deadline = nil; i = i + 1
    elseif frame > deadline then
      log:write(frame .. (title_key and " title stays\n" or " no title\n")); deadline = nil; i = i + 1
    end
  elseif cmd == "type" then
    manager.machine.natkeyboard:post_coded((rest:gsub("\\r", "\r"))); i = i + 1
  elseif cmd == "oa" then
    field(":keyb_special", "Open Apple"):set_value(1)
    manager.machine.natkeyboard:post_coded(rest)
    release_oa = frame + 20; waiting = 25; i = i + 1
  elseif cmd == "ctrl" then
    field(":keyb_special", "Control"):set_value(1)
    manager.machine.natkeyboard:post_coded(rest)
    release_ctrl = frame + 20; waiting = 25; i = i + 1
  elseif cmd == "need" then
    -- ask the lab's orchestrator for steps: write /xchg/need, wait for /xchg/steps.txt, run its lines next
    if not need_asked then
      local f = io.open("/xchg/need", "w"); f:write(rest); f:close(); need_asked = frame
    end
    local f = io.open("/xchg/steps.txt", "r")
    if f then
      local new = {}
      for line in f:lines() do if line:match("%S") then table.insert(new, line) end end
      f:close(); os.remove("/xchg/steps.txt")
      log:write(frame .. " steps for " .. rest .. ": " .. #new .. "\n")
      for k = #new, 1, -1 do table.insert(steps, i + 1, new[k]) end
      need_asked = nil; i = i + 1
    elseif frame - need_asked > 3600 then
      log:write(frame .. " TIMEOUT need " .. rest .. "\n"); need_asked = nil; i = i + 1
    end
  elseif cmd == "click" then
    -- point at the target whose id is given (as the arrow keys would) and press Return
    local id = tonumber(rest)
    local n = space:read_u8(ADDR.t_n)
    local found = nil
    for k = 0, n - 1 do if space:read_u8(ADDR.t_id + k) == id then found = k end end
    if found then
      space:write_u8(ADDR.t_cur, found)
      manager.machine.natkeyboard:post("\r")
      log:write(frame .. " click " .. id .. "\n")
    else
      log:write(frame .. " NO TARGET " .. id .. "\n")
    end
    waiting = 10; i = i + 1
  elseif cmd == "point" then
    -- make the target with this id the current one, without a key (an arrow key then moves on from it)
    local id = tonumber(rest)
    local n = space:read_u8(ADDR.t_n)
    for k = 0, n - 1 do if space:read_u8(ADDR.t_id + k) == id then space:write_u8(ADDR.t_cur, k) end end
    i = i + 1
  elseif cmd == "arrows" then
    -- e.g. "RRRDD": each arrow key pressed for 4 frames, 12 frames apart
    local map = {R = {":X7", "→"}, L = {":X7", "←"}, D = {":X7", "↓"}, U = {":X6", "↑"}}
    if not arrow_q then arrow_q = rest; arrow_t = 0 end
    if arrow_t == 0 then
      local c = arrow_q:sub(1, 1)
      if c == "" then arrow_q = nil; i = i + 1; return end
      arrow_f = field(map[c][1], map[c][2]); arrow_f:set_value(1); arrow_t = 1
    elseif arrow_t == 4 then
      arrow_f:clear_value(); arrow_t = arrow_t + 1
    elseif arrow_t >= 12 then
      arrow_q = arrow_q:sub(2); arrow_t = 0; return
    else
      arrow_t = arrow_t + 1
    end
    if arrow_t > 1 then arrow_t = arrow_t end
  elseif cmd == "key" then
    -- the IIe's own codes: left 8, right 21, up 11, down 10, return 13, escape 27, delete 127
    local codes = {left = 8, right = 21, up = 11, down = 10, ret = 13, esc = 27, del = 127, tab = 9}
    local s = {}
    for word in rest:gmatch("%S+") do
      local n, name = word:match("^(%d*)(%a+)$")
      for k = 1, (tonumber(n) or 1) do s[#s + 1] = string.char(codes[name]) end
    end
    manager.machine.natkeyboard:post(table.concat(s)); i = i + 1
  elseif cmd == "caps" then
    field(":keyb_special", "Caps Lock"):set_value(1)
    release_caps = frame + 10; waiting = 15; i = i + 1
  elseif cmd == "oaraw" then
    -- Open-Apple held while the next steps type (released by oaup)
    field(":keyb_special", "Open Apple"):set_value(1); i = i + 1
  elseif cmd == "oaup" then
    field(":keyb_special", "Open Apple"):clear_value(); i = i + 1
  elseif cmd == "snap" then
    manager.machine.screens[":screen"]:snapshot("/out/" .. rest .. ".png")
    log:write(frame .. " snap " .. rest .. "\n"); i = i + 1
  elseif cmd == "dumpmem" then
    local a, n, name = rest:match("^(%x+)%s+(%d+)%s+(%S+)$")
    a = tonumber(a, 16)
    local f = io.open("/out/" .. name .. ".bin", "wb")
    for k = 0, tonumber(n) - 1 do f:write(string.char(space:read_u8(a + k))) end
    f:close(); i = i + 1
  elseif cmd == "dumptext" then
    local f = io.open("/out/" .. rest .. ".txt", "w"); f:write(screen_text()); f:close(); i = i + 1
  elseif cmd == "set" then
    local tag, name, v = rest:match("^(%S+)%s+(.-)%s+(%d+)$")
    local f = field(tag, name)
    if f then f.user_value = tonumber(v) else log:write("no field " .. rest .. "\n") end
    i = i + 1
  elseif cmd == "hold" then
    local name, v, n = rest:match("^(%S+)%s+(%d+)%s+(%d+)$")
    hold_addr, hold_v, hold_until = ADDR[name], tonumber(v), frame + tonumber(n)
    if not hold_addr then log:write("no address for " .. name .. "\n") end
    i = i + 1
  elseif cmd == "exit" then
    log:write(frame .. " exit\n"); log:close()
    manager.machine:exit()
    i = i + 1
  else
    log:write("unknown step " .. step .. "\n"); i = i + 1
  end
end)
