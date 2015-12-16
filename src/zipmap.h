#ifndef _ZIPMAP_H_
#define _ZIPMAP_H_

#include "script.h"

void push_zipmap(lua_State *L, const char *zm);
void zipmap_dump(const char *zm);
#endif
