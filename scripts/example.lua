function format_value(item)
    -- return hash to string
    if item.type == "hash" or item.type == "zset" then
        local hash_str = "{"
        for k, v in pairs(item.value) do
            hash_str = hash_str ..k.."=>"..v.." ,"
        end
        return string.sub(hash_str, 0, -2) .."}"
        
    -- return list to string
    elseif item.type == "set" or item.type == "list" then
        return "["..table.concat(item.value, ", ").."]"
    else
        return item.value
    end
end

function handle(item)
    local value = format_value(item) 
    -- env = {version = $version, db_num = $db_num}
    local item_str = string.format("{ select_db:%d, type: %s, expire_time: %s, key: %s, value: %s}", 
        env.db_num, item.type, item.expire_time, item.key, value)
     print(item_str)
end
