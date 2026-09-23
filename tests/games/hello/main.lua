function love.load()
  print("Bonjour, tout fonctionne !")
  print(10 / 2, type(unpack), type(loadstring), type(setfenv), love.graphics.getWidth(), love.graphics.getHeight())
end

function love.draw()
  love.graphics.setColor(1, 0, 0)
  love.graphics.rectangle("fill", 10, 10, 40, 40)
  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Hello Love2D sur AKA !", 10, 60)
end
