--------------------------------------------------------------------------------------------------
-- CASSE-BRIQUES — un Arkanoid complet en Lua/Love2D pour AKA-Love
--
-- Ce jeu accompagne un tutoriel (voir le dossier PDF fourni à côté) qui explique CE code, morceau
-- par morceau, dans le même esprit que le tutoriel C++ original de Gamebuino AKA (21 chapitres).
-- Tout ce qui suit tient dans UN seul fichier exprès : c'est plus facile à lire d'une traite quand
-- on découvre le jeu, et le tutoriel indique où couper si vous voulez le séparer en plusieurs
-- fichiers `require()`-és (voir le chapitre « Assemblage final »).
--
-- Commandes : flèches ou stick = déplacer la raquette. A (espace) = lancer la balle / valider un
-- écran. MENU (échap) = pause. Sur PC : love2D standard, ce fichier tourne aussi tel quel.
--------------------------------------------------------------------------------------------------

local L, H                                    -- dimensions de l'écran (love.graphics.getWidth/Height)

--------------------------------------------------------------------------------------------------
-- Constantes de jeu : tout ce qu'on veut pouvoir régler facilement est ici, nulle part ailleurs.
--------------------------------------------------------------------------------------------------
local RAQUETTE_W, RAQUETTE_H = 44, 6
local RAQUETTE_VITESSE = 220                  -- pixels / seconde
local BALLE_RAYON = 3
local BALLE_VITESSE = 150                     -- norme du vecteur vitesse (constante : voir Chapitre 10)
local BRIQUE_W, BRIQUE_H = 24, 10
local BRIQUE_MARGE = 2
local BRIQUE_HAUT = 24                        -- offset en haut de l'écran avant la 1ère rangée
local BONUS_CHANCE = 0.22                     -- probabilité qu'une brique cassée lâche un bonus
local BONUS_VITESSE = 70
local VIES_DEPART = 3

--------------------------------------------------------------------------------------------------
-- État du jeu. `etat` pilote une petite machine à états (menu / jeu / pause / gameover / victoire) —
-- voir Chapitre 12 du tutoriel original. Chaque état a sa propre fonction update_XXX / dessine_XXX,
-- appelée depuis love.update/love.draw selon la valeur de `etat`.
--------------------------------------------------------------------------------------------------
local etat = "menu"
local score, vies, niveauNum = 0, VIES_DEPART, 1
local meilleurScore = 0

