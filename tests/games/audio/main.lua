function love.load()
  bip = love.audio.newSource("sons/bip.wav", "static")
  loop = love.audio.newSource("sons/loop.wav", "static")
  loop:setLooping(true)
  print("isPlaying avant", bip:isPlaying())
  bip:play()
  print("isPlaying après play", bip:isPlaying())
  print("type", bip:type(), bip:getType())
  print("volume", bip:getVolume())
  bip:setVolume(0.5)
  print("volume2", bip:getVolume())

  local c = bip:clone()
  print("clone isPlaying (pas encore lancé)", c:isPlaying())

  local ok, err = pcall(love.audio.newSource, "sons/missing.wav")
  print("fichier manquant", ok, err)

  local badSrc = love.audio.newSource("sons/bad.wav")
  local playedOk = badSrc:play()
  print("wav invalide : play renvoie", playedOk)

  loop:play()
  print("loop isPlaying", loop:isPlaying())

  love.audio.setVolume(0.8)
end

t = 0
function love.update(dt)
  t = t + dt
  if t > 0.3 and t < 0.35 then print("bip après 0.3s", bip:isPlaying()) end
  if t > 0.6 and t < 0.65 then print("bip après 0.6s (fini)", bip:isPlaying()) end
  if t > 1.0 and t < 1.05 then print("loop après 1s (toujours en cours, a bouclé)", loop:isPlaying()) end
  if t > 1.3 then love.event.quit() end
end
