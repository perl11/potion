/** \file lib/buffile.c
  PNBufFile class for buffered stream FILE* IO

  fgets, fopen, fscanf, fprintf, fread
  \seealso http://stackoverflow.com/questions/1658476/c-fopen-vs-open

 (c) 2013 perl11.org */
#define __USE_XOPEN2K8
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <errno.h>
#include "p2.h"

#if defined(__linux__) || defined(__CYGWIN__)
#define HAVE_FMEMOPEN
#endif

struct PNBufFile {
  PN_OBJECT_HEADER;
  PN_SIZE siz;
  FILE * file;
  PN     path;
};
typedef vPN(BufFile) pn_ffile;
const int BufFileSize = sizeof(struct PNBufFile) - sizeof(struct PNData);

/**\memberof PNBufFile
  opens a buffered stream
  \param path PNString
  \param modestr PNString r,r+,w,w+,a,a+
  \returns opened PNBufFile or PNError */
PN potion_buffile_fopen(Potion *P, PN cl, PN ign, PN path, PN modestr) {
  FILE *file;
  struct PNBufFile *self;
  if (!(file = fopen(PN_STR_PTR(path), PN_STR_PTR(modestr))))
    return potion_io_error(P, "open");
  self = (struct PNBufFile *)potion_data_alloc(P, sizeof(struct PNBufFile));
  self->siz = BufFileSize;
  self->file = file;
  self->path = path;
  return (PN)self;
}
/**\memberof Lobby
  create a temporary file with fdopen for w+
  The file shall be automatically deleted when all references to the file are closed.
  \return a new PNBufFile */
PN potion_buffile_tmpfile(Potion *P, PN cl, PN ign) {
  struct PNBufFile *self;
  self = (struct PNBufFile *)potion_data_alloc(P, sizeof(struct PNBufFile));
  self->siz = BufFileSize;
  self->file = tmpfile();
  if (!self->file)
    return potion_io_error(P, "tmpfile");
  self->path = PN_NIL;
  return (PN)self;
}
/**\memberof PNBufFile
  \c fdopen associate a stream with the existing file descriptor, fd
  \see potion_buffile_with_fd()
  \param fd PN_NUM
  \param modestr PNString r,r+,w,w+,a,a+
  \returns PNBufFile or PNError */
PN potion_buffile_fdopen(Potion *P, PN cl, pn_ffile self, PN fd, PN modestr) {
  FILE *file;
  if (!(file = fdopen(PN_INT(fd), PN_STR_PTR(modestr))))
    return potion_io_error(P, "fdopen");
  self->siz = BufFileSize;
  self->file = file;
  self->path = PN_NIL;
  return (PN)self;
}

/**\memberof PNBufFile
  \c freopen opens the file whose name is the string pointed to by path and associates the stream
  pointed to by the stream argument with it.  The original file (if it exists) is closed. 
  The primary use of the freopen() function is to change the file associated  with
  a standard text stream (stderr, stdin, or stdout).
  \param path PNString
  \param modestr PNString r,r+,w,w+,a,a+
  \param stream PNBufFile
  \returns PNBufFile or PNError */
PN potion_buffile_freopen(Potion *P, PN cl, pn_ffile self, PN path, PN modestr, pn_ffile stream) {
  FILE *file;
  if ((PN_TYPE(stream) != PN_TUSER) ||
     !(file = freopen(PN_STR_PTR(path), PN_STR_PTR(modestr), stream->file))) {
    return potion_io_error(P, "freopen");
  }
  self->siz = BufFileSize;
  self->file = file;
  self->path = stream->path;
  return (PN)self;
}

#ifdef HAVE_FMEMOPEN
/**\memberof PNBytes
  \c fmemopen opens a stream that permits the access specified by mode. 
  The stream allows I/O to be performed on the string or memory buffer
  pointed to by buf.
  \param buf PNBytes
  \param modestr PNString r,r+,w,w+,a,a+
  \see `man 3 fmemopen`
  \returns PNBufFile or PNError */
PN potion_buffile_fmemopen(Potion *P, PN cl, PN buf, PN modestr) {
  FILE *file;
  struct PNBufFile *self;
  if (!(file = fmemopen(PN_STR_PTR(buf), PN_STR_LEN(buf), PN_STR_PTR(modestr))))
    return potion_io_error(P, "fmemopen");
  self = (struct PNBufFile *)potion_data_alloc(P, sizeof(struct PNBufFile));
  self->siz = BufFileSize;
  self->file = file;
  self->path = PN_NIL;
  return (PN)self;
}
#endif

/**\memberof PNBufFile
  \c flush and \c close a PNBufFile.
  \return PNNumber (0 or EOF) */
