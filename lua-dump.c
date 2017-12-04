#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


#define BUFFER_SIZE 1024
#define SLOT_SIZE	16

//#define ESCAPE_SPACE

struct write_buffer {
	char* ptr;
	size_t size;
	size_t offset;
	char init[BUFFER_SIZE];
};

struct array_kv {
	struct write_buffer* k;
	struct write_buffer* v;
};

struct array_context {
	int offset;
	int size;
	struct array_kv** slots;
	struct array_kv* init[SLOT_SIZE];
};

static inline void 
buffer_init(struct write_buffer* buffer) {
	buffer->ptr = buffer->init;
	buffer->size = BUFFER_SIZE;
	buffer->offset = 0;
}

static inline void 
buffer_reservce(struct write_buffer* buffer, size_t len) {
	if (buffer->offset + len <= buffer->size) {
		return;
	}
	size_t nsize = buffer->size * 2;
	while (nsize < buffer->offset + len) {
		nsize = nsize * 2;
	}
	char* nptr = (char*)malloc(nsize);
	memcpy(nptr, buffer->ptr, buffer->size);
	buffer->size = nsize;

	if (buffer->ptr != buffer->init)
		free(buffer->ptr);
	buffer->ptr = nptr;
}

static inline void 
buffer_addchar(struct write_buffer* buffer, char c) {
	buffer_reservce(buffer, 1);
	buffer->ptr[buffer->offset++] = c;
}

static inline void 
buffer_addlstring(struct write_buffer* buffer, const char* str,size_t len) {
	buffer_reservce(buffer, len);
	memcpy(buffer->ptr + buffer->offset, str, len);
	buffer->offset += len;
}

static inline void 
buffer_addstring(struct write_buffer* buffer, const char* str) {
	int len = strlen(str);
	buffer_addlstring(buffer,str,len);
}

static inline void 
buffer_release(struct write_buffer* buffer) {
	if (buffer->ptr != buffer->init)
		free(buffer->ptr);
}

static inline void
tab(struct write_buffer* buffer,int depth) {
#ifndef ESCAPE_SPACE
	int i;
	for(i=0;i<depth;i++)
		buffer_addchar(buffer, '\t');
#endif
}

static inline void
newline(struct write_buffer* buffer) {
#ifdef ESCAPE_SPACE
	buffer_addchar(buffer, ',');
#else
	buffer_addstring(buffer, ",\n");
#endif
}

static inline void
table_begin(struct write_buffer* buffer) {
#ifdef ESCAPE_SPACE
	buffer_addchar(buffer, '{');
#else
	buffer_addstring(buffer, "{\n");
#endif
}

static inline void 
array_init(struct array_context* array) {
	array->size = SLOT_SIZE;
	array->offset = 0;
	array->slots = array->init;
}

static inline void 
array_release(struct array_context* array) {
	int i;
	for (i = 0; i < array->offset;i++) {
		struct array_kv* kv = array->slots[i];
		buffer_release(kv->k);
		buffer_release(kv->v);
		free(kv->k);
		free(kv->v);
		free(kv);
	}

	if (array->slots != array->init)
		free(array->slots);
}

static inline void 
array_append(struct array_context* array, struct write_buffer* k, struct write_buffer* v) {
	if (array->offset == array->size) {
		int nsize = array->size * 2;
		struct array_kv** nslots = (struct array_kv**)malloc(sizeof(struct array_kv*) * nsize);
		memcpy(nslots, array->slots, sizeof(struct array_kv*) * array->size);
		if (array->slots != array->init)
			free(array->slots);
		array->slots = nslots;
		array->size = nsize;
	}
	struct array_kv* kv = (struct array_kv*)malloc(sizeof(*kv));
	kv->k = k;
	kv->v = v;
	array->slots[array->offset++] = kv;
}

static inline int 
array_compare(const void* a, const void* b) {
	struct array_kv* l = *(struct array_kv**)a;
	struct array_kv* r = *(struct array_kv**)b;
	return strcmp(l->k->ptr, r->k->ptr);
}

static inline void 
array_sort(struct array_context* array) {
	qsort(array->slots, array->offset, sizeof(struct array_kv*), array_compare);
}

void pack_table(lua_State* L, struct write_buffer* buffer, int index, int depth);
void pack_table_sort(lua_State* L, struct write_buffer* buffer, int index, int depth);

