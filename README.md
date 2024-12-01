## ~ readme ~

         .ooo
          'OOOo
      ~ p ooOOOo tion ~
          .OOO
           oO      %% a little
             Oo    fast language.
            'O
             `
            (o)
        ___/ /
       /`    \
      /v^  `  ,
     (...v/v^/
      \../::/
       \/::/

[![Build Status](https://travis-ci.org/perl11/potion.svg)](https://travis-ci.org/perl11/potion) [![Coverity Status](https://scan.coverity.com/projects/6934/badge.svg)](https://scan.coverity.com/projects/perl11-potion) [perl11.org/potion/](http://perl11.org/potion/)

## ~ potion ~

Potion is an object- and mixin-oriented (traits)
language.

Its exciting points are:

 * Just-in-time compilation to x86 and x86-64
   machine code function pointers. This means
   she's a speedy one. Who integrates very
   well with C extensions.

   The JIT is turned on by default and is
   considered the primary mode of operation.

 * Intermediate bytecode format and VM. Load
   and dump code. Decent speed and cross-
   architecture. Heavily based on Lua's VM.

 * A lightweight generational GC, based on
   Basile Starynkevitch's work on Qish, with
   ~4ms per GC on average with < 100MB heaps.
   <http://starynkevitch.net/Basile/qishintro.html>

 * Bootstrapped "id" object model, based on
   Ian Piumarta's soda languages. This means
   everything in the language, including
   object allocation and interpreter state
   are part of the object model.
   (See COPYING for citations.)

 * Interpreter is thread-safe and reentrant.
   I hope this will facilitate coroutines,
   parallel interpreters and sandboxing.

 * Small. Under 10kloc. Right now we're like
   6,000 or something. Install sloccount
   and run: make sloc.

 * Reified AST and bytecode structures. This
   is very important to me. By giving access
   to the parser and compiler, it allows people
   to target other platforms, write code analysis
   tools and even fully bootstrapped VMs. I'm
   not as concerned about the Potion VM being
   fully bootstrapped, especially as it is tied
   into the JIT so closely.

 * Memory-efficient classes. Stored like C
   structs. (Although the method lookup table
   can be used like a hash for storing arbitrary
   data.)

 * The JIT is also used to speed up some other
   bottlenecks. For example, instance variable
   and method lookup tables are compiled into
   machine code.

However, some warnings:

 * Strings are immutable (like Lua) and byte
   arrays are used for I/O buffers.

 * Limited platform support for coroutines.
   This affects exceptions. I'm and feeling
   rather uninspired on the matter. Let's hear
   from you.

 * The parser is not GC safe. This affects eval.
   Do not waste too much memory inside eval.

 * No OS threads yet.


## ~ a whiff of potion ~

    5 times: "Odelay!" print.

Or,

    add = (x, y): x + y.
    add(2, 4) string print

Or,

    hello =
      "(x): ('hello ', x) print." eval
    hello ('world')


## ~ building and installing ~

    $ make

Look inside the file called INSTALL for options.


## ~ how it transpired ~

This isn't supposed to happen!

I started playing with Lua's internals and reading
stuff by Ian Piumarta and Nicolas Cannasse. And I,
well... I don't know how this happened!

Turns out making a language is a lovely old time,
you should try it. If you keep it small, fit the
VM and the parser and the stdlib all into 10k
lines, then it's no sweat.

To be fair, I'd been tinkering with the parser
for years, though.


## ~ the potion pledge ~

EVERYTHING IS AN OBJECT.
However, OBJECTS AREN'T EVERYTHING.

(And, incidentally, everything is a function.)


## ~ items to understand ~

1. A traditional object is a tuple of data
   and methods: (D, M).
   
   D is kept in the object itself.
   M is kept in classes.

2. In Potion, objects are just D.

3. Every object has an M.

4. But M can be altered, swapped,
   added to, removed from, whatever.

5. Objects do not have classes.
   The M is a mixin, a collection
   of methods.

Example: all strings have a "length"
method. This method comes with Potion.
It's in the String mixin.

6. You can swap out mixins for the span
   of a single source file.

Example: you could give all strings a
"backwards" method. But just for the
code inside your test.pn script.

7. You can re-mix for the span of a
   single closure.

To sum up:

EVERYTHING IS AN OBJECT.
EVEN MIXINS ARE OBJECTS.
AND, OF COURSE, CLOSURES ARE OBJECTS.

However, OBJECTS AREN'T EVERYTHING.
THEY ARE USELESS WITHOUT MIXINS.


## ~ unique ideas (to be implemented) ~

Potion does have a few unique features
underway.

* It is two languages in one.

The language itself is objects and closures.

    Number add = (x): self + x.

But it also includes a data language.

    app = [window (width=200, height=400)
      [button "OK", button "Cancel"]]
    
The code and data languages can be interleaved
over and over again. In a way, I'm trying to find
a middle ground between s-expressions and stuff like
E4X. I like that s-expressions are a very light data
syntax, but I like that E4X clearly looks like data.

When s-expressions appear in Lisp code, they look
like code. I think it is nice to distinguish the two.

* Deeply nested blocks can be closed quickly.
I don't like significant whitespace, personally.
But I don't like end end end end.

    say = (phrase):
      10 times (i):
        20 times (j):
          phrase print
    _say

The closing "_ say" ends the block saved to "say" var.

Normally, blocks are closed with a period. In this case
we'd need three periods, which looks strange.

    say = ():
      10 times:
        20 times:
          "Odelay!" print
    ...

If you prefer, you can give it some space. Or you can
use a variable name introduced by the block,

    say = (phrase):
      10 times (i):
        20 times (j):
          phrase print
    _ phrase


    say = (phrase):
      10 times (i):
        20 times (j):
          phrase print
      _ i
    .

Maybe it all looks strange. I don't know. I'm just trying
things out, okay?

* Elimination of line noise.

  I avoid @, #, $, %, {}.
  Stick with ., |, (), [], =, !, ?. Easier on the eyes.
  These are common punctuations in English.

* I try to defer to English when it comes to punctuation rules.

Period means "end". (In other langs it means "method call".)
Comma breaks up statements.
Space between messages gives a noun-verb feeling.

    window open (width=400, height=500)


* Named block args.


    (1, 2, 3) map (item=x, index=i): i display, x + 1.


* Assign is match + bind really (~ planned ~).

Assignments are side-effects only, but here extended.  Atoms on the
left-hand side (lhs) are trivial, but we prefer the power of LISP's
destructuring-bind within macros, or prolog or elixirs matching. So
= is actually a match operator which will recursively check if the
expressions on both left and right side match, and binds all found
lhs variables.

    (1, x) = (1, 2) => (x=2)
    (1, x) = (2, 3) => false
    1 = 2           => false

_ is a special variable which matches everything, but is never bound,
| seperates the head and tail from a list or lick.

So we can check trees like this:

    (_, x, 2)   = (0, 1, 2)   and say x #=> 1
    [_, [x, 1]] = [0, [1, 2]] and say x #=> 1
    [_, x]      = [0, [1, 2]] and say x #=> [1, 2]
    [_ | x]     = [0, 1, 2]   and say x #=> [1, 2]

    fun = (a, b): [0, [a, b]].
    [_ | [x, 1]] = fun(1, 2)  and say x #=> 1

## ~ feverish and fond thankyous ~

why is gravely indebted to Basile Starynkevitch, who fielded
questions about his garbage collector. why favors French hackers
to an extreme (Xavier Leroy, Nicolas Cannasse, Guy Decoux,
Mathieu Bochard to name only a portion of those I admire) and
is very glad to represent their influence in Potion's garbage
collector.

Matz, for answering why's questions about conservative GC and
for encouraging him so much. Potion's stack scanning code and
some of the object model come from Ruby.

Steve Dekorte for the Io language, libgarbagecollector and
libcoroutine -- I referred frequently to all of them in
sorting out what he wanted.

Of course, Mauricio Fernandez for his inspiring programming journal
housed at
http://web.archive.org/web/20110814062722/http://eigenclass.org/R2/
and for works derived throughout the course of it -- extprot most of
all.  Many of my thoughts about language internals (object repr, GC,
etc.) are informed by him.

Ian Piumarta for peg/leg. We use a re-entrant custom version
of it, but the original library is sheer minimalist parsing
amazement.

Final appreciations to Jonathan Wright and William Morgan
who pitched in, back in the wee hours of Potion's history.
Thanks.


## ~ license ~

See COPYING for legal information. It's an MIT license,
which lets you do anything you want with this. I'm hoping
that makes it very nice for folks who want to embed a little
Potion in their app!