PN potion_buffile_fclose(Potion *P, PN cl, pn_ffile self) {
  return PN_NUM(fclose(self->file));
}
/**\memberof PNBufFile
  \c read the next character from a PNBufFile
  \return PNNumber */
PN potion_buffile_fgetc(Potion *P, PN cl, pn_ffile self) {
  return PN_NUM(fgetc(self->file));
}
/**\memberof PNBufFile
  read next line from PNBufFile, max 1024
  \see potion_lobby_read() and potion_file_read()
  \return PNBytes or PN_NIL */
PN potion_buffile_fgets(Potion *P, PN cl, pn_ffile self) {
  const int linemax = 1024;
  char line[linemax];
  if (fgets(line, linemax, self->file) != NULL)
    return potion_byte_str(P, line);
  return PN_NIL;
}
/**\memberof PNBufFile
  \c read nitems of size from the stream into the PNBytes buf
  \param buf PNBytes
  \param size PNNumber
  \param nitems PNNumber
  \return PNNumber of read items or PNError */
PN potion_buffile_fread(Potion *P, PN cl, pn_ffile self, PN buf, PN size, PN nitems) {
  int r = fread(PN_STR_PTR(buf), PN_INT(size), PN_INT(nitems), self->file);
  if (r < PN_INT(nitems))
    return potion_io_error(P, "fread");
  return PN_NUM(r);
}
/**\memberof PNBufFile
  \c write nitems of size starting at the pointer pointing to buf to the stream.
  \param buf PNBytes or PNString
  \param size PNNumber or if PN_NIL the length of buf
  \param nitems PNNumber, default 1
  \return PNNumber of written items or PNError */
PN potion_buffile_fwrite(Potion *P, PN cl, pn_ffile self, PN buf, PN size, PN nitems) {
  if (!size && (!nitems || PN_INT(nitems) == 1)) {
    size = potion_send(buf, PN_STR("length"));
    nitems = PN_NUM(1);
  }
  switch (PN_TYPE(buf)) {
    case PN_TSTRING:
    case PN_TBYTES: break;
    case PN_NIL:    return PN_NIL;
    default: return potion_type_error(P, buf);
  }
  int r = fwrite(PN_STR_PTR(buf), PN_INT(size), PN_INT(nitems), self->file);
  if (r < PN_INT(nitems))
    return potion_io_error(P, "fwrite");
  return PN_NUM(r);
}
/**\memberof PNBufFile
  \c write the byte (0-255) to a PNBufFile
  \param byte PNNumber
  \return PNNumber */
PN potion_buffile_fputc(Potion *P, PN cl, pn_ffile self, PN byte) {
  return PN_NUM(fputc(PN_INT(byte), self->file));
}
/**\memberof PNBufFile
  \c write line to PNBufFile
  \param str PNString or PNBytes
  \return PNNumber or PNError */
PN potion_buffile_fputs(Potion *P, PN cl, pn_ffile self, PN str) {
  int r;
  if (!(r=fputs(PN_STR_PTR(str), self->file)))
    return potion_io_error(P, "fputs");
  return PN_NUM(r);
}
/**\memberof PNBufFile
   \c "fflush"
   \return true or PNError */
PN potion_buffile_fflush(Potion *P, PN cl, pn_ffile self) {
  if (fflush(self->file))
    return potion_io_error(P, "fflush");
  return PN_TRUE;
}
/**\memberof PNBufFile
   \c "fseek" set the file-position indicator for the stream.
  \param offset PNNumber
  \param whence PNNumber
  \return true or PNError */
PN potion_buffile_fseek(Potion *P, PN cl, pn_ffile self, PN offset, PN whence) {
  if (fseek(self->file, PN_INT(offset), PN_INT(whence)))
    return potion_io_error(P, "fseek");
  return PN_TRUE;
}
/**\memberof PNBufFile
   \c "ftell" returns the file-position indicator for the stream measured in bytes from the
   beginning of the file.
  \return PNNumber */
PN potion_buffile_ftell(Potion *P, PN cl, pn_ffile self) {
  long r = ftell(self->file);
  if (r == -1)
    return potion_io_error(P, "ftell");
  return PN_NUM(r);
}
/**\memberof PNBufFile
   \c "feof" tests the end-of-file indicator for the stream.
   \return true or false */
PN potion_buffile_feof(Potion *P, PN cl, pn_ffile self) {
  int r = feof(self->file);
  return r ? PN_TRUE : PN_FALSE;
}
/**\memberof PNBufFile
   \c "fileno" \returns its integer file descriptor. */
PN potion_buffile_fileno(Potion *P, PN cl, pn_ffile self) {
  return PN_NUM(fileno(self->file));
}
/**\memberof PNBufFile
   \c "unlink" the PNBufFile. close it if open.
   \return true or nil */
