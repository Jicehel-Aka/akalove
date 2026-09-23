function love.draw()
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("HELLO", 0, 100, 320, "center")
  love.graphics.printf("mot mot mot mot mot mot mot mot mot mot mot", 0, 140, 60, "left")
  love.graphics.print({ { 1, 0, 0 }, "ROUGE", { 0, 1, 0 }, "VERT" }, 0, 10)
end