local raquette   -- { x, y, w, h, elargieJusqua }
local balle      -- { x, y, vx, vy, collee }
local briques    -- liste de { x, y, w, h, hp, incassable, couleur }
local bonus      -- liste de { x, y, type }
local particules -- liste de { x, y, vx, vy, vie, couleur } (petit effet visuel à la casse d'une brique)

--------------------------------------------------------------------------------------------------
-- Sons (Chapitre 13). Un `Source` par effet, rejouée à chaque fois (bip.wav-like : courte et
-- percussive, donc on ne gère pas de superposition compliquée — .play() redémarre depuis le début).
--------------------------------------------------------------------------------------------------
local son = {}

local function jouerSon(nom)
  if son[nom] then son[nom]:play() end
end

--------------------------------------------------------------------------------------------------
-- Rectangles et collisions (Chapitre 10) : une seule fonction générique, réutilisée partout —
-- raquette/balle, balle/brique, bonus/raquette. C'est LE test qui revient sans arrêt dans un jeu 2D.
--------------------------------------------------------------------------------------------------
local function seChevauchent(ax, ay, aw, ah, bx, by, bw, bh)
  return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah
end

--------------------------------------------------------------------------------------------------
-- Plans de niveaux (Chapitre 15) : une grille de codes, une ligne par rangée.
--   0 = rien   1 = brique normale (1 coup)   2 = brique solide (2 coups)   9 = incassable
-- new_plan_tableau(...) construit les briques à partir d'un plan ; genererNiveau(...) (Chapitre 16)
-- fabrique un plan au hasard (reproductible via un seed) pour les niveaux au-delà de ceux écrits ici.
--------------------------------------------------------------------------------------------------
local COLONNES = 11

local PLANS = {
  -- Niveau 1 : un simple pavé multicolore, pour prendre en main le jeu.
  {
    {1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1},
  },
  -- Niveau 2 : un cadre incassable, des briques solides (2 coups) au centre.
  {
    {9,9,9,9,9,9,9,9,9,9,9},
    {9,1,1,2,2,2,2,2,1,1,9},
    {9,1,2,2,2,2,2,2,2,1,9},
    {9,1,1,2,2,2,2,2,1,1,9},
    {9,9,9,9,9,9,9,9,9,9,9},
  },
}

-- Petits motifs réutilisables par le générateur procédural (Chapitre 16) : chacun renvoie true/false
-- pour dire si la case (colonne, rangée) contient une brique, sur une grille de `rangs` rangées.
local MOTIFS = {
  damier    = function(c, r) return (c + r) % 2 == 0 end,
  losange   = function(c, r, rangs)
    local cc = math.abs(c - (COLONNES - 1) / 2)
    return cc + math.abs(r - (rangs - 1) / 2) <= (COLONNES - 1) / 2
  end,
  rideaux   = function(c, r) return c % 3 ~= 1 end,
  escalier  = function(c, r) return c >= r end,
}
local NOMS_MOTIFS = { "damier", "losange", "rideaux", "escalier" }

-- Procédural ne veut pas dire aléatoire (Chapitre 16) : avec le même seed, on retrouve TOUJOURS le
-- même niveau. On sème avec le numéro de niveau : le niveau 5 est toujours le même niveau 5.
local function genererPlan(numero)
  love.math.setRandomSeed(numero * 7919)   -- un nombre premier, pour bien mélanger de petits seeds
  local rangs = 3 + math.min(4, math.floor(numero / 2))          -- de plus en plus grand
  local motif = MOTIFS[NOMS_MOTIFS[love.math.random(1, #NOMS_MOTIFS)]]
  local plan = {}
  for r = 1, rangs do
    plan[r] = {}
    for c = 1, COLONNES do
      if not motif(c - 1, r - 1, rangs) then
        plan[r][c] = 0
      else
        local jet = love.math.random(1, 100)
        if jet <= 8 then plan[r][c] = 9                           -- quelques incassables, rares
        elseif jet <= 8 + math.min(40, numero * 4) then plan[r][c] = 2   -- de plus en plus de solides
        else plan[r][c] = 1 end
      end
    end
  end
  return plan
end

local COULEUR_PAR_HP = {
  [1] = {0.9, 0.3, 0.3},
  [2] = {0.95, 0.65, 0.2},
}

local function construireBriquesDepuisPlan(plan)
  briques = {}
  local totalLargeur = COLONNES * (BRIQUE_W + BRIQUE_MARGE) - BRIQUE_MARGE
  local ox = (L - totalLargeur) / 2
  for r, ligne in ipairs(plan) do
    for c, code in ipairs(ligne) do
      if code ~= 0 then
        table.insert(briques, {
          x = ox + (c - 1) * (BRIQUE_W + BRIQUE_MARGE),
          y = BRIQUE_HAUT + (r - 1) * (BRIQUE_H + BRIQUE_MARGE),
          w = BRIQUE_W, h = BRIQUE_H,
          hp = (code == 9) and 1 or code,
          incassable = (code == 9),
        })
      end
    end
  end
end

--------------------------------------------------------------------------------------------------
-- Mise en place d'un niveau / d'une partie (Chapitre 17 : ce qu'on réinitialise et quand).
--------------------------------------------------------------------------------------------------
local function placerRaquetteEtBalle()
  raquette = { x = L / 2 - RAQUETTE_W / 2, y = H - 18, w = RAQUETTE_W, h = RAQUETTE_H, elargieJusqua = 0 }
  balle = { x = L / 2, y = raquette.y - BALLE_RAYON - 1, vx = 0, vy = 0, collee = true }
end

local function demarrerNiveau(numero)
  niveauNum = numero
  local plan = PLANS[numero] or genererPlan(numero)
  construireBriquesDepuisPlan(plan)
  bonus, particules = {}, {}
  placerRaquetteEtBalle()
end

local function demarrerPartie()
  score, vies = 0, VIES_DEPART
  demarrerNiveau(1)
  etat = "jeu"
end

--------------------------------------------------------------------------------------------------
-- Sauvegarde du meilleur score (Chapitre 17, version love.filesystem : l'API "objet" File, pour
-- changer de love.filesystem.read/write utilisées ailleurs dans les autres exemples AKA-Love).
--------------------------------------------------------------------------------------------------
local function chargerMeilleurScore()
  if not love.filesystem.getInfo("meilleur.txt") then return 0 end
  local f = love.filesystem.newFile("meilleur.txt", "r")
  local contenu = f:read()
  f:close()
  return tonumber(contenu) or 0
end

local function sauverMeilleurScore()
  local f = love.filesystem.newFile("meilleur.txt")
  f:open("w")
  f:write(tostring(meilleurScore))
  f:close()
end

--------------------------------------------------------------------------------------------------
-- love.load : chargé une fois. On y prépare les sons et on relit le meilleur score.
--------------------------------------------------------------------------------------------------
function love.load()
  L, H = love.graphics.getWidth(), love.graphics.getHeight()
  love.graphics.setBackgroundColor(0.06, 0.06, 0.12)

  for nom, fichier in pairs({ mur = "mur.wav", raquette = "raquette.wav", brique = "brique.wav",
                             bonus = "bonus.wav", vie = "vie_perdue.wav", victoire = "victoire.wav" }) do
    local ok, src = pcall(love.audio.newSource, "sons/" .. fichier, "static")
    if ok then son[nom] = src end          -- si le fichier manque, le jeu continue juste sans ce son
  end

  meilleurScore = chargerMeilleurScore()
  policeTitre = love.graphics.newFont(24)
  placerRaquetteEtBalle()
end

--------------------------------------------------------------------------------------------------
-- Entrées (Chapitre 07/08) : une seule fonction, utilisée en jeu comme dans les menus au clavier.
--------------------------------------------------------------------------------------------------
local function deplacerRaquette(dt)
  local vitesse = RAQUETTE_VITESSE
  if love.keyboard.isDown("left") then raquette.x = raquette.x - vitesse * dt end
  if love.keyboard.isDown("right") then raquette.x = raquette.x + vitesse * dt end
  raquette.x = math.max(0, math.min(L - raquette.w, raquette.x))
  if balle.collee then balle.x = raquette.x + raquette.w / 2 end
end

--------------------------------------------------------------------------------------------------
-- Lancer la balle (Chapitre 09) : toujours vers le haut, avec un léger angle selon le mouvement en
-- cours de la raquette — un petit clin d'œil qui rend le lancer moins figé.
--------------------------------------------------------------------------------------------------
local function lancerBalle()
  if not balle.collee then return end
  balle.collee = false
  local angle = -math.pi / 2 + (love.keyboard.isDown("left") and -0.3 or love.keyboard.isDown("right") and 0.3 or 0)
  balle.vx = math.cos(angle) * BALLE_VITESSE
  balle.vy = math.sin(angle) * BALLE_VITESSE
end

--------------------------------------------------------------------------------------------------
-- Rebond sur la raquette, « Niveau 3 » du chapitre 10 : un pourcentage continu selon l'endroit
-- touché (bord gauche = grand angle vers la gauche, centre = tout droit, bord droit = vers la
-- droite), à VITESSE CONSTANTE (seule la direction change — voir la note sur le sinus/cosinus).
--------------------------------------------------------------------------------------------------
local function rebondirSurRaquette()
  local centreBalle = balle.x
  local centreRaquette = raquette.x + raquette.w / 2
  -- pourcentage : -1 (bord gauche) .. 0 (centre) .. 1 (bord droit)
  local pourcentage = (centreBalle - centreRaquette) / (raquette.w / 2)
  pourcentage = math.max(-1, math.min(1, pourcentage))
  local angleMax = math.rad(60)   -- jamais totalement à l'horizontale
  local angle = -math.pi / 2 + pourcentage * angleMax
  balle.vx = math.cos(angle) * BALLE_VITESSE
  balle.vy = math.sin(angle) * BALLE_VITESSE
  jouerSon("raquette")
end

--------------------------------------------------------------------------------------------------
-- Collision balle / brique (Chapitre 11) : on trouve sur quel AXE la balle pénètre le moins dans la
-- brique (c'est celui-là qu'on inverse), sinon une balle qui arrive en biais traverse les coins.
--------------------------------------------------------------------------------------------------
local function toucherBrique(b, i)
  local penetrationX = math.min(balle.x + BALLE_RAYON - b.x, b.x + b.w - (balle.x - BALLE_RAYON))
  local penetrationY = math.min(balle.y + BALLE_RAYON - b.y, b.y + b.h - (balle.y - BALLE_RAYON))
  if penetrationX < penetrationY then balle.vx = -balle.vx else balle.vy = -balle.vy end

  if not b.incassable then
    b.hp = b.hp - 1
    for k = 1, 6 do   -- petit nuage de particules (purement décoratif)
      table.insert(particules, { x = b.x + b.w / 2, y = b.y + b.h / 2,
                                 vx = love.math.random(-60, 60), vy = love.math.random(-60, 60),
                                 vie = 0.35, couleur = COULEUR_PAR_HP[math.min(b.hp + 1, 2)] })
    end
    if b.hp <= 0 then
      score = score + 10 * niveauNum
      if love.math.random() < BONUS_CHANCE then
        table.insert(bonus, { x = b.x + b.w / 2, y = b.y + b.h / 2, type = "elargir" })
      end
      table.remove(briques, i)
      jouerSon("brique")
    else
      jouerSon("brique")
    end
  else
    jouerSon("mur")
  end
end

--------------------------------------------------------------------------------------------------
-- Bonus qui tombent (Chapitre 14) : un seul type ici pour rester lisible — élargit la raquette
-- pendant quelques secondes. Le tutoriel décrit aussi le multi-ball et le laser comme extensions.
--------------------------------------------------------------------------------------------------
local function mettreAJourBonus(dt)
  for i = #bonus, 1, -1 do
    local bo = bonus[i]
    bo.y = bo.y + BONUS_VITESSE * dt
    if seChevauchent(bo.x - 4, bo.y - 4, 8, 8, raquette.x, raquette.y, raquette.w, raquette.h) then
      raquette.elargieJusqua = love.timer.getTime() + 8
      table.remove(bonus, i)
      jouerSon("bonus")
    elseif bo.y > H then
      table.remove(bonus, i)
    end
  end
  local largeurCible = (raquette.elargieJusqua > love.timer.getTime()) and RAQUETTE_W * 1.6 or RAQUETTE_W
  raquette.w = largeurCible   -- (pas d'animation de transition : volontairement simple)
end

--------------------------------------------------------------------------------------------------
-- update_jeu : le cœur de la boucle de jeu (Chapitre 05/06).
--------------------------------------------------------------------------------------------------
local function verifierNiveauTermine()
  for _, b in ipairs(briques) do
    if not b.incassable then return false end
  end
  return true
end

local function perdreLaVie()
  vies = vies - 1
  jouerSon("vie")
  if vies <= 0 then
    if score > meilleurScore then meilleurScore = score; sauverMeilleurScore() end
    etat = "gameover"
  else
    placerRaquetteEtBalle()
  end
end

local function update_jeu(dt)
  deplacerRaquette(dt)
  mettreAJourBonus(dt)

  for i = #particules, 1, -1 do
    local p = particules[i]
    p.x, p.y = p.x + p.vx * dt, p.y + p.vy * dt
    p.vie = p.vie - dt
    if p.vie <= 0 then table.remove(particules, i) end
  end

  if balle.collee then return end

  balle.x = balle.x + balle.vx * dt
  balle.y = balle.y + balle.vy * dt

  if balle.x - BALLE_RAYON < 0 then balle.x = BALLE_RAYON; balle.vx = -balle.vx; jouerSon("mur")
  elseif balle.x + BALLE_RAYON > L then balle.x = L - BALLE_RAYON; balle.vx = -balle.vx; jouerSon("mur") end
  if balle.y - BALLE_RAYON < 0 then balle.y = BALLE_RAYON; balle.vy = -balle.vy; jouerSon("mur") end

  if balle.y - BALLE_RAYON > H then
    perdreLaVie()
    return
  end

  if balle.vy > 0 and seChevauchent(balle.x - BALLE_RAYON, balle.y - BALLE_RAYON, BALLE_RAYON * 2, BALLE_RAYON * 2,
                                    raquette.x, raquette.y, raquette.w, raquette.h) then
    balle.y = raquette.y - BALLE_RAYON
    rebondirSurRaquette()
  end

  for i = #briques, 1, -1 do
    local b = briques[i]
    if seChevauchent(balle.x - BALLE_RAYON, balle.y - BALLE_RAYON, BALLE_RAYON * 2, BALLE_RAYON * 2, b.x, b.y, b.w, b.h) then
      toucherBrique(b, i)
      break   -- une seule brique par frame : évite les doubles rebonds sur deux briques adjacentes
    end
  end

  if verifierNiveauTermine() then
    if score > meilleurScore then meilleurScore = score; sauverMeilleurScore() end
    if PLANS[niveauNum + 1] or niveauNum < 99 then
      etat = "victoire"
    end
  end
end

--------------------------------------------------------------------------------------------------
-- love.update / love.keypressed : le routeur de la machine à états (Chapitre 12 et 19).
--------------------------------------------------------------------------------------------------
function love.update(dt)
  if etat == "jeu" then update_jeu(dt) end
end

function love.keypressed(key)
  if key == "escape" then                         -- MENU : bascule pause, ou retour au menu ailleurs
    if etat == "jeu" then etat = "pause"
    elseif etat == "pause" then etat = "jeu"
    elseif etat == "gameover" or etat == "victoire" then etat = "menu" end
    return
  end
  if key ~= "space" then return end                -- A : action contextuelle selon l'écran
  if etat == "menu" then demarrerPartie()
  elseif etat == "jeu" then lancerBalle()
  elseif etat == "gameover" then etat = "menu"
  elseif etat == "victoire" then demarrerNiveau(niveauNum + 1); etat = "jeu" end
end

--------------------------------------------------------------------------------------------------
-- Dessin (Chapitre 04) : une fonction par écran, choisie selon `etat`.
--------------------------------------------------------------------------------------------------
local function dessinerBriques()
  for _, b in ipairs(briques) do
    if b.incassable then love.graphics.setColor(0.5, 0.5, 0.55)
    else
      local c = COULEUR_PAR_HP[math.min(b.hp, 2)]
      love.graphics.setColor(c[1], c[2], c[3])
    end
    love.graphics.rectangle("fill", b.x, b.y, b.w, b.h)
  end
end

local function dessinerHUD()
  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Score " .. score, 4, 4)
  love.graphics.printf("Niveau " .. niveauNum, 0, 4, L, "center")
  love.graphics.printf("Vies " .. vies, 0, 4, L - 4, "right")
end

local function dessine_jeu()
  dessinerBriques()

  for _, bo in ipairs(bonus) do
    love.graphics.setColor(0.3, 0.9, 1)
    love.graphics.circle("fill", bo.x, bo.y, 4)
  end
  for _, p in ipairs(particules) do
    love.graphics.setColor(p.couleur[1], p.couleur[2], p.couleur[3], math.max(0, p.vie / 0.35))
    love.graphics.rectangle("fill", p.x, p.y, 2, 2)
  end

  love.graphics.setColor(1, 1, 1)
  love.graphics.rectangle("fill", raquette.x, raquette.y, raquette.w, raquette.h)
  love.graphics.circle("fill", balle.x, balle.y, BALLE_RAYON)

  dessinerHUD()
  if balle.collee then
    love.graphics.printf("A pour lancer", 0, H / 2, L, "center")
  end
end

local function dessine_menu()
  love.graphics.setFont(policeTitre)
  love.graphics.setColor(1, 0.9, 0.3)
  love.graphics.printf("CASSE-BRIQUES", 0, 50, L, "center")
  love.graphics.setFont()   -- sans argument : revient à la police par défaut (voir le tutoriel, §police)
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("Meilleur score : " .. meilleurScore, 0, 110, L, "center")
  love.graphics.printf("A pour jouer", 0, 140, L, "center")
  love.graphics.printf("Fleches ou stick : raquette", 0, 170, L, "center")
  love.graphics.printf("MENU : pause en jeu", 0, 184, L, "center")
end

local function dessine_pause()
  dessine_jeu()
  love.graphics.setColor(0, 0, 0, 0.6)
  love.graphics.rectangle("fill", 0, 0, L, H)
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("PAUSE", 0, H / 2 - 10, L, "center")
  love.graphics.printf("MENU pour reprendre", 0, H / 2 + 8, L, "center")
end

local function dessine_gameover()
  love.graphics.setColor(0.7, 0.15, 0.15)
  love.graphics.printf("PERDU", 0, 80, L, "center")
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("Score : " .. score, 0, 110, L, "center")
  love.graphics.printf("Meilleur : " .. meilleurScore, 0, 126, L, "center")
  love.graphics.printf("A pour revenir au menu", 0, 160, L, "center")
end

local function dessine_victoire()
  love.graphics.setColor(0.3, 0.9, 0.4)
  love.graphics.printf("NIVEAU " .. niveauNum .. " TERMINE !", 0, 90, L, "center")
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("Score : " .. score, 0, 120, L, "center")
  love.graphics.printf("A pour continuer", 0, 150, L, "center")
end

local ECRANS = { menu = dessine_menu, jeu = dessine_jeu, pause = dessine_pause,
                 gameover = dessine_gameover, victoire = dessine_victoire }

function love.draw()
  ECRANS[etat]()
end