void 
pack_key(lua_State* L, struct write_buffer* buffer, int index, int depth) {
	int type = lua_type(L, index);
	switch (type)
	{
		case LUA_TNUMBER:
		{
			lua_pushvalue(L,index);
			size_t sz = 0;
			const char *str = lua_tolstring(L, -1, &sz);
			lua_pop(L,1);
			buffer_addlstring(buffer, str, sz);
			break;
		}
		case LUA_TSTRING:
		{
			size_t sz = 0;
			const char *str = lua_tolstring(L, index, &sz);
			buffer_addchar(buffer, '\"');
			buffer_addlstring(buffer, str, sz);
			buffer_addchar(buffer, '\"');
			break;
		}
		default:
			luaL_error(L, "key not support type %s", lua_typename(L, type));
			break;
	}
}


void 
pack_value(lua_State* L, struct write_buffer* buffer, int index, int depth,int sort) {
	int type = lua_type(L, index);
	switch (type) {
		case LUA_TNIL:
			buffer_addstring(buffer, "nil");
			break;
		case LUA_TNUMBER: 
		{
			lua_pushvalue(L,index);
			size_t sz = 0;
			const char *str = lua_tolstring(L, -1, &sz);
			lua_pop(L,1);
			buffer_addlstring(buffer, str, sz);
			break;
		}
		case LUA_TBOOLEAN:
		{
			 int val = lua_toboolean(L, index);
			 if (val)
				 buffer_addstring(buffer, "true");
			 else
				 buffer_addstring(buffer, "false");
			 break;
		}
		case LUA_TSTRING:
		{
			size_t sz = 0;
			const char *str = lua_tolstring(L, index, &sz);
			buffer_addchar(buffer, '\"');
			buffer_addlstring(buffer, str, sz);
			buffer_addchar(buffer, '\"');
			break;
		}
		case LUA_TTABLE:
		{
		   if (index < 0) {
			   index = lua_gettop(L) + index + 1;
		   }

		   if (sort) {
		   		pack_table_sort(L, buffer, index, ++depth);
		   } else {
		   		pack_table(L, buffer, index, ++depth);
		   }

		   break;
		}
		default:
			luaL_error(L, "value no support type %s", lua_typename(L, type));
			break;
	}
}

void 
pack_table(lua_State* L, struct write_buffer* buffer, int index, int depth) {
	table_begin(buffer);
	int array_size = lua_rawlen(L, index);
	int i;
	for (i = 1; i <= array_size; i++) {
		lua_rawgeti(L, index, i);
		int top = lua_gettop(L);
		tab(buffer, depth);
		pack_value(L, buffer, -1, depth, 0);
		newline(buffer);
		top = lua_gettop(L);
		lua_pop(L, 1);
	}

	lua_pushnil(L);
	while (lua_next(L, index) != 0) {
		if (lua_type(L,-2) == LUA_TNUMBER) {
			int i = (int)lua_tointeger(L, -2);
			lua_Number n = lua_tonumber(L, -2);
			if ((lua_Number)i == n) {
				if (i > 0 && i <= array_size) {
					lua_pop(L, 1);
					continue;
				}
			}
		}

		tab(buffer, depth);

		buffer_addchar(buffer, '[');
		pack_key(L, buffer, -2, depth);
		buffer_addstring(buffer, "] = ");

		pack_value(L, buffer, -1, depth, 0);

		newline(buffer);
		lua_pop(L, 1);
	}
	
	tab(buffer, depth-1);
	buffer_addchar(buffer, '}');
}

