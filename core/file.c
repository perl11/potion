/** \file file.c
  PNFile class for unbuffered blocking file descriptor IO.

  Only raw and fast POSIX open,read,write,seek calls on fd.
  fgets (aka readline) is only supported on stdin via the \c "read" method.
  \see http://stackoverflow.com/questions/1658476/c-fopen-vs-open

  \see the \c buffile library (PNBufFile) for buffered io via FILE*
    for fopen, fscanf, fprintf, fread, fgets see there.
  \see the aio library (PNAio) for async non-blocking io, via libuv bindings.

 (c) 2008 why the lucky stiff, the freelance professor */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
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

typedef struct PNFile * volatile pn_file;

/**\memberof PNFile
  constructor method. opens a file with 0755 and returns the created PNFile
 \param path PNString
 \param modestr PNString r,r+,w,w+,a,a+
 \return self or PN_NIL */
PN potion_file_new(Potion *P, PN cl, PN self, PN path, PN modestr) {
  struct PNFile * file = (PN)PN_ALLOC_N(PN_TFILE, struct PNFile, 0 * sizeof(PN));
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
  if ((fd = open(PN_STR_PTR(path), mode, 0755)) == -1)
    return potion_io_error(P, "open");
  file->fd = fd;
  file->path = path;
  file->mode = mode;
  return (PN)file;
}

/**\memberof PNFile
  \c "fd" class method.
  \param fd PNInteger
  \return a new PNFile object for the already opened file descriptor (sorry, empty path). */
PN potion_file_with_fd(Potion *P, PN cl, PN self, PN fd) {
  struct PNFile *file = PN_ALLOC_N(PN_TFILE, struct PNFile, 0 * sizeof(PN));
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

/**\memberof PNFile
  \c "close" the file
  \return PN_NIL */
PN potion_file_close(Potion *P, PN cl, pn_file self) {
  int retval;
  while (retval = close(self->fd), retval == -1 && errno == EINTR) ;
  self->fd = -1;
  return PN_NIL;
}

/**\memberof PNFile
 \c "read" n PNBytes from the file
 \param n PNInteger
 \return n PNBytes or nil or PNError */
PN potion_file_read(Potion *P, PN cl, pn_file self, PN n) {
  n = PN_INT(n);
  char buf[n];
  int r = read(self->fd, buf, n);
  if (r == -1) {
    return potion_io_error(P, "read");
    //perror("read");
    // TODO: error
    //return PN_NUM(-1);
  } else if (r == 0) {
    return PN_NIL;
  }
  return potion_byte_str2(P, buf, r);
}

/**\memberof PNFile
 \c "write" a binary representation of obj to the file handle.
 \param obj PNString, PNBytes, PNInteger (long or double), PNBoolean (char 0 or 1)
 \return PNInteger written bytes or PN_NIL */
PN potion_file_write(Potion *P, PN cl, pn_file self, PN obj) {
  long len = 0;
  char *ptr = NULL;
  //TODO: maybe extract ptr+len to seperate function
  if (!PN_IS_PTR(obj)) {
    if (!obj) return PN_NIL; //silent
    else if (PN_IS_INT(obj)) {
      long tmp = PN_NUM(obj); len = sizeof(tmp); ptr = (char *)&tmp;
    }
    else if (PN_IS_BOOL(obj)) {
      char tmp = (obj == PN_TRUE) ? 1 : 0; len = 1; ptr = (char *)&tmp;
    }
    else {
      assert(0 && "Invalid primitive type");
    }
  } else {
    switch (PN_TYPE(obj)) {
      case PN_TSTRING: len = PN_STR_LEN(obj); ptr = PN_STR_PTR(obj); break;
      case PN_TBYTES:  len = potion_send(obj, PN_STR("length")); ptr = PN_STR_PTR(obj); break;
      case PN_TNUMBER: {
        double tmp = PN_DBL(obj); len = sizeof(tmp); ptr = (char *)&tmp;
        break;
      }
      default: return potion_type_error(P, obj);
    }
  }
  int r = write(self->fd, ptr, len);
  if (r == -1)
    return potion_io_error(P, "write");
  return PN_NUM(r);
}

/**\memberof PNFile
  \c "print" a stringification of any object to the filehandle.
  Note that \c write prints the binary value of the object.
  \param obj any
  \return "" or PNError */
PN potion_file_print(Potion *P, PN cl, pn_file self, PN obj) {
  PN r = potion_file_write(P, cl, self, potion_send(obj, PN_string));
  return PN_IS_INT(r) ? PN_STR0 : r;
}

/**\memberof PNFile
   "string" method. some internal descr */
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

/**\memberof Lobby
  global "read" method, read next line from stdin via fgets()
  \return PNString or or PN_NIL */
PN potion_lobby_read(Potion *P, PN cl, PN self) {
  char line[1024];
  if (fgets(line, 1024, stdin) != NULL)
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
    potion_table_put(P, PN_NIL, pe, PN_STRN(*env, key - *env),
      potion_str(P, key + 1));
    env++;
  }
  potion_send(P->lobby, PN_def, PN_STRN("Env", 3), pe);
  potion_method(P->lobby, "read", potion_lobby_read, 0);
  
  potion_type_constructor_is(file_vt, PN_FUNC(potion_file_new, "path=S,mode=S"));
  potion_class_method(file_vt, "fd", potion_file_with_fd, "fd=N");
  potion_method(file_vt, "string", potion_file_string, 0);
  potion_method(file_vt, "close", potion_file_close, 0);
  potion_method(file_vt, "read", potion_file_read, "n=N");
  potion_method(file_vt, "write", potion_file_write, "str=S");
  potion_method(file_vt, "print", potion_file_print, "obj=o");
}
