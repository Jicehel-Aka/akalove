-- Infinite Runner — identique sous Love2D 11.5 (PC) et AKA-Love
local L, H, SOL
local joueur, obstacles, fonds, score, gameOver, timerSpawn
local imgFond, sonSaut, sonMort

local function charge(chemin, loader)             -- si l'asset manque, on continue sans
  local ok, res = pcall(loader, chemin)
  return ok and res or nil
end

local function joue(son) if son then son:stop(); son:play() end end

function love.load()
  L, H = love.graphics.getWidth(), love.graphics.getHeight()
  SOL = H - 40
  imgFond = charge("images/fond.png", love.graphics.newImage)
  sonSaut = charge("sons/saut.wav", function(p) return love.audio.newSource(p, "static") end)
  sonMort = charge("sons/mort.wav", function(p) return love.audio.newSource(p, "static") end)
  joueur = { x = 40, y = SOL - 16, l = 16, h = 16, vy = 0, saut = -220, gravite = 600 }
  obstacles, fonds = {}, { { x = 0 }, { x = L } }
  score, gameOver, timerSpawn = 0, false, 1.2
end

function love.keypressed(k)
  if gameOver and (k == "space" or k == "return") then love.load() return end
  if (k == "space" or k == "up") and joueur.y >= SOL - joueur.h then
    joueur.vy = joueur.saut
    joue(sonSaut)
  end
end

function love.update(dt)
  if gameOver then return end
  joueur.vy = joueur.vy + joueur.gravite * dt
  joueur.y = joueur.y + joueur.vy * dt
  if joueur.y > SOL - joueur.h then joueur.y = SOL - joueur.h; joueur.vy = 0 end

  local largeurFond = imgFond and imgFond:getWidth() or L
  for _, f in ipairs(fonds) do
    f.x = f.x - 60 * dt
    if f.x + largeurFond < 0 then f.x = f.x + 2 * largeurFond end
  end

  timerSpawn = timerSpawn - dt
  if timerSpawn <= 0 then
    table.insert(obstacles, { x = L, y = SOL - 24, l = 12, h = 24 })
    timerSpawn = love.math.random(10, 20) / 10
  end

  for i = #obstacles, 1, -1 do
    local o = obstacles[i]
    o.x = o.x - 140 * dt
    if o.x + o.l < 0 then
      table.remove(obstacles, i)
    elseif joueur.x < o.x + o.l and o.x < joueur.x + joueur.l and
           joueur.y < o.y + o.h and o.y < joueur.y + joueur.h then
      gameOver = true
      joue(sonMort)
    end
  end
  score = score + dt * 10
end

function love.draw()
  if imgFond then
    for _, f in ipairs(fonds) do love.graphics.draw(imgFond, f.x, 0) end
  else
    love.graphics.setColor(0.1, 0.1, 0.3)
    love.graphics.rectangle("fill", 0, 0, L, SOL)
  end
  love.graphics.setColor(0.3, 0.8, 0.3)
  love.graphics.rectangle("fill", 0, SOL, L, H - SOL)
  love.graphics.setColor(1, 0.2, 0.2)
  for _, o in ipairs(obstacles) do love.graphics.rectangle("fill", o.x, o.y, o.l, o.h) end
  love.graphics.setColor(1, 1, 1)
  love.graphics.rectangle("fill", joueur.x, joueur.y, joueur.l, joueur.h)
  love.graphics.print("Score: " .. math.floor(score), 4, 4)
  if gameOver then
    love.graphics.printf("GAME OVER - Espace pour rejouer", 0, H / 2 - 4, L, "center")
  end
end
