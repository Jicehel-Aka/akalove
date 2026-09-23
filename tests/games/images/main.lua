function love.load()
  ship = love.graphics.newImage("images/ship.png")
  sheet = love.graphics.newImage("images/sheet.png")
  solid = love.graphics.newImage("images/solid.bmp")
  quadRed = love.graphics.newQuad(0, 0, 16, 16, sheet:getWidth(), sheet:getHeight())
  quadBlue = love.graphics.newQuad(16, 0, 16, 16, sheet)   -- variante Love 11 : image au lieu de sw,sh
  print("ship", ship:getWidth(), ship:getHeight())
end

function love.draw()
  -- sprite simple, non transformé
  love.graphics.setColor(1,1,1)
  love.graphics.draw(ship, 10, 10)

  -- teinté (rouge) : le fond noir doit rester noir (alpha respecté), le vaisseau devient sombre
  love.graphics.setColor(1, 0, 0)
  love.graphics.draw(ship, 40, 10)
  love.graphics.setColor(1,1,1)

  -- tourné 90°, centré (ox,oy = centre), agrandi x2 : le carré de 8x12 devient 12x8 et double
  love.graphics.draw(ship, 100, 40, math.pi/2, 2, 2, 8, 8)

  -- alpha 50%
  love.graphics.setColor(1,1,1,0.5)
  love.graphics.draw(ship, 10, 60)
  love.graphics.setColor(1,1,1)

  -- quads d'une feuille de sprites (deux cases 16x16)
  love.graphics.draw(sheet, quadRed, 150, 10)
  love.graphics.draw(sheet, quadBlue, 170, 10)

  -- BMP opaque
  love.graphics.draw(solid, 150, 40)

  -- hors écran (doit juste ne rien dessiner, pas planter)
  love.graphics.draw(ship, 400, 400)
  love.graphics.draw(ship, -50, -50)
end
