-- même script que tests/conformance/t_transform.lua du dossier de projet
function love.load()
  love.graphics.setDefaultFilter("nearest", "nearest")
  love.graphics.setLineStyle("rough")
end

function love.draw()
  love.graphics.setColor(1, 0, 0)
  love.graphics.rectangle("fill", 10, 10, 40, 40)
  love.graphics.push()
  love.graphics.translate(160, 120)
  love.graphics.rotate(math.pi / 6)
  love.graphics.setColor(0, 1, 0, 0.5)
  love.graphics.rectangle("fill", -30, -20, 60, 40)
  love.graphics.pop()
end
