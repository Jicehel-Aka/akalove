-- Attrape ! : déplacez la barre (croix) pour attraper les carrés qui tombent.
-- A (espace) : rejouer.  MENU (échap) : quitter.
-- N'utilise que l'API standard de Love2D : le même code tourne sur PC avec `love examples/attrape`.

local W, H = 320, 240
local barre, carres, score, vies, meilleur, temps, delai, fini

local function lire_meilleur()
  local texte = love.filesystem.read("meilleur.txt")   -- nil si le fichier n'existe pas encore
  return tonumber(texte) or 0
end

local function nouvelle_partie()
  barre = { x = W / 2 - 24, y = H - 20, w = 48, h = 8, vitesse = 200 }
  carres, score, vies, temps, delai, fini = {}, 0, 3, 0, 0.8, false
end

function love.load()
  love.graphics.setBackgroundColor(0.08, 0.10, 0.20)
  meilleur = lire_meilleur()
  nouvelle_partie()
end

function love.update(dt)
  if vies <= 0 then
    if not fini then                                   -- fin de partie : une seule fois
      fini = true
      if score > meilleur then meilleur = score end
      love.filesystem.write("meilleur.txt", tostring(meilleur))   -- sauvegarde sur la carte SD
    end
    return
  end

  if love.keyboard.isDown("left")  then barre.x = barre.x - barre.vitesse * dt end
  if love.keyboard.isDown("right") then barre.x = barre.x + barre.vitesse * dt end
  barre.x = math.max(0, math.min(W - barre.w, barre.x))

  temps = temps + dt
  if temps >= delai then                               -- un nouveau carré, de plus en plus souvent
    temps = 0
    delai = math.max(0.3, delai - 0.02)
    table.insert(carres, { x = love.math.random(0, W - 12), y = -12, t = 12, v = 70 + score * 3 })
  end

  for i = #carres, 1, -1 do
    local c = carres[i]
    c.y = c.y + c.v * dt
    local touche = c.y + c.t >= barre.y and c.y <= barre.y + barre.h
               and c.x + c.t >= barre.x and c.x <= barre.x + barre.w
    if touche then
      score = score + 1
      table.remove(carres, i)
    elseif c.y > H then
      vies = vies - 1
      table.remove(carres, i)
    end
  end
end

function love.keypressed(key)
  if key == "space" and vies <= 0 then nouvelle_partie() end   -- bouton A
  if key == "escape" then love.event.quit() end                -- bouton MENU
end

function love.draw()
  love.graphics.setColor(0.3, 0.8, 0.5)
  love.graphics.rectangle("fill", barre.x, barre.y, barre.w, barre.h)
  love.graphics.setColor(1, 0.8, 0.2)
  for _, c in ipairs(carres) do
    love.graphics.rectangle("fill", c.x, c.y, c.t, c.t)
  end
  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Score " .. score .. "   Vies " .. vies, 8, 6)
  love.graphics.printf("Meilleur " .. meilleur, 0, 6, W - 8, "right")
  if vies <= 0 then
    love.graphics.printf("Perdu !   A : rejouer", 0, H / 2 - 6, W, "center")
  end
end
