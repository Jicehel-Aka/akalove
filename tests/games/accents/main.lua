function love.draw()
  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Café à Noël, ça été: ¡Hola! ¿Qué? Grüße, Straße, öäü ÖÄÜ «Œuf»", 4, 4)

  -- texte coloré multi-segments : la position du 2e segment dépend de text_width() du 1er (accents inclus)
  love.graphics.print({ { 1, 0, 0 }, "café ", { 0, 1, 0 }, "X" }, 0, 16)

  -- alignement à droite : la largeur d'un mot accentué doit compter les CODEPOINTS, pas les octets UTF-8
  -- ("café" = 4 caractères/32px, alors que 4 lettres + 1 octet de continuation utf8 en compterait 5 à tort)
  love.graphics.printf("café", 0, 28, 100, "right")

  -- retour à la ligne : un mot accentué à cheval sur la limite ne doit jamais couper un caractère
  -- multi-octets en deux (chaque ligne reste un texte UTF-8 valide)
  love.graphics.printf("mot été mot été mot", 0, 40, 40, "left")
end
