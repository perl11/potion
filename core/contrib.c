/** \file contrib.c
  stuff written by other folks, seen on blogs, etc.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "config.h"

#define ONEMASK ((size_t)(-1) / 0xFF)

/// wonderful utf-8 counting trickery
/// by colin percival
///
/// http://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
///
size_t
potion_cp_strlen_utf8(const char * _s)
{
  const char * s;
  size_t count = 0;
  size_t u;
  unsigned char b;

  /* Handle any initial misaligned bytes. */
  for (s = _s; (uintptr_t)(s) & (sizeof(size_t) - 1); s++) {
    b = *s;
    if (b == '\0') goto done;
    count += (b >> 7) & ((~b) >> 6); /* NOT the first byte of a character? */
  }
  /* Handle complete blocks, vectorizable */
  for (; ; s += sizeof(size_t)) {
    __builtin_prefetch(&s[256], 0, 0);
    u = *(size_t *)(s); /* Grab 4 or 8 bytes of UTF-8 data. */
    if ((u - ONEMASK) & (~u) & (ONEMASK * 0x80)) break; /* exit on \0 */
    /* count bytes which are NOT the first byte of a character.
       TODO: could use a lookup table */
    u = ((u & (ONEMASK * 0x80)) >> 7) & ((~u) >> 6);
    count += (u * ONEMASK) >> ((sizeof(size_t) - 1) * 8);
  }
  /* any left-over bytes. */
  for (; ; s++) {
    b = *s;
    if (b == '\0') break;
    /* Is this byte NOT the first byte of a character? */
    count += (b >> 7) & ((~b) >> 6); /* NOT the first byte of a character? */
  }
done:
  return ((s - _s) - count);
}

#ifdef __MINGW32__
#include <windows.h>
#include <sys/unistd.h>
#define PN_ALIGN(o, x)   (((((o) - 1) / (x)) + 1) * (x))

void *potion_mmap(size_t length, const char exec)
{ 
  void *mem = VirtualAlloc(NULL, length, MEM_COMMIT,
    exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
  if (mem == NULL) {
    /* One last attempt at the highest page.
       On Windows VirtualAlloc(NULL) sometimes fails due to Illegal System DLL Relocation at a reserved address. */
    SYSTEM_INFO SystemInfo;
    size_t high;
    int psz;
    GetSystemInfo(&SystemInfo);
    psz = SystemInfo.dwAllocationGranularity;
    high = (size_t)SystemInfo.lpMaximumApplicationAddress - PN_ALIGN(length, psz) + 1;
#ifdef DEBUG
    fprintf(stderr, "** potion_mmap(%ld%s) failed, try last page at 0x%x\n", length, exec ? ",exec" : "", high);
#endif
    mem = VirtualAlloc((void*)high, length, MEM_COMMIT,
                       exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
    if (mem == NULL) {
      fprintf(stderr, "** potion_mmap(%ld%s) failed\n", length, exec ? ",exec" : "");
    }
  }
  return mem;
}

int potion_munmap(void *mem, size_t len)
{
  return VirtualFree(mem, len, MEM_DECOMMIT) != 0 ? 0 : -1;
}

#else
#include <sys/mman.h>

void *potion_mmap(size_t length, const char exec)
{
  int prot = exec ? PROT_EXEC : 0;
  void *mem = mmap(NULL, length, prot|PROT_READ|PROT_WRITE,
    (MAP_PRIVATE|MAP_ANON), -1, 0);
  if (mem == MAP_FAILED) {
    fprintf(stderr, "** potion_mmap(%ld%s) failed\n", (long)length, exec ? ",exec" : "");
    return NULL;
  }
  return mem;
}

int potion_munmap(void *mem, size_t len)
{
  return munmap(mem, len);
}

#endif

#if POTION_WIN32
/// vasprintf from nokogiri
/// http://github.com/tenderlove/nokogiri
/// (written by Geoffroy Couprie)
int vasprintf (char **strp, const char *fmt, va_list ap)
{
  int len = vsnprintf (NULL, 0, fmt, ap) + 1;
  char *res = (char *)malloc((unsigned int)len);
  if (res == NULL)
      return -1;
  *strp = res;
  return vsnprintf(res, (unsigned int)len, fmt, ap);
}

/// asprintf from glibc
int
asprintf (char **string_ptr, const char *format, ...)
{
  va_list arg;
  int done;

  va_start (arg, format);
  done = vasprintf (string_ptr, format, arg);
  va_end (arg);

  return done;
}
#endif
