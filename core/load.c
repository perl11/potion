//
// load.c
// loading of external code
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include "potion.h"
#include "internal.h"
#include "table.h"

void potion_load_code(Potion *P, const char *filename) {
  PN buf, code;
  int fd = -1;
  struct stat stats;
  if (stat(filename, &stats) == -1) {
    fprintf(stderr, "** %s does not exist.", filename);
    goto done;
  }
  fd = open(filename, O_RDONLY | O_BINARY);
  if (fd == -1) {
    fprintf(stderr, "** could not open %s. check permissions.", filename);
    goto done;
  }
  buf = potion_bytes(P, stats.st_size);
  if (read(fd, PN_STR_PTR(buf), stats.st_size) == stats.st_size) {
    PN_STR_PTR(buf)[stats.st_size] = '\0';
    code = potion_source_load(P, PN_NIL, buf);
    if (!PN_IS_PROTO(code)) {
      potion_run(P, potion_send(
        potion_parse(P, buf), PN_compile, potion_str(P, filename), PN_NIL));
    }
  } else {
    fprintf(stderr, "** could not read entire file: %s.", filename);
  }
  
done:
  if (fd != -1)
    close(fd);
}

static PN potion_initializer_name(Potion *P, const char *filename, PN_SIZE len) {
  PN_SIZE ext_name_len = 0;
  char *allocated_str, *ext_name;
  PN func_name = potion_byte_str(P, "Potion_Init_");
  while (*(filename + ++ext_name_len) != '.' && ext_name_len <= len);
  allocated_str = ext_name = malloc(ext_name_len + 1);
  if (allocated_str == NULL) {
    // TODO: fatal error
    fprintf(stderr, "** Couldn't allocate memory.\n");
    exit(1);
  }
  strncpy(ext_name, filename, ext_name_len);
  ext_name[ext_name_len] = '\0';
  ext_name += ext_name_len;
  while (*--ext_name != '/' && ext_name >= allocated_str);
  ext_name++;
  pn_printf(P, func_name, "%s", ext_name);
  free(allocated_str);
  return func_name;
}

void potion_load_dylib(Potion *P, const char *filename) {
  void *handle = dlopen(filename, RTLD_LAZY);
  void (*func)(Potion *);
  char *err;
  if (handle == NULL) {
    // TODO: error
    fprintf(stderr, "** error loading %s: %s\n", filename, dlerror());
    return;
  }
  func = dlsym(handle, PN_STR_PTR(
    potion_initializer_name(P, filename, strlen(filename))));
  err = dlerror();
  if (err != NULL) {
    fprintf(stderr, "** error loading %s: %s\n", filename, err);
    dlclose(handle);
    return;
  }
  func(P);
}

PN potion_load(Potion *P, PN cl, PN self, PN file) {
  char *filename = PN_STR_PTR(file),
    *file_ext = filename + PN_STR_LEN(file);
  while (*--file_ext != '.' && file_ext >= filename);
  if (file_ext++ != filename) {
    if (strcmp(file_ext, "pn") == 0)
      potion_load_code(P, filename);
    else if (strcmp(file_ext, "so") == 0 ||
             strcmp(file_ext, "dylib") == 0
            )
      potion_load_dylib(P, filename);
    else
      ; // TODO: error
  } else {
    // ...
  }
  
  return PN_NIL;
}

void potion_loader_init(Potion *P) {
  potion_method(P->lobby, "load", potion_load, "file=S");
}