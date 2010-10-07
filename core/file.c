//
// file.c
// loading code and data from files
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "potion.h"
#include "internal.h"
#include "table.h"

extern char **environ;

PN potion_file_new(Potion *P, PN cl, PN self, PN path, PN modestr) {
  int fd;
  mode_t mode;
  if (strcmp(PN_STR_PTR(modestr), "r") == 0) {
    mode = O_RDONLY;
  } else if (strcmp(PN_STR_PTR(modestr), "r+") == 0) {
    mode = O_RDWR;
  } else if (strcmp(PN_STR_PTR(modestr), "w") == 0) {
    mode = O_WRONLY | O_TRUNC | O_CREAT;
  } else if (strcmp(PN_STR_PTR(modestr), "w+") == 0) {
    mode = O_RDWR | O_TRUNC | O_CREAT;
  } else {
    // invalid mode
    return PN_NIL;
  }
  if ((fd = open(PN_STR_PTR(path), mode)) == -1) {
    perror("open");
    // TODO: error
    return PN_NIL;
  }
  ((struct PNFile *)self)->fd = fd;
  ((struct PNFile *)self)->path = path;
  ((struct PNFile *)self)->mode = mode;
  return self;
}

PN potion_lobby_read(Potion *P, PN cl, PN self) {
  const int linemax = 1024;
  char line[linemax];
  if (fgets(line, linemax, stdin) != NULL)
    return potion_str(P, line);
  return PN_NIL;
}

void potion_file_init(Potion *P) {
  PN file_vt = PN_VTABLE(PN_TFILE);
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
  
  potion_type_constructor_is(file_vt, PN_FUNC(potion_file_new, "path=S,mode=S"));
}
