local cjson = require "cjson"

function handle(item)
    print(cjson.encode(item))
end
