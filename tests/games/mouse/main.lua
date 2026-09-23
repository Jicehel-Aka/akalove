function love.load()
  x, y = love.mouse.getPosition()
  print("depart", x, y)
end
function love.mousepressed(x, y, button)
  print("mousepressed", x, y, button)
end
function love.mousereleased(x, y, button)
  print("mousereleased", x, y, button)
end
function love.update(dt)
  x, y = love.mouse.getPosition()
end
function love.draw()
  love.graphics.setColor(1,1,0)
  love.graphics.circle("fill", x, y, 4)
end
