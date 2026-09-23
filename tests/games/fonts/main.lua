function love.load()
  small = love.graphics.getFont()      -- taille par défaut (8)
  medium = love.graphics.newFont(16)
  big = love.graphics.newFont(24)
  print("heights", small:getHeight(), medium:getHeight(), big:getHeight())
  print("width café", medium:getWidth("café"))
end
function love.draw()
  love.graphics.setColor(1,1,1)
  love.graphics.setFont(small)
  love.graphics.print("Petit café", 4, 4)
  love.graphics.setFont(medium)
  love.graphics.print("Moyen", 4, 20)
  love.graphics.setFont(big)
  love.graphics.print("Grand", 4, 50)
  love.graphics.setFont(small)   -- rétabli pour le reste de l'UI
  love.graphics.print("Retour normal", 4, 90)
end
