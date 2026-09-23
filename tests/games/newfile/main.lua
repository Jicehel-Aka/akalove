function love.load()
  local f = love.filesystem.newFile("hs.json")
  f:open("w")
  f:write('{"score":42}')
  f:close()

  local f2 = love.filesystem.newFile("hs.json")
  f2:open("r")
  print("read", f2:read())
  print("eof", f2:eof())
  print("size", f2:getSize())
  f2:close()

  local f3 = love.filesystem.newFile("hs.json", "r")
  print("direct", f3:isOpen(), f3:read(4))

  local f4 = love.filesystem.newFile("hs.json")
  f4:open("a")
  f4:write("!")
  f4:close()
  print("appended", love.filesystem.read("hs.json"))
  love.event.quit()
end
