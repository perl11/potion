ucontext on x86_64 in inline assembly as in potion, 
on 32 bit with calls to getcontext.
nicer coro API than ours, esp. yield (long)

https://github.com/lpereira/lwan.git

More tricks from lwan.h
-----------------------
static ALWAYS_INLINE unsigned long
_has_zero_byte(unsigned long n)
{
    return ((n - 0x01010101UL) & ~n) & 0x80808080UL;
}

static ALWAYS_INLINE unsigned long
_is_space(char ch)
{
    return _has_zero_byte((0x1010101 * ch) ^ 0x090a0d20);
}

static ALWAYS_INLINE char *
_ignore_leading_whitespace(char *buffer)
{
    while (*buffer && _is_space(*buffer))
        buffer++;
    return buffer;
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define MULTICHAR_CONSTANT(a,b,c,d) ((int32_t)((a) | (b) << 8 | (c) << 16
# | (d) << 24))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define MULTICHAR_CONSTANT(d,c,b,a) ((int32_t)((a) | (b) << 8 | (c) << 16
# | (d) << 24))
#else
#  error Unknown byteorder
#endif
#define MULTICHAR_CONSTANT_L(a,b,c,d) (MULTICHAR_CONSTANT(a,b,c,d) | 0x20202020)
#define STRING_SWITCH(s) switch (*((int32_t *)(s)))
#define STRING_SWITCH_L(s) switch (*((int32_t *)(s)) | 0x20202020)
EXT_JPG = MULTICHAR_CONSTANT_L('.','j','p','g'),
HTTP_HDR_CONNECTION        = MULTICHAR_CONSTANT_L('C','o','n','n'),
...

#define ATOMIC_READ(V)          (*(volatile typeof(V) *)&(V))
#define ATOMIC_AAF(P, V)        (__sync_add_and_fetch((P), (V)))
#define ATOMIC_INC(V)           ATOMIC_AAF(&(V), 1)
#define ATOMIC_DEC(V)           ATOMIC_AAF(&(V), -1)
#define ATOMIC_BITWISE(P, O, V) (__sync_##O##_and_fetch((P), (V)))
