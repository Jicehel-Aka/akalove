function love.load()
  print("size", love.graphics.getWidth(), love.graphics.getHeight())
  local w, h = love.window.getMode()
  print("mode", w, h)
end

function love.draw()
  love.graphics.setColor(1, 0, 0)
  love.graphics.rectangle("fill", 0, 0, 400, 300)
  love.graphics.setColor(0, 0, 1)
  love.graphics.circle("fill", 400, 300, 100)
end
