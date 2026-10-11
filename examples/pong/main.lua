-- Pong AKA — exemple « complet » : raquettes en sprites, rebond sonore, score, sauvegarde.
-- Identique sous Love2D 11.5 sur PC (`love examples/pong`) et sur la console AKA-Love.
--
-- Commandes :
--   Haut / Bas   : déplacer ta raquette (gauche)      [flèches sur PC, D-Pad sur l'AKA]
--   A (Espace)   : servir / rejouer
--   MENU (Échap) : quitter
-- La raquette de droite est tenue par l'ordinateur (IA simple, battable).

local W, H = 320, 240
local PW, PH = 8, 40          -- taille d'une raquette (= taille du sprite paddle.png)
local BALL = 8                -- taille de la balle (= taille du sprite ball.png)

local imgPaddle, imgBall, sonRebond
local gauche, droite, balle, scoreG, scoreD, enJeu, meilleur

-- Chargement tolérant : si un asset manque, on continue (dessin de secours en rectangles).
local function charge(chemin, loader)
  local ok, res = pcall(loader, chemin)
  return ok and res or nil
end

local function joue(son)
  if son then son:stop(); son:play() end
end

local function sert(dir)
  balle = { x = W / 2 - BALL / 2, y = H / 2 - BALL / 2,
            vx = 150 * dir, vy = love.math.random(-80, 80) }
  enJeu = true
end

local function lire_meilleur()
  return tonumber(love.filesystem.read("meilleur.txt") or "") or 0
end

function love.load()
  love.graphics.setBackgroundColor(0.05, 0.07, 0.12)
  imgPaddle  = charge("images/paddle.png", love.graphics.newImage)
  imgBall    = charge("images/ball.png",   love.graphics.newImage)
  sonRebond  = charge("sons/rebond.wav", function(p) return love.audio.newSource(p, "static") end)
  gauche = { x = 10,          y = H / 2 - PH / 2, vitesse = 200 }
  droite = { x = W - 10 - PW, y = H / 2 - PH / 2, vitesse = 150 }
  scoreG, scoreD = 0, 0
  meilleur = lire_meilleur()
  enJeu = false                 -- on attend le service (A)
  balle = { x = W / 2 - BALL / 2, y = H / 2 - BALL / 2, vx = 0, vy = 0 }
end

local function clamp(v, lo, hi)
  if v < lo then return lo elseif v > hi then return hi else return v end
end

local function rebond_mur() joue(sonRebond) end

function love.update(dt)
  -- Raquette du joueur (clavier / D-Pad).
  if love.keyboard.isDown("up")   then gauche.y = gauche.y - gauche.vitesse * dt end
  if love.keyboard.isDown("down") then gauche.y = gauche.y + gauche.vitesse * dt end
  gauche.y = clamp(gauche.y, 0, H - PH)

  if not enJeu then return end

  -- IA : la raquette de droite suit la balle, mais plafonnée (donc battable).
  local cible = balle.y + BALL / 2 - PH / 2
  if droite.y + 2 < cible then droite.y = droite.y + droite.vitesse * dt
  elseif droite.y - 2 > cible then droite.y = droite.y - droite.vitesse * dt end
  droite.y = clamp(droite.y, 0, H - PH)

  balle.x = balle.x + balle.vx * dt
  balle.y = balle.y + balle.vy * dt

  -- Rebonds haut / bas.
  if balle.y <= 0 then balle.y = 0; balle.vy = -balle.vy; rebond_mur() end
  if balle.y + BALL >= H then balle.y = H - BALL; balle.vy = -balle.vy; rebond_mur() end

  -- Rebond sur une raquette (AABB) : on renvoie et on accélère un peu ; l'angle dépend du point d'impact.
  local function frappe(p, cote)
    if balle.x < p.x + PW and p.x < balle.x + BALL and balle.y < p.y + PH and p.y < balle.y + BALL then
      balle.vx = -balle.vx * 1.05
      local rel = (balle.y + BALL / 2) - (p.y + PH / 2)
      balle.vy = balle.vy + rel * 3
      balle.x = cote == "G" and (p.x + PW) or (p.x - BALL)   -- on décolle la balle de la raquette
      joue(sonRebond)
    end
  end
  frappe(gauche, "G")
  frappe(droite, "D")

  -- Point marqué : la balle sort par un côté.
  if balle.x + BALL < 0 then
    scoreD = scoreD + 1; sert(1)
  elseif balle.x > W then
    scoreG = scoreG + 1; sert(-1)
    if scoreG > meilleur then meilleur = scoreG; love.filesystem.write("meilleur.txt", tostring(meilleur)) end
  end
end

function love.keypressed(key)
  if key == "escape" then love.event.quit() end          -- MENU
  if (key == "space" or key == "return") and not enJeu then
    sert(love.math.random(0, 1) == 0 and -1 or 1)         -- A : service
  end
end

local function dessine_raquette(p)
  if imgPaddle then love.graphics.setColor(1, 1, 1); love.graphics.draw(imgPaddle, p.x, p.y)
  else love.graphics.setColor(0.9, 0.9, 0.9); love.graphics.rectangle("fill", p.x, p.y, PW, PH) end
end

function love.draw()
  -- Filet central.
  love.graphics.setColor(0.25, 0.3, 0.4)
  for y = 0, H, 16 do love.graphics.rectangle("fill", W / 2 - 1, y, 2, 8) end

  dessine_raquette(gauche)
  dessine_raquette(droite)

  if imgBall then love.graphics.setColor(1, 1, 1); love.graphics.draw(imgBall, balle.x, balle.y)
  else love.graphics.setColor(1, 1, 1); love.graphics.rectangle("fill", balle.x, balle.y, BALL, BALL) end

  love.graphics.setColor(1, 1, 1)
  love.graphics.print(scoreG .. "   " .. scoreD, W / 2 - 18, 6)
  love.graphics.printf("Record " .. meilleur, 0, 6, W - 8, "right")
  if not enJeu then
    love.graphics.printf("A : servir     MENU : quitter", 0, H - 20, W, "center")
  end
end
