
function format_value(obj)
    -- return hash to string
    if obj.type == "hash" or obj.type == "zset" then
        local hash_str = "{"
        for k, v in pairs(obj.value) do
            hash_str = hash_str ..k.."=>"..v.." ,"
        end
        return string.sub(hash_str, 0, -2) .."}"
        
    -- return list to string
    elseif obj.type == "set" or obj.type == "list" then
        return "["..table.concat(obj.value, ", ").."]"
    else
        return obj.value
    end
end

function handle(obj)
    local value = format_value(obj) 
    -- env = {version = $version, db_num = $db_num}
    print(env.db_num)
    local obj_str = string.format("{ type: %s, expire_time: %s, key: %s, value: %s}", 
        obj.type, obj.expire_time, obj.key, value)
     print(obj_str)
end
