-- A test double of the Uthernet II (WIZnet W5100 in indirect bus mode) for MAME, which has no
-- emulation of the card: taps on slot 1's registers ($C094-$C097) model the chip's memory, socket 0
-- as TCP and socket 1 as UDP. TCP bytes go through files in /xchg to eth_relay.py on the host, which
-- holds the real connection to the lab parlor; a UDP packet to port 53 is answered here with an A record.
local space = manager.machine.devices[":maincpu"].spaces["program"]
local BASE = 0xC094
local mem = {}
for a = 0, 0x7FFF do mem[a] = 0 end
local mr, addr = 0, 0
local sock = {}
for s = 0, 3 do sock[s] = {sr = 0, rx_wr = 0, tx_rd = 0} end
local inpos = 0
local logf = io.open("/out/w5100.log", "w")
local function log(s) logf:write(s .. "\n"); logf:flush() end

local function sbase(s) return 0x0400 + s * 0x100 end
local function get16(a) return (mem[a] << 8) | mem[a + 1] end
local function set16(a, v) mem[a] = (v >> 8) & 0xFF; mem[a + 1] = v & 0xFF end
local function txbase(s) return 0x4000 + s * 0x800 end
local function rxbase(s) return 0x6000 + s * 0x800 end

local function rx_put(s, bytes)
  local k = sock[s]
  for i = 1, #bytes do
    mem[rxbase(s) + (k.rx_wr & 0x7FF)] = bytes:byte(i)
    k.rx_wr = (k.rx_wr + 1) & 0xFFFF
  end
end

local function append(path, data)
  local f = io.open(path, "ab"); f:write(data); f:close()
end

