// Généré par tools/embed_lua.py — NE PAS ÉDITER (sources : lua/*.lua)
#include "embedded_lua.h"

const char* const EMBEDDED_BOOT_LUA = R"LUA(-- boot.lua : exécuté par le runtime avant main.lua.
-- Réalise en Lua ce que Love2D réalise en Lua : love.run, love.event, love.handlers, quelques modules simples.

love._version = "11.5"
love._version_major, love._version_minor, love._version_revision = 11, 5, 0
love._version_codename = "Mysterious Mysteries"
function love.getVersion() return 11, 5, 0, "Mysterious Mysteries" end

love.aka.version = "0.3"
arg = {}

-- ------------------------------------------------------------------------------------------------
-- os.exit : quitter le jeu = revenir au lanceur (pas de redémarrage de la console)
function os.exit(code) love.event.quit(code or 0) end

-- ------------------------------------------------------------------------------------------------
-- love.event : file d'événements en Lua ; pump() interroge les boutons (C++)
do
  local queue, head, tail = {}, 1, 0
  love.event = {}
  function love.event.push(name, ...)
    tail = tail + 1
    queue[tail] = { n = select("#", ...) + 1, name, ... }
  end
  function love.event.pump() love.aka._poll() end
  function love.event.poll()
    return function()
      if head <= tail then
        local e = queue[head]
        queue[head] = nil
        head = head + 1
        return unpack(e, 1, e.n)
      end
    end
  end
  function love.event.clear() queue, head, tail = {}, 1, 0 end
  function love.event.quit(code) love.event.push("quit", code) end
end

-- ------------------------------------------------------------------------------------------------
-- Gestionnaires d'événements et boucle principale (calqués sur le love.run de Love 11)
love.handlers = setmetatable({
  keypressed      = function(k, s, r) if love.keypressed  then return love.keypressed(k, s, r) end end,
  keyreleased     = function(k, s)    if love.keyreleased then return love.keyreleased(k, s) end end,
  gamepadpressed  = function(j, b)    if love.gamepadpressed  then return love.gamepadpressed(j, b) end end,
  gamepadreleased = function(j, b)    if love.gamepadreleased then return love.gamepadreleased(j, b) end end,
  mousepressed    = function(...) if love.mousepressed  then return love.mousepressed(...)  end end,
  mousereleased   = function(...) if love.mousereleased then return love.mousereleased(...) end end,
  mousemoved      = function(...) if love.mousemoved    then return love.mousemoved(...)    end end,
  quit            = function() end,
}, { __index = function(_, name) error("Unknown event: " .. tostring(name)) end })

function love.run()
  if love.load then love.load({}, {}) end
  if love.timer then love.timer.step() end
  local dt = 0
  return function()                                   -- appelée une fois par frame par le runtime
    love.event.pump()
    for name, a, b, c, d, e in love.event.poll() do
      if name == "quit" then
        if not love.quit or not love.quit() then return a or 0 end
      end
      love.handlers[name](a, b, c, d, e)
    end
    dt = love.timer.step()
    if love.update then love.update(dt) end
    love.graphics.origin()
    love.graphics.clear(love.graphics.getBackgroundColor())
    if love.draw then love.draw() end
    love.graphics.present()
    love.timer.sleep(0.001)
  end
end

-- ------------------------------------------------------------------------------------------------
-- conf.lua : renvoie la table de configuration (structure de Love 11)
function love.aka._load_conf()
  local t = {
    identity = nil, version = "11.5", console = false,
    window = { title = "Untitled", icon = nil, width = 800, height = 600, borderless = false,
               resizable = false, minwidth = 1, minheight = 1, fullscreen = false,
               fullscreentype = "desktop", vsync = 1, msaa = 0, display = 1, highdpi = false },
    modules = {},
    aka = {},
  }
  if love.filesystem.getInfo("conf.lua", "file") then
    local chunk, err = love.filesystem.load("conf.lua")
    if not chunk then error(err, 0) end
    chunk()
  end
  if love.conf then love.conf(t) end
  if type(t.window) ~= "table" then t.window = { width = 800, height = 600, title = "Untitled" } end
  if type(t.aka) ~= "table" then t.aka = {} end
  return t