void 
pack_table_sort(lua_State* L, struct write_buffer* buffer, int index, int depth) {
	table_begin(buffer);
	int array_size = lua_rawlen(L, index);
	int i;
	for (i = 1; i <= array_size; i++) {
		lua_rawgeti(L, index, i);
		tab(buffer, depth);
		pack_value(L, buffer, -1, depth, 1);
		newline(buffer);
		lua_pop(L, 1);
	}

	struct array_context array;
	array_init(&array);

	lua_pushnil(L);
	while (lua_next(L, index) != 0) {
		if (lua_type(L,-2) == LUA_TNUMBER) {
			int i = (int)lua_tointeger(L, -2);
			lua_Number n = lua_tonumber(L, -2);
			if ((lua_Number)i == n) {
				if (i > 0 && i <= array_size) {
					lua_pop(L, 1);
					continue;
				}
			}
		}

		struct write_buffer* kbuffer = (struct write_buffer*)malloc(sizeof(*kbuffer));
		buffer_init(kbuffer);

		buffer_addchar(kbuffer, '[');
		pack_key(L, kbuffer, -2, depth);
		buffer_addstring(kbuffer, "] = ");

		struct write_buffer* vbuffer = (struct write_buffer*)malloc(sizeof(*vbuffer));
		buffer_init(vbuffer);

		pack_value(L, vbuffer, -1, depth, 1);

		array_append(&array, kbuffer, vbuffer);

		lua_pop(L, 1);
	}
	
	array_sort(&array);

	for (i = 0; i < array.offset;i++) {
		struct array_kv* kv = array.slots[i];
		tab(buffer, depth);
		buffer_addlstring(buffer, kv->k->ptr, kv->k->offset);
		buffer_addlstring(buffer, kv->v->ptr, kv->v->offset);
		newline(buffer);
	}

	array_release(&array);

	tab(buffer, depth-1);
	buffer_addchar(buffer, '}');
}

static int 
pack(lua_State* L) {
	int type = lua_type(L, 1);
	if (type != LUA_TTABLE)
		luaL_error(L, "must be table");
	
	struct write_buffer buffer;
	buffer_init(&buffer);

    pack_table(L, &buffer, 1, 1);

	lua_pushlstring(L, buffer.ptr, buffer.offset);

	buffer_release(&buffer);
	return 1;
}

static int 
pack_sort(lua_State* L) {
	int type = lua_type(L, 1);
	if (type != LUA_TTABLE)
		luaL_error(L, "must be table");
	
	struct write_buffer buffer;
	buffer_init(&buffer);

    pack_table_sort(L, &buffer, 1, 1);

	lua_pushlstring(L, buffer.ptr, buffer.offset);

	buffer_release(&buffer);
	return 1;
}

enum token_type {
	T_STRING,
	T_NUMBER,
	T_NIL,
	T_BOOL
};

struct parser_token {
	enum token_type tt;
	union {
		lua_Number number;
		char* str;
		int boolean;
	} value;
	int strlen;
};

struct parser_context {
	char* data;
	char* ptr;
	struct parser_token* token;
	char* reserve;
	int length;
};


static inline void 
skip_space(struct parser_context* parser) {
	char *n = parser->ptr;
	while (isspace(*n) && *n) {
		n++;
	}
	parser->ptr = n;
	return;
}

static inline int 
expect(struct parser_context* parser, char c) {
	return *parser->ptr == c;
}

static inline void 
next_token(struct parser_context *parser) {
	parser->ptr++;
	skip_space(parser);
}

void
next_string_token(lua_State* L,struct parser_context *parser) {
	assert(expect(parser,'"'));
	parser->ptr++;
	int index = 0;
	char ch = *parser->ptr;
	while(ch != 0 && ch != '"') {
		if (index >= parser->length) {
			parser->reserve = realloc(parser->reserve,parser->length*2);
			parser->length *= 2;
		}
		parser->reserve[index] = ch;
		index++;
		parser->ptr++;
		ch = *parser->ptr;
	}
	parser->token->tt = T_STRING;
	parser->token->value.str = parser->reserve;
	parser->token->strlen = index;
}

void
next_number_token(lua_State* L,struct parser_context *parser) {
	char ch = *parser->ptr;

	int index = 0;
	while((ch >= '0' && ch <= '9') || ch == '.') {
		if (index >= parser->length) {
			parser->reserve = realloc(parser->reserve,parser->length*2);
			parser->length *= 2;
		}
		parser->reserve[index++] = ch;
		parser->ptr++;
		ch = *parser->ptr;
	}
	parser->token->tt = T_NUMBER;
	lua_pushlstring(L,parser->reserve,index);
	parser->token->value.number = lua_tonumber(L,-1);
	lua_pop(L,1);
}

void
unpack_table(lua_State* L,struct parser_context *parser);

