-- errorhandler.lua : écran d'erreur d'AKA-Love (rouge). Appelé au point de l'erreur avec le message ;
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
