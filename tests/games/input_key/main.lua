function love.keypressed(k, s, r) print("keypressed", k, s, r) end
function love.keyreleased(k, s) print("keyreleased", k, s) end
function love.gamepadpressed(j, b) print("gamepadpressed", b) end
function love.update(dt)
  if love.keyboard.isDown("a") then print("alias-a") end
  if love.keyboard.isDown("d", "right") then print("right?") end
end
