-- Configuration : lue avant main.lua (même fichier que sous Love2D sur PC).
function love.conf(t)
  t.identity = "runner"            -- dossier de sauvegarde : AKA_Love/save/runner/
  t.window.title = "Infinite Runner"
  t.window.width  = 320            -- écran de l'AKA : 320 x 240
  t.window.height = 240
end
