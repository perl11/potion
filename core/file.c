//
// file.c
// loading code and data from files
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"

extern char **environ;

PN potion_file_with(Potion *P, PN cl, PN self, PN path) {
  return PN_NIL;
}

PN potion_lobby_read(Potion *P, PN cl, PN self) {
  const int linemax = 1024;
  char line[linemax];
  if (fgets(line, linemax, stdin) != NULL)
    return potion_str(P, line);
  return PN_NIL;
}

void potion_file_init(Potion *P) {
  // PN file_vt = PN_VTABLE(PN_TFILE);
  char **env = environ, *key;
  PN pe = potion_table_empty(P);
  while (*env != NULL) {
    for (key = *env; *key != '='; key++);
    potion_table_put(P, PN_NIL, pe, potion_str2(P, *env, key - *env),
      potion_str(P, key + 1));
    env++;
  }
  potion_send(P->lobby, PN_def, potion_str(P, "Env"), pe);
  potion_method(P->lobby, "read", potion_lobby_read, 0);
}
