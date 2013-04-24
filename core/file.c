///\file file.c
/// PNFile class for file descriptors
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "potion.h"
#include "internal.h"
#include "table.h"

#ifdef __APPLE__
# include <crt_externs.h>
# undef environ
# define environ (*_NSGetEnviron())
#else
  extern char **environ;
#endif

typedef vPN(File) pn_file;

///\memberof PNFile
/// constructor method. opens a file with 0755 and returns the created PNFile
///\param path PNString
///\param modestr PNString r,r+,w,w+,a,a+
///\return self or PN_NIL
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
  } else if (strcmp(PN_STR_PTR(modestr), "a") == 0) {
    mode = O_WRONLY | O_CREAT | O_APPEND;
  } else if (strcmp(PN_STR_PTR(modestr), "a+") == 0) {
    mode = O_RDWR | O_CREAT | O_APPEND;
  } else {
    // invalid mode
    return PN_NIL;
  }
  if ((fd = open(PN_STR_PTR(path), mode, 0755)) == -1) {
    perror("open");
    // TODO: error, perm
    return PN_NIL;
  }
  ((struct PNFile *)self)->fd = fd;
  ((struct PNFile *)self)->path = path;
  ((struct PNFile *)self)->mode = mode;
  return self;
}

///\memberof PNFile
/// "fd" class method.
///\param fd PNNumber
///\return a new PNFile object for the already opened file descriptor (sorry, empty path).
PN potion_file_with_fd(Potion *P, PN cl, PN self, PN fd) {
  struct PNFile *file = (struct PNFile *)potion_object_new(P, PN_NIL, PN_VTABLE(PN_TFILE));
  file->fd = PN_INT(fd);
  file->path = PN_NIL;
#ifdef F_GETFL
  file->mode = fcntl(file->fd, F_GETFL) | O_ACCMODE;
#else
  struct stat st;
  if (fstat(file->fd, &st) == -1) perror("fstat");
  file->mode = st.st_mode;
#endif
  return (PN)file;
}

///\memberof PNFile
/// "close" method.
///\return PN_NIL
PN potion_file_close(Potion *P, PN cl, pn_file self) {
  close(self->fd);
  self->fd = -1;
  return PN_NIL;
}

///\memberof PNFile
/// "read" method.
///\param n PNNumber
///\return n PNBytes
PN potion_file_read(Potion *P, PN cl, pn_file self, PN n) {
  n = PN_INT(n);
  char buf[n];
  int r = read(self->fd, buf, n);
  if (r == -1) {
    perror("read");
    // TODO: error
    return PN_NUM(-1);
  } else if (r == 0) {
    return PN_NIL;
  }
  return potion_byte_str2(P, buf, r);
}

///\memberof PNFile
/// "write" method.
///\param str PNString
///\return PNNumber written bytes or PN_NIL
PN potion_file_write(Potion *P, PN cl, pn_file self, PN str) {
  int r = write(self->fd, PN_STR_PTR(str), PN_STR_LEN(str));
  if (r == -1) {
    perror("write");
    // TODO: error
    return PN_NIL;
  }
  return PN_NUM(r);
}

///\memberof PNFile
/// "string" method. some internal descr
PN potion_file_string(Potion *P, PN cl, pn_file self) {
  int fd = self->fd, rv;
  char *buf;
  PN str;
  if (self->path != PN_NIL && fd != -1) {
    rv = asprintf(&buf, "<file %s fd: %d>", PN_STR_PTR(self->path), fd);
  } else if (fd != -1) {
    rv = asprintf(&buf, "<file fd: %d>", fd);
  } else {
    rv = asprintf(&buf, "<closed file>");
  }
  if (rv == -1) potion_allocation_error();
  str = potion_str(P, buf);
  free(buf);
  return str;
}

/// memberof Lobby
/// global "read" method, read next line from stdin via fgets()
///\return PNString or or PN_NIL
PN potion_lobby_read(Potion *P, PN cl, PN self) {
  const int linemax = 1024;
  char line[linemax];
  if (fgets(line, linemax, stdin) != NULL)
    return potion_str(P, line);
  return PN_NIL;
}

/// set Env global
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
  potion_class_method(file_vt, "fd", potion_file_with_fd, "fd=N");
  potion_method(file_vt, "string", potion_file_string, 0);
  potion_method(file_vt, "close", potion_file_close, 0);
  potion_method(file_vt, "read", potion_file_read, "n=N");
  potion_method(file_vt, "write", potion_file_write, "str=S");
}
