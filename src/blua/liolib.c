/*
** $Id: liolib.c,v 2.73.1.3 2008/01/18 17:47:43 roberto Exp $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define liolib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "../i_system.h"
#include "../g_game.h"
#include "../netcode/d_netfil.h"
#include "../netcode/net_command.h"
#include "../lua_libs.h"
#include "../byteptr.h"
#include "../lua_script.h"
#include "../m_misc.h"
#include "../i_time.h"
#include <dirent.h>
#include <sys/stat.h>

#define IO_INPUT	1
#define IO_OUTPUT	2

#define FMT_FILECALLBACKID "file_callback_%d"

static INT64 numwrittenbytes = 0;
static INT64 numopenedfiles = 0;

static int pushresult (lua_State *L, int i, const char *filename) {
  int en = errno;
  if (i) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushnil(L);
    if (filename)
      lua_pushfstring(L, "%s: %s", filename, strerror(en));
    else
      lua_pushfstring(L, "%s", strerror(en));
    lua_pushinteger(L, en);
    return 3;
  }
}

#define tofilep(L)	((FILE **)luaL_checkudata(L, 1, LUA_FILEHANDLE))

static int io_type (lua_State *L) {
  void *ud;
  luaL_checkany(L, 1);
  ud = lua_touserdata(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_FILEHANDLE);
  if (ud == NULL || !lua_getmetatable(L, 1) || !lua_rawequal(L, -2, -1))
    lua_pushnil(L);
  else if (*((FILE **)ud) == NULL)
    lua_pushliteral(L, "closed file");
  else
    lua_pushliteral(L, "file");
  return 1;
}

static FILE *tofile (lua_State *L) {
  FILE **f = tofilep(L);
  if (*f == NULL)
    luaL_error(L, "attempt to use a closed file");
  return *f;
}

static FILE **newfile (lua_State *L) {
  FILE **pf = (FILE **)lua_newuserdata(L, sizeof(FILE *));
  *pf = NULL;
  luaL_getmetatable(L, LUA_FILEHANDLE);
  lua_setmetatable(L, -2);
  return pf;
}

static int io_noclose (lua_State *L) {
  lua_pushnil(L);
  lua_pushliteral(L, "cannot close standard file");
  return 2;
}

static int io_fclose (lua_State *L) {
  FILE **p = tofilep(L);
  int ok = (fclose(*p) == 0);
  *p = NULL;
  return pushresult(L, ok, NULL);
}

static int aux_close (lua_State *L) {
  lua_getfenv(L, 1);
  lua_getfield(L, -1, "__close");
  return (lua_tocfunction(L, -1))(L);
}

static int io_close (lua_State *L) {
  if (lua_isnone(L, 1))
    lua_rawgeti(L, LUA_ENVIRONINDEX, IO_OUTPUT);
  tofile(L);
  return aux_close(L);
}

static int io_gc (lua_State *L) {
  FILE *f = *tofilep(L);
  if (f != NULL)
    aux_close(L);
  return 0;
}

static int io_tostring (lua_State *L) {
  FILE *f = *tofilep(L);
  if (f == NULL)
    lua_pushliteral(L, "file (closed)");
  else
    lua_pushfstring(L, "file (%p)", f);
  return 1;
}

void MakePathDirs(char *path)
{
	char *c;

	for (c = path; *c; c++)
		if (*c == '/' || *c == '\\')
		{
			char sep = *c;
			*c = '\0';
			I_mkdir(path, 0755);
			*c = sep;
		}
}

static int CheckFileName(lua_State* L, const char* filename)
{
	if (strchr(filename, '\\'))
	{
		luaL_error(L, "access denied to %s: \\ is not allowed, use / instead", filename);
		return pushresult(L, 0, filename);
	}

	if (strstr(filename, "./") || strstr(filename, "..") || strchr(filename, ':') || filename[0] == '/')
	{
		luaL_error(L, "access denied to %s", filename);
		return pushresult(L,0,filename);
	}

	return 0;
}

static int io_open (lua_State *L) {
	const char *filename = luaL_checkstring(L, 1);
	const char *mode = luaL_optstring(L, 2, "r");
	int checkresult;

	checkresult = CheckFileName(L, filename);
	if (checkresult)
		return checkresult;

	FILE **pf = newfile(L);
	char *realfilename = va("%s" PATHSEP "%s", luafiledir, filename);
	MakePathDirs(realfilename);
	*pf = fopen(realfilename, mode);
	return (*pf == NULL) ? pushresult(L, 0, filename) : 1;
}

static int io_tmpfile (lua_State *L) {
  FILE **pf = newfile(L);
  *pf = tmpfile();
  return (*pf == NULL) ? pushresult(L, 0, NULL) : 1;
}

static int io_readline (lua_State *L);

static void aux_lines (lua_State *L, int idx, int toclose) {
  lua_pushvalue(L, idx);
  lua_pushboolean(L, toclose);
  lua_pushcclosure(L, io_readline, 2);
}

static int f_lines (lua_State *L) {
  tofile(L);
  aux_lines(L, 1, 0);
  return 1;
}

static int read_number (lua_State *L, FILE *f) {
  lua_Number d;
  if (fscanf(f, LUA_NUMBER_SCAN, &d) == 1) {
    lua_pushnumber(L, d);
    return 1;
  }
  else return 0;
}

static int test_eof (lua_State *L, FILE *f) {
  int c = getc(f);
  ungetc(c, f);
  lua_pushlstring(L, NULL, 0);
  return (c != EOF);
}

static int read_line (lua_State *L, FILE *f) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (;;) {
    size_t l;
    char *p = luaL_prepbuffer(&b);
    if (fgets(p, LUAL_BUFFERSIZE, f) == NULL) {
      luaL_pushresult(&b);
      return (lua_objlen(L, -1) > 0);
    }
    l = strlen(p);
    if (l == 0 || p[l-1] != '\n')
      luaL_addsize(&b, l);
    else {
      luaL_addsize(&b, l - 1);
      luaL_pushresult(&b);
      return 1;
    }
  }
}

static int read_chars (lua_State *L, FILE *f, size_t n) {
  size_t rlen;
  size_t nr;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  rlen = LUAL_BUFFERSIZE;
  do {
    char *p = luaL_prepbuffer(&b);
    if (rlen > n) rlen = n;
    nr = fread(p, sizeof(char), rlen, f);
    luaL_addsize(&b, nr);
    n -= nr;
  } while (n > 0 && nr == rlen);
  luaL_pushresult(&b);
  return (n == 0 || lua_objlen(L, -1) > 0);
}

static int g_read (lua_State *L, FILE *f, int first) {
  int nargs = lua_gettop(L) - 1;
  int success;
  int n;
  clearerr(f);
  if (nargs == 0) {
    success = read_line(L, f);
    n = first+1;
  }
  else {
    luaL_checkstack(L, nargs+LUA_MINSTACK, "too many arguments");
    success = 1;
    for (n = first; nargs-- && success; n++) {
      if (lua_type(L, n) == LUA_TNUMBER) {
        size_t l = (size_t)lua_tointeger(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = lua_tostring(L, n);
        luaL_argcheck(L, p && p[0] == '*', n, "invalid option");
        switch (p[1]) {
          case 'n':
            success = read_number(L, f);
            break;
          case 'l':
            success = read_line(L, f);
            break;
          case 'a':
            read_chars(L, f, ~((size_t)0));
            success = 1;
            break;
          default:
            return luaL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (ferror(f))
    return pushresult(L, 0, NULL);
  if (!success) {
    lua_pop(L, 1);
    lua_pushnil(L);
  }
  return n - first;
}

static int f_read (lua_State *L) {
  return g_read(L, tofile(L), 2);
}

static int io_readline (lua_State *L) {
  FILE *f = *(FILE **)lua_touserdata(L, lua_upvalueindex(1));
  int sucess;
  if (f == NULL)
    luaL_error(L, "file is already closed");
  sucess = read_line(L, f);
  if (ferror(f))
    return luaL_error(L, "%s", strerror(errno));
  if (sucess) return 1;
  else {
    if (lua_toboolean(L, lua_upvalueindex(2))) {
      lua_settop(L, 0);
      lua_pushvalue(L, lua_upvalueindex(1));
      aux_close(L);
    }
    return 0;
  }
}

static int g_write (lua_State *L, FILE *f, int arg) {
  int nargs = lua_gettop(L) - 1;
  int status = 1;
  for (; nargs--; arg++) {
    if (lua_type(L, arg) == LUA_TNUMBER) {
      status = status &&
          fprintf(f, LUA_NUMBER_FMT, lua_tonumber(L, arg)) > 0;
    }
    else {
      size_t l;
      const char *s = luaL_checklstring(L, arg, &l);
      status = status && (fwrite(s, sizeof(char), l, f) == l);
    }
  }
  return pushresult(L, status, NULL);
}

static int f_write (lua_State *L) {
  return g_write(L, tofile(L), 2);
}

static int f_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = tofile(L);
  int op = luaL_checkoption(L, 2, "cur", modenames);
  long offset = luaL_optlong(L, 3, 0);
  op = fseek(f, offset, mode[op]);
  if (op)
    return pushresult(L, 0, NULL);
  else {
    lua_pushinteger(L, ftell(f));
    return 1;
  }
}

static int f_setvbuf (lua_State *L) {
  static const int mode[] = {_IONBF, _IOFBF, _IOLBF};
  static const char *const modenames[] = {"no", "full", "line", NULL};
  FILE *f = tofile(L);
  int op = luaL_checkoption(L, 2, NULL, modenames);
  lua_Integer sz = luaL_optinteger(L, 3, LUAL_BUFFERSIZE);
  int res = setvbuf(f, NULL, mode[op], sz);
  return pushresult(L, res == 0, NULL);
}

static int f_flush (lua_State *L) {
  return pushresult(L, fflush(tofile(L)) == 0, NULL);
}

// Novas funções implementadas
static int io_delete(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    int checkresult = CheckFileName(L, filename);
    if (checkresult) return checkresult;
    
    char *realfilename = va("%s" PATHSEP "%s", luafiledir, filename);
    int result = remove(realfilename);
    return pushresult(L, result == 0, filename);
}

static int io_mkdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int checkresult = CheckFileName(L, path);
    if (checkresult) return checkresult;
    
    char *realpath = va("%s" PATHSEP "%s", luafiledir, path);
    int result = I_mkdir(realpath, 0755);
    return pushresult(L, result == 0, path);
}

static int io_deletedir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int checkresult = CheckFileName(L, path);
    if (checkresult) return checkresult;
    
    char *realpath = va("%s" PATHSEP "%s", luafiledir, path);
    int result = rmdir(realpath);
    return pushresult(L, result == 0, path);
}

static int io_getext(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    const char *ext = strrchr(filename, '.');
    if (ext && ext != filename) {
        lua_pushstring(L, ext);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int io_exists(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    int checkresult = CheckFileName(L, filename);
    if (checkresult) return checkresult;
    
    char *realfilename = va("%s" PATHSEP "%s", luafiledir, filename);
    struct stat st;
    int result = stat(realfilename, &st);
    lua_pushboolean(L, result == 0);
    return 1;
}

static int io_isdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int checkresult = CheckFileName(L, path);
    if (checkresult) return checkresult;
    
    char *realpath = va("%s" PATHSEP "%s", luafiledir, path);
    struct stat st;
    int result = stat(realpath, &st);
    lua_pushboolean(L, result == 0 && S_ISDIR(st.st_mode));
    return 1;
}

static int io_listfiles(lua_State *L) {
    const char *path = luaL_optstring(L, 1, "");
    int checkresult = CheckFileName(L, path);
    if (checkresult) return checkresult;
    
    char *realpath = va("%s" PATHSEP "%s", luafiledir, path);
    DIR *dir = opendir(realpath);
    if (!dir) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    
    lua_newtable(L);
    int i = 1;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            lua_pushnumber(L, i++);
            lua_pushstring(L, entry->d_name);
            lua_settable(L, -3);
        }
    }
    closedir(dir);
    return 1;
}

static const luaL_Reg iolib[] = {
  {"close", io_close},
  {"delete", io_delete},
  {"deletedir", io_deletedir},
  {"exists", io_exists},
  {"getext", io_getext},
  {"isdir", io_isdir},
  {"listfiles", io_listfiles},
  {"mkdir", io_mkdir},
  {"open", io_open},
  {"tmpfile", io_tmpfile},
  {"type", io_type},
  {NULL, NULL}
};

static const luaL_Reg flib[] = {
  {"close", io_close},
  {"flush", f_flush},
  {"lines", f_lines},
  {"read", f_read},
  {"seek", f_seek},
  {"setvbuf", f_setvbuf},
  {"write", f_write},
  {"__gc", io_gc},
  {"__tostring", io_tostring},
  {NULL, NULL}
};

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_FILEHANDLE);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_register(L, NULL, flib);
}

static void createstdfile (lua_State *L, FILE *f, int k, const char *fname) {
  *newfile(L) = f;
  if (k > 0) {
    lua_pushvalue(L, -1);
    lua_rawseti(L, LUA_ENVIRONINDEX, k);
  }
  lua_pushvalue(L, -2);
  lua_setfenv(L, -2);
  lua_setfield(L, -3, fname);
}

static void newfenv (lua_State *L, lua_CFunction cls) {
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, cls);
  lua_setfield(L, -2, "__close");
}

LUALIB_API int luaopen_io (lua_State *L) {
  createmeta(L);
  newfenv(L, io_fclose);
  lua_replace(L, LUA_ENVIRONINDEX);
  luaL_register(L, LUA_IOLIBNAME, iolib);
  newfenv(L, io_noclose);
  createstdfile(L, stdin, IO_INPUT, "stdin");
  createstdfile(L, stdout, IO_OUTPUT, "stdout");
  createstdfile(L, stderr, 0, "stderr");
  lua_pop(L, 1);
  return 1;
}
