#ifndef _SCRIPT_H_                                                          
#define _SCRIPT_H_                                                          
#include <lua.h>                                                            
#include <lauxlib.h>                                                        
#include <lualib.h>                                                         

#define RDB_ENV "env"
#define RDB_CB "handle"

lua_State *script_init(const char *filename);                               
void script_release(lua_State *L);                                          
int script_check_func_exists(lua_State * L, const char *func_name);         
void script_pushtablestring(lua_State* L , char* key , char* value);
void script_pushtableinteger(lua_State* L , char* key , int value);
void script_pushtableunsigned(lua_State* L , char* key , unsigned value);
void script_push_list_elem(lua_State* L, char* key, int ind);
void script_need_gc(lua_State* L);
#endif