local function dns_answer(s, q)
  -- q: the query; answer every A question with 10.0.2.2 (the relay connects to the lab parlor anyway)
  local id = q:sub(1, 2)
  local i = 13
  while q:byte(i) and q:byte(i) ~= 0 do i = i + q:byte(i) + 1 end
  local question = q:sub(13, i + 4)
  local name = {}
  local j = 13
  while q:byte(j) and q:byte(j) ~= 0 do
    name[#name + 1] = q:sub(j + 1, j + q:byte(j)); j = j + q:byte(j) + 1
  end
  log("dns " .. table.concat(name, "."))
  local r = id .. "\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00" .. question ..
    "\xC0\x0C\x00\x01\x00\x01\x00\x00\x0E\x10\x00\x04\x0A\x00\x02\x02"
  local b = sbase(s)
  local hdr = string.char(mem[b + 0x0C], mem[b + 0x0D], mem[b + 0x0E], mem[b + 0x0F], 0, 53,
    (#r >> 8) & 0xFF, #r & 0xFF)
  rx_put(s, hdr .. r)
end

local function command(s, c)
  local b = sbase(s)
  local k = sock[s]
  if c == 0x01 then           -- OPEN
    local mode = mem[b] & 0x0F
    k.sr = (mode == 1) and 0x13 or (mode == 2) and 0x22 or 0
    k.rx_wr, k.tx_rd = 0, 0
    set16(b + 0x22, 0); set16(b + 0x24, 0); set16(b + 0x28, 0)
    log(string.format("sock %d open mode %d sr %02x", s, mode, k.sr))
  elseif c == 0x04 then       -- CONNECT
    log(string.format("sock %d connect %d.%d.%d.%d:%d", s, mem[b + 0x0C], mem[b + 0x0D], mem[b + 0x0E],
      mem[b + 0x0F], get16(b + 0x10)))
    os.remove("/xchg/eth_closed")
    append("/xchg/eth_ctl", "open\n")
    k.sr = 0x17
  elseif c == 0x20 then       -- SEND
    local wr = get16(b + 0x24)
    local out = {}
    while k.tx_rd ~= wr do
      out[#out + 1] = string.char(mem[txbase(s) + (k.tx_rd & 0x7FF)])
      k.tx_rd = (k.tx_rd + 1) & 0xFFFF
    end
    set16(b + 0x22, k.tx_rd)
    local data = table.concat(out)
    if s == 0 then
      append("/xchg/eth_out", data)
    elseif get16(b + 0x10) == 53 then
      dns_answer(s, data)
    end
  elseif c == 0x08 or c == 0x10 then  -- DISCON, CLOSE
    if s == 0 and k.sr == 0x17 then append("/xchg/eth_ctl", "close\n") end
    k.sr = 0
    log(string.format("sock %d close", s))
  end
end

local function rd(a)
  local s = (a >= 0x0400 and a < 0x0800) and ((a - 0x0400) >> 8) or nil
  if s then
    local o = a & 0xFF
    local k = sock[s]
    if o == 0x01 then return 0 end
    if o == 0x03 then return k.sr end
    if o == 0x20 or o == 0x21 then
      local free = 0x800 - ((get16(sbase(s) + 0x24) - k.tx_rd) & 0xFFFF)
      return (o == 0x20) and (free >> 8) or (free & 0xFF)
    end
    if o == 0x26 or o == 0x27 then
      local n = (k.rx_wr - get16(sbase(s) + 0x28)) & 0xFFFF
      return (o == 0x26) and (n >> 8) or (n & 0xFF)
    end
  end
  return mem[a & 0x7FFF] or 0
end

local function wr(a, v)
  if a >= 0x0400 and a < 0x0800 and (a & 0xFF) == 0x01 then
    command((a - 0x0400) >> 8, v)
    return
  end
  mem[a & 0x7FFF] = v
end

local ntap = 0
w5100_rtap = space:install_read_tap(BASE, BASE + 3, "w5100r", function(offset, data, mask)
  local r = offset - BASE
  ntap = ntap + 1
  if ntap < 12 then log(string.format("rd %04x data %02x mask %x", offset, data, mask)) end
  if r == 0 then return mr end
  if r == 1 then return (addr >> 8) & 0xFF end
  if r == 2 then return addr & 0xFF end
  local v = rd(addr)
  if mr & 2 ~= 0 then addr = (addr + 1) & 0xFFFF end
  return v
end)

w5100_wtap = space:install_write_tap(BASE, BASE + 3, "w5100w", function(offset, data, mask)
  local r = offset - BASE
  ntap = ntap + 1
  if ntap < 12 then log(string.format("wr %04x data %02x mask %x", offset, data, mask)) end
  if r == 0 then
    if data & 0x80 ~= 0 then
      for a = 0, 0x7FFF do mem[a] = 0 end
      for s = 0, 3 do sock[s] = {sr = 0, rx_wr = 0, tx_rd = 0} end
      mr = 0
    else
      mr = data
    end
  elseif r == 1 then addr = (data << 8) | (addr & 0xFF)
  elseif r == 2 then addr = (addr & 0xFF00) | data
  else
    wr(addr, data)
    if mr & 2 ~= 0 then addr = (addr + 1) & 0xFFFF end
  end
end)

-- Each frame: bytes from the relay into socket 0's receive buffer, as many as it has room for (the chip's
-- TCP window holds the rest back), and the relay's news of a close.
w5100_frame = emu.add_machine_frame_notifier(function()
  local room = 0x800 - ((sock[0].rx_wr - get16(sbase(0) + 0x28)) & 0xFFFF)
  local f = room > 0 and io.open("/xchg/eth_in", "rb")
  if f then
    f:seek("set", inpos)
    local d = f:read(room)
    f:close()
    if d and #d > 0 then
      inpos = inpos + #d
      rx_put(0, d)
    end
  end
  if sock[0].sr == 0x17 then
    local c = io.open("/xchg/eth_closed", "r")
    if c then c:close(); sock[0].sr = 0x1C; log("remote closed") end
  end
end)
log("w5100 ready")
