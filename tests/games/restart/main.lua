function love.load()
  if love.filesystem.getInfo("ran.flag") then
    print("second run")
    love.event.quit()
  else
    love.filesystem.write("ran.flag", "1")
    error("premier lancement")
  end
end