end

-- ------------------------------------------------------------------------------------------------
-- love.filesystem.lines (au-dessus de read)
function love.filesystem.lines(name)
  local s = love.filesystem.read(name)
  if not s then error("Could not open file " .. tostring(name), 2) end
  local pos = 1
  return function()
    if pos > #s then return nil end
    local e = s:find("\n", pos, true)
    local line
    if e then line = s:sub(pos, e - 1); pos = e + 1 else line = s:sub(pos); pos = #s + 1 end
    return (line:gsub("\r$", ""))
  end
end

-- ------------------------------------------------------------------------------------------------
-- love.mouse est implémenté côté C++ (bind_system.cpp) : curseur virtuel piloté par le stick,
-- clic gauche = bouton A. Voir open_system().

-- ------------------------------------------------------------------------------------------------
-- Politique « jamais de silence » : une fonction que le runtime ne fournit pas vaut nil (comme dans Love
-- quand une fonction n'existe pas : la détection de fonctionnalités `if love.x.y then` reste juste), et
-- l'absence est journalisée UNE fois dans le terminal du PC. Un appel donne l'erreur Lua habituelle
-- « attempt to call field 'y' (a nil value) » avec fichier:ligne.
love.aka.missing = {}
local function guard(mod, name)
  setmetatable(mod, { __index = function(_, k)
    local key = name .. "." .. k
    if not love.aka.missing[key] then
      love.aka.missing[key] = true
      print("[AKA-Love] absent : love." .. key)
    end
    return nil
  end })
end
for _, name in ipairs({ "sound", "image", "data", "thread", "physics", "video", "touch", "font" }) do
  love[name] = love[name] or {}
end
for _, name in ipairs({ "graphics", "keyboard", "joystick", "window", "math", "system", "timer", "filesystem",
                        "event", "mouse", "sound", "image", "data", "thread", "physics", "video",
                        "touch", "font" }) do
  if type(love[name]) == "table" then guard(love[name], name) end
end
)LUA";

const char* const EMBEDDED_ERRORHANDLER_LUA = R"LUA(-- errorhandler.lua : écran d'erreur d'AKA-Love (rouge). Appelé au point de l'erreur avec le message ;
-- renvoie la fonction qui sera appelée à chaque frame jusqu'à ce qu'elle renvoie une valeur non nulle
-- ("restart" pour relancer le jeu, un nombre pour quitter).

function love.errorhandler(msg)
  msg = tostring(msg)
  local trace = debug.traceback("", 3)
  trace = trace:gsub("^\nstack traceback:\n", ""):gsub("^\t%[C%]: in function 'error'\n", "")
  print("Error: " .. msg .. "\n" .. trace)                  -- visible dans le terminal du PC

  love.aka.setInputMode("both")                             -- l'écran d'erreur reçoit clavier ET manette
  love.aka.useNativeResolution()                            -- lisible même si le jeu était en 800x600 virtuel
  love.graphics.reset()
  love.graphics.setBackgroundColor(0.55, 0, 0)
  local text = ("Erreur Lua\n\n" .. msg .. "\n\n" .. trace):gsub("\t", "  ")

  return function()
    love.event.pump()
    for name, a, b in love.event.poll() do
      if name == "quit" then return a or 1 end
      if name == "keypressed" and (a == "space" or a == "return") then return "restart" end
      if name == "keypressed" and a == "x" then return 1 end
      if name == "gamepadpressed" and b == "a" then return "restart" end
      if name == "gamepadpressed" and b == "b" then return 1 end
    end
    love.graphics.clear(love.graphics.getBackgroundColor())
    love.graphics.setColor(1, 1, 1)
    love.graphics.printf(text, 6, 6, love.graphics.getWidth() - 12)
    love.graphics.printf("A : relancer      B : quitter", 0, love.graphics.getHeight() - 14,
                         love.graphics.getWidth(), "center")
    love.graphics.present()
    love.timer.sleep(0.02)
  end
end
)LUA";

