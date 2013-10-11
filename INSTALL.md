# ~ building potion ~

Normally

    $ make

To build with debugging symbols

    $ make DEBUG=1

To build without JIT

    $ make JIT=0

Lastly, to verify your build

    $ make test

On a typical GNU make < 4.0 sometimes the `test/classes/creatures/` fails.
Try `make -j1 test` or gmake-4.0 then.

## ~ the latest potion ~

To build the bleeding edge, you will need
GNU make, binutils and clang or gcc.
clang produces better code than gcc, but is harder to debug.

    $ git clone git://github.com/perl11/potion.git
    $ cd potion
    $ git submodule update --init
    $ make

## ~ installing ~

    $ sudo make install

## ~ building on windows ~

Potion's win32 binaries are built using MinGW.
<http://mingw.org/>

It's a bit hard to setup mingw and gmake on Windows.
I usually run a shell under Cygwin and add MinGW
to my $PATH.

Once that's all done,

    $ make

The easiest way to do this, actually, is on Linux or Darwin.
On Ubuntu, if you have MinGW installed,

    $ make clean; make core/syntax.c
    $ make config CC=i686-w64-mingw32-gcc CROSS=1
    $ touch core/syntax.c
    $ make && make dist

This will first create a native greg and core/syntax.c,
sets CROSS=1 and cross-compiles with the given CC.
See tools/mk-release.sh.
make test will not work, you need to copy a make dist tarball
to the machine and test it there.

win64 is not supported yet. It uses a slighlty different ABI,
cygwin64 misses pthread\_barrier\_t.
and x86_64-w64-mingw32-gcc fails by creating a wrong `config.h`

## ~ building on bsd ~

BSD make is not supported.
You can either install gnu make (gmake)

    $ sudo port install gmake

or try `./configure` which creates a special BSD `config.mk`

or try to merge 'master' with the branch 'bsd'

    $ git merge bsd
    ... resolve conflicts, or not

## ~ building with a strict C++ compiler ~

potion does not support strict C++ compilers.

Either add a C dialect to CC in config.inc (i.e. -std=c89),

    g++ --help=C; clang++ -x C -std=gnu89

or try to merge with the branch 'p2-c++'.

## ~ creating documentation ~

This is required for `make dist` and release admins.
You'll need:

    redcloth to convert .textile to html,
    doxygen (1.8 or 1.9), and
    GNU global for gtags and htags

On windows et al.: `gem install RedCloth`