PN potion_buffile_unlink(Potion *P, PN cl, pn_ffile self) {
  if (fileno(self->file) != -1) fclose(self->file);
  if (!self->path || unlink(PN_STR_PTR(self->path))) {
    return potion_io_error(P, "unlink");
  }
  return PN_TRUE;
}
/**\memberof PNBufFile
   acquire for a thread ownership of a PNBufFile object */
PN potion_buffile_flockfile(Potion *P, PN cl, pn_ffile self) {
  flockfile(self->file); return PN_TRUE;
}
/**\memberof PNBufFile
   non-blocking version of \see potion_buffile_flockfile().
  \returns 0 for success */
PN potion_buffile_ftrylockfile(Potion *P, PN cl, pn_ffile self) {
  return PN_NUM(ftrylockfile(self->file));
}
/**\memberof PNBufFile
   relinquish the ownership granted to the thread of a PNBufFile object */
PN potion_buffile_funlockfile(Potion *P, PN cl, pn_ffile self) {
  funlockfile(self->file); return PN_TRUE;
}
/**\memberof PNBufFile
  \c "fprintf" to file.
  \param obj any
  \return PN_NIL */
#if 0
PN potion_buffile_fprintf(Potion *P, PN cl, pn_ffile self, PN fmt, ...) {
  return fprintf(self->file, potion_send(obj, PN_string));
}
#endif
/**\memberof PNBufFile
  \c "string" method. some internal descr */
PN potion_buffile_string(Potion *P, PN cl, pn_ffile self) {
  int fd = fileno(self->file), rv;
  char *buf;
  PN str;
  if (self->path != PN_NIL && fd != -1) {
    rv = asprintf(&buf, "<buffile %s fd: %d>", PN_STR_PTR(self->path), fd);
  } else if (fd != -1) {
    rv = asprintf(&buf, "<buffile %p fd: %d>", self->file, fd);
  } else {
    rv = asprintf(&buf, "<closed buffile %p>", self->file);
  }
  if (rv == -1) potion_allocation_error();
  str = potion_str(P, buf);
  free(buf);
  return str;
}

void Potion_Init_buffile(Potion *P) {
  PN ffile_vt = potion_type_new2(P, PN_TUSER, PN_VTABLE(PN_TFILE), PN_STR("BufFile"));
  potion_type_constructor_is(ffile_vt, PN_FUNC(potion_buffile_fopen, "path=S,mode=S"));
  //potion_class_method(ffile_vt, "fd", potion_buffile_fdopen, "fd=N,mode=S");
  potion_method(P->lobby, "fopen", potion_buffile_fopen, "path=S,mode=S");
  potion_method(ffile_vt, "fdopen", potion_buffile_fdopen, "fd=N,mode=S");
  potion_method(ffile_vt, "freopen", potion_buffile_freopen, "path=S,mode=S,buffile=o");
#ifdef HAVE_FMEMOPEN
  potion_method(PN_VTABLE(PN_TBYTES), "fmemopen", potion_buffile_fmemopen, "mode=S");
#endif
  potion_method(ffile_vt, "close", potion_buffile_fclose, 0);
  potion_method(ffile_vt, "fread",  potion_buffile_fread, "buf=S,size=N,n:=1");
  potion_method(ffile_vt, "write", potion_buffile_fwrite, "buf=S|size=N,n:=1");
  //potion_method(ffile_vt, "fprintf", potion_buffile_fprintf, "fmt=S|...");
  potion_method(ffile_vt, "fgetc", potion_buffile_fgetc, 0);
  potion_method(ffile_vt, "fgets", potion_buffile_fgets, 0);
  potion_method(ffile_vt, "fputc", potion_buffile_fputc, "byte=N");
  potion_method(ffile_vt, "fputs", potion_buffile_fputs, "str=S");
  potion_method(ffile_vt, "fflush", potion_buffile_fflush, 0);
  potion_method(ffile_vt, "fseek", potion_buffile_fseek, "offset=N, whence=N");
  potion_method(ffile_vt, "ftell", potion_buffile_ftell, 0);
  potion_method(ffile_vt, "feof", potion_buffile_feof, 0);
  potion_method(ffile_vt, "fileno", potion_buffile_fileno, 0);
  potion_method(ffile_vt, "flockfile", potion_buffile_flockfile, 0);
  potion_method(ffile_vt, "ftrylockfile", potion_buffile_ftrylockfile, 0);
  potion_method(ffile_vt, "funlockfile", potion_buffile_funlockfile, 0);
  potion_method(ffile_vt, "unlink", potion_buffile_unlink, 0);
  potion_method(ffile_vt, "string", potion_buffile_string, 0);
  potion_method(P->lobby, "tmpfile", potion_buffile_tmpfile, 0);
}