void
unpack_key(lua_State* L,struct parser_context *parser,int i) {
	char ch = *parser->ptr;

	if (ch == '[') {
		ch = *(++parser->ptr);
		if (ch == '"') {
			next_string_token(L,parser);
			lua_pushlstring(L,parser->token->value.str,parser->token->strlen);
			parser->ptr++;
		} else if (ch >= '0' && ch <= '9') {
			next_number_token(L,parser);
			lua_Integer integer = parser->token->value.number;
			lua_Number number = parser->token->value.number;
			if (integer == number) {
				lua_pushinteger(L,integer);
			} else {
				lua_pushnumber(L,number);
			}
			skip_space(parser);
		} else {
			luaL_error(L,"unpack key error");
		}
		++parser->ptr;
		skip_space(parser);
	} else if (ch == '"') {
		next_string_token(L,parser);
		lua_pushlstring(L,parser->token->value.str,parser->token->strlen);
		++parser->ptr;
		skip_space(parser);
	} else if (ch >= '0' && ch <= '9') {
		next_number_token(L,parser);
		lua_Integer integer = parser->token->value.number;
		lua_Number number = parser->token->value.number;
		if (integer == number) {
			lua_pushinteger(L,integer);
		} else {
			lua_pushnumber(L,number);
		}
		skip_space(parser);
	} else if (ch == '{') {
		unpack_table(L,parser);
		++parser->ptr;
		skip_space(parser);
	} else {
		luaL_error(L,"unpack key error:unknown ch");
	}

	assert(expect(parser,',') || expect(parser,'='));
}

void
unpack_value(lua_State* L,struct parser_context *parser) {
	next_token(parser);

	char ch = *parser->ptr;

	//string?
	if (ch == '"') {
		next_string_token(L,parser);
		lua_pushlstring(L,parser->token->value.str,parser->token->strlen);
		parser->ptr++;
		return;
	}

	//number?
	if (ch >= '0' && ch <= '9') {
		next_number_token(L,parser);
		lua_Integer integer = parser->token->value.number;
		lua_Number number = parser->token->value.number;
		if (integer == number) {
			lua_pushinteger(L,integer);
		} else {
			lua_pushnumber(L,number);
		}
		return;
	}

	//nil?
	if (ch == 'n') {
		if (strncmp(parser->ptr,"nil",3) == 0) {
			parser->token->tt = T_NIL;
			parser->ptr += 3;
			lua_pushnil(L);
			return;
		}
	}

	//true?
	if (ch == 't') {
		if (strncmp(parser->ptr,"true",4) == 0) {
			parser->token->tt = T_BOOL;
			parser->token->value.boolean = 1;
			parser->ptr += 4;
			lua_pushboolean(L,parser->token->value.boolean);
			return;
		}
	}

	//false?
	if (ch == 'f') {
		if (strncmp(parser->ptr,"false",5) == 0) {
			parser->token->tt = T_BOOL;
			parser->token->value.boolean = 0;
			parser->ptr += 5;
			lua_pushboolean(L,parser->token->value.boolean);
			return;
		}
	}

	//table?
	if (ch == '{') {
		unpack_table(L,parser);
		parser->ptr++;
	}
}

void
unpack_table(lua_State* L,struct parser_context *parser) {
	assert(expect(parser,'{'));

	lua_newtable(L);

	parser->ptr++;

	int i = 1;
	while(!expect(parser,'}')) {
		skip_space(parser);
		if (expect(parser,',')) {
			next_token(parser);
			if (expect(parser,'}')) {
				break;
			} else {
				unpack_key(L,parser,i);
			}
		} else {
			unpack_key(L,parser,i);
		}

		if (expect(parser,',')) {
			lua_seti(L,-2,i);
			i++;
			continue;
		}
		assert(expect(parser,'='));

		unpack_value(L,parser);
		lua_settable(L,-3);
	}
}

int
unpack(lua_State* L) {
	size_t size;
	const char* str = luaL_checklstring(L,1,&size);
	struct parser_context parser;
	struct parser_token token;

	parser.data = (char*)str;
	parser.ptr = parser.data;
	parser.token = &token;
	parser.reserve = malloc(64);
	parser.length = 64;

	unpack_table(L,&parser);

	free(parser.reserve);

	return 1;
}

int
luaopen_dump(lua_State* L){
	luaL_Reg l[] = {
		{ "pack", pack },
		{ "pack_sort", pack_sort },
		{ "unpack", unpack },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	return 1;
}