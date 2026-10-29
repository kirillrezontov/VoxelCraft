local v = voxelcraft

return {
    on_load = function()
        local visits = (tonumber(v.get_state("visits")) or 0) + 1
        assert(v.set_state("visits", tostring(visits)))
        v.message("Glowstone ready - visit " .. visits)
    end,
    on_place = function(event)
        if event.id == "demo:glowstone" then
            local placed = (tonumber(v.get_state("placed")) or 0) + 1
            assert(v.set_state("placed", tostring(placed)))
            v.message("Placed glowstone " .. placed)
        end
    end,
    on_craft = function(event)
        if event.id == "demo:glowstone" then
            v.message("Crafted glowstone")
        end
    end
}
