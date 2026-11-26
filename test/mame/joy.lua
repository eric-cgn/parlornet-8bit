-- The Apple joystick in the game port (MAME's -gameio joy), for the step runners (run.lua, gsrun.lua).
-- Its fields are found by their bits, since their names depend on the other cards: with the IIe's mouse
-- card in, the joystick is MAME's second player.
local J = {}
local function field(name, mask)
  local port = manager.machine.ioport.ports[":gameio:joy:" .. name]
  if not port then return nil end
  for _, f in pairs(port.fields) do if f.mask == mask then return f end end
end

-- The stick held at x and y (0 to 255, 128 the middle), or let go back to the middle with no numbers.
function J.stick(x, y)
  local fx, fy = field("joystick_1_x", 0xFF), field("joystick_1_y", 0xFF)
  if x then fx:set_value(x); fy:set_value(y) else fx:clear_value(); fy:clear_value() end
end

-- Button b (0 or 1) down (v 1) or up (v 0).
function J.button(b, v)
  local f = field("joystick_buttons", b == 0 and 0x10 or 0x20)
  if v == 1 then f:set_value(1) else f:clear_value() end
end

-- The joystick as the VICE rig's joy step has it: bits 1 up, 2 down, 4 left, 8 right, 16 button 0 and 32
-- button 1, the stick all the way over.
function J.bits(bits)
  local x = (bits & 4 ~= 0) and 0 or (bits & 8 ~= 0) and 255 or 128
  local y = (bits & 1 ~= 0) and 0 or (bits & 2 ~= 0) and 255 or 128
  J.stick(x, y)
  J.button(0, (bits & 16 ~= 0) and 1 or 0)
  J.button(1, (bits & 32 ~= 0) and 1 or 0)
end

function J.release()
  J.stick(); J.button(0, 0); J.button(1, 0)
end

-- With no joystick in the port nothing ends the paddle timers' pulses, so $C064 and $C065 read with bit
-- 7 set: on (true) the port reads that way, off (false) the joystick is back. at is where the program
-- reads them: $C064 on the IIe, $E0C064 on the IIgs.
local tap = nil
function J.nostick(space, on, at)
  at = at or 0xC064
  if on and not tap then
    tap = space:install_read_tap(at, at + 1, "nostick", function(offset, data, mask)
      return data | 0x80
    end)
  elseif not on and tap then
    tap:remove(); tap = nil
  end
end

return J
