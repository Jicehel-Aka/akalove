function love.gamepadpressed(j, b) print("gamepadpressed", b, j:isGamepad(), j:isGamepadDown("b")) end
function love.gamepadreleased(j, b) print("gamepadreleased", b) end
function love.update(dt)
  local j = love.joystick.getJoysticks()[1]
  if j:getGamepadAxis("leftx") ~= 0 then print("axis", j:getGamepadAxis("leftx")) end
end
