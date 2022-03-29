if #arg ~= 2 then
    error("Ein oder beide Argumente sind leer")
end
local offset = tonumber(arg[1])
local amp = tonumber(arg[2])
if offset == nil then
    error("offset ist keine Zahl")
end
if amp == nil then
    error("amplitude ist keine Zahl")
end
local radstep = 0.1
local rads = 0

while true do
    local val = ((math.sin(rads)) * amp + offset)
    rads = rads + radstep
    notify(val)
    sleep(1000)
end
