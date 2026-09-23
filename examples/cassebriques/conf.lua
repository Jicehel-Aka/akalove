-- Casse-Briques AKA-Love — configuration
function love.conf(t)
  t.identity = "cassebriques"      -- dossier de sauvegarde : AKA_Love/save/cassebriques/
  t.window.title = "Casse-Briques"
  t.window.width  = 320            -- résolution native de l'AKA : pas de mise à l'échelle
  t.window.height = 240
  t.aka = { scale = "none" }
end
