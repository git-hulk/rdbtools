#include "script.h"
#include <stdlib.h>
#include <string.h>
#include "log.h"

LUALIB_API int (luaopen_cjson) (lua_State *L); 

void
lua_loadlib(lua_State *L, const char *libname, lua_CFunction luafunc) {
    lua_pushcfunction(L, luafunc);
    lua_pushstring(L, libname);
    lua_call(L, 1, 0); 
}

lua_State *
script_init(const char *filename)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_loadlib(L, "cjson", luaopen_cjson);
    if (luaL_dofile(L, filename)) {
        logger(ERROR,"%s", lua_tostring(L, -1));
    }

    lua_newtable(L);
    lua_setglobal(L, RDB_ENV);

    if (!script_check_func_exists(L, RDB_CB)) {
        logger(ERROR, "function %s is reqired in %s.\n", RDB_CB, filename);
    }
    return L;
}

void
script_release(lua_State *L)
{
    lua_close(L);
}

int
script_check_func_exists(lua_State * L, const char *func_name) {
    int ret;

    lua_getglobal(L, func_name);
    ret = lua_isfunction(L,lua_gettop(L));
    lua_pop(L,-1);

    return ret;
}

void
script_pushtableinteger(lua_State* L , char* key , double value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

void
script_pushtablestring(lua_State* L , char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

void
script_push_list_elem(lua_State* L , char* key, int ind)
{
    lua_pushstring(L, key);
    lua_rawseti(L,-2,ind + 1);
}
