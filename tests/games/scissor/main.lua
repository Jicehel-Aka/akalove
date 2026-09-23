function love.load()
  love.graphics.setScissor(0, 0, 20, 20)
  print("scissor", love.graphics.getScissor())
end
function love.draw()
  love.graphics.setColor(1, 0, 0)
  love.graphics.rectangle("fill", 0, 0, 100, 100)
  love.graphics.setScissor()
  love.graphics.setColor(0, 1, 0)
  love.graphics.rectangle("fill", 100, 100, 10, 10)
  love.graphics.setScissor(0, 0, 20, 20)
end
