# ~ building potion ~

Normally

    $ make

To build with debugging symbols

    $ make DEBUG=1

To build without JIT

    $ make JIT=0

Lastly, to verify your build

    $ make test

## ~ the latest potion ~

To build the bleeding edge, you will need
GNU make, binutils and gcc or clang.

    $ git clone --branch master git://github.com/perl11/potion.git
    $ cd potion
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

The easiest way to do this, actually, is on Linux.

On Ubuntu, if you have MinGW installed,

    $ make CC=i586-mingw32msvc-gcc

## ~ building on bsd ~

BSD make is not supported.
You can either install gnu make (gmake)

    $ sudo port install gmake

or try to merge 'master' with the branch 'bsd'

    $ git merge bsd
    ... resolve conflicts, or not

and remove then -ldl from config.inc LIBS

    $ make
    $ cat config.inc | sed 's, -ldl,,' > config.inc
    $ make

## ~ building with a strict C++ compiler ~

potion does not support strict C++ compilers.

Either add a C dialect to CC in config.inc (i.e. -std=c89),

    g++ --help=C; clang++ -x C -std=gnu89

or try to merge with the branch 'p2-c++'.
