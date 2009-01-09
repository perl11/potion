//
// contrib.c
// stuff written by other folks, seen on blogs, etc. 
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//
// wonderful utf-8 counting trickery
// by colin percival
//
// http://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
//
#define ONEMASK ((size_t)(-1) / 0xFF)

size_t
potion_cp_strlen_utf8(const char * _s)
{
  const char * s;
  size_t count = 0;
  size_t u;
  unsigned char b;

  for (s = _s; (uintptr_t)(s) & (sizeof(size_t) - 1); s++) {
    b = *s;
    if (b == '\0') goto done;
    count += (b >> 7) & ((~b) >> 6);
  }

  for (; ; s += sizeof(size_t)) {
    __builtin_prefetch(&s[256], 0, 0);
    u = *(size_t *)(s);
    if ((u - ONEMASK) & (~u) & (ONEMASK * 0x80)) break;
    u = ((u & (ONEMASK * 0x80)) >> 7) & ((~u) >> 6);
    count += (u * ONEMASK) >> ((sizeof(size_t) - 1) * 8);
  }

  for (; ; s++) {
    b = *s;
    if (b == '\0') break;
    count += (b >> 7) & ((~b) >> 6);
  }
done:
  return ((s - _s) - count);
}

#ifdef __MINGW32__
#include <windows.h>
#include <sys/unistd.h>

void *mingw_mmap(size_t length)
{ 
  void *start;
  HANDLE handle = CreateFileMapping(0, NULL, PAGE_EXECUTE_READWRITE,
    0, length, NULL);
 
  if (handle == NULL)
    fprintf(stderr, "** mingw_mmap failed");
  
  start = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, length);
  CloseHandle(handle);
  
  return start;
}
#endif
