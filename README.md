# Reforth

*Some general Forth gripes:*

* Most non-trivial Forth source code becomes unreadable to all but the author.
* Vocabularies, wordlists, and the search order are a clunky way to do namespaces.
* Avoiding namespace pollution encourages building longer words which are harder to debug.
* Excessive stack manipulation is common, and is just confusing and distracting.
* ANS-Forth local variables are an overly-complex solution.
* Forth control structures could be more flexible.

*Some high-level gripes when using Forth inside other popular envrionments:*

* Null-terminated strings produce cleaner code than counted strings.
* Null-terminated strings allow easier linking to external C libraries.
* A regular expression engine would make scripting tasks more pleasant.
* Networking and Multithreading functionality is important.
* Hooks to parse and generate XML or HTML would be nice for web stuff.

## Rethinking Forth

*Enter Reforth!*

It is small, familiar, still a *Forth*. Not an ANS-Forth out of the box, but:

* Where Reforth uses ANS words, they generally work the same way or can be easily redefined.
* Reforth is pretty normal under the hood, so adding a full ANS layer would be straight forward.

Version 1 is built with C to get going quickly and so it can run on my x86(-64) machines and my ARM devices. It uses GNU C's [Labels as Values](http://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html) (like [gforth does](http://www.complang.tuwien.ac.at/forth/gforth/Docs-html/Threading.html#Threading)) to build a proper threaded Forth without relying on the C call stack.

It is [on github](https://github.com/seanpringle/reforth).

Reforth tries to solve my gripes *without fundamentally changing Forth's elegance and simplicity*.

Now to the gripes! Many of these have been solved in other Forths and certainly none are new ideas. But, for whatever reason, I havn't found them all together in one place. So, here goes...

## Simplified Control Structures

In Reforth, the word **end** replaces traditional Forth's **then**, **loop**, **again**, and **repeat**. This:

* reduces the core word count
* is shorter
* is simpler (every code block ends with **end**)
* is easier for non-Forth folk to pick up
* is only slightly more complex under the hood
* allows interlocking functionality between counted and conditional loops

Any number of **while** calls can be made in a single conditional loop:

    begin
        condition1
    while
        condition2
    while
        operation
    end

The above allows conditional chaining like the && operator in C or bash.

The word **until** does not terminate a loop, but is just syntactic sugar for **0= while**. Plus, the two can be mixed:

    begin
        condition1
    while
        condition2
    until
        operation
    end

Counted loops use **for end** instead of **do loop**, and take a single argument rather than an index and limit:

    10 for i . cr end

Having simpler counted loops allows the conditional loop words to also be used there:

    10 for
        condition1
    while
        operation
    end

And conversely **i** can also be used in a conditional loop to get the index:

    begin
        i . cr
    end

The words **leave** and **next** work like C's **break** and **continue** for fine-grained control, and may apply to either counted or conditional loops:

    begin
        condition
        if    operation
            next
        end
        condition1
        if    operation1
            leave
        end
    end

    10 for
        condition
        if    operation
            leave
        end
    end

Finally, ANS-Forth's **case of endof endcase** have been dumped, because it is easy to build a switch:

    begin
        condition1
        if    operation1
            leave
        end
        condition2
        if    operation1
            leave
        end
        operation
        leave
    end

## Smarter Control Structures

Words **if**, **begin**, and **for** are state-smart. When called in interpret mode they automatically enter compile mode. Later, **end** detects the fact, executes the compiled code fragment, and reverts to interpret mode. This is quite useful when using Forth as a shell or scripting language.

    > 5 for "hello world\n" type end
    hello world
    hello world
    hello world
    hello world
    hello world
     ok (0)

## Sub-Words

Word definitions may be nested.

    : hiphip ( -- )
        : cheer "hip hip, hooray!" type ;
        cheer cheer cheer
    ;

Ok, so one could do the above with a loop. But there is more to it:

* Sub-words are private and scoped so they are only visible in the current definition, avoiding namespace pollution. Above, one could not call **cheer** outside the definition of **hiphip**.
* Smaller definitions are a good thing for code clarity and maintenance. Go ahead and break up those long definitions into sub-words. Try using a sub-words instead of a nested code blocks.
* Private sub-words solve half the problem with vocabularies. More on this later.

Implementing sub-words is easy. Many Forths could probably do it:

1. Make **:** (colon) an immediate word.
1. Turn Forth's compile/interpret flag into a counter. Have **:** (colon) increment it and **;** (semi-colon) decrement.
1. Implement one of the following:
    * Compile a jump branch around sub-words so they do not execute until called, or...
    * Require sub-words be defined at the start of the outer word and adjust its entry point.
1. Make **;** (semicolon) patch the current wordlist appropriately to hide sub-words.

Example:

    : dump ( address length -- )

        : hex ( a -- )
            at! 16
            for c@+
                FFh and "%02x " format type
            end ;

        : ascii ( a -- )
            at! 16
            for c@+
                dup alpha? over digit? or 0=
                if drop 46 end emit
            end ;

        16 / 1+
        for
            dup "\n%08x  " format type
            dup hex space dup ascii
            16 +
        end
        drop ;

Result:

    > 'dump sys:xt-body @ 100 dump

    0062ef0a  72 00 18 00 14 00 6f 00 10 00 00 00 00 00 00 00  r.....o.........
    0062ef1a  73 00 0f 00 19 00 6f 00 ff 00 00 00 00 00 00 00  s.....o.........
    0062ef2a  28 00 70 00 04 00 25 30 32 78 20 00 98 00 74 00  ..p....02x....t.
    0062ef3a  02 00 72 00 1b 00 14 00 6f 00 10 00 00 00 00 00  ..r.....o.......
    0062ef4a  00 00 73 00 12 00 19 00 08 00 96 00 0a 00 95 00  ..s.............
    0062ef5a  29 00 2f 00 71 00 07 00 09 00 6f 00 2e 00 00 00  ....q.....o.....
    0062ef6a  00 00 00 00 40 00 74 00 02 00 6f 00 10 00 00 00  ......t...o..... ok

## Two Local Variables

Supporting random numbers of local variables in Forth makes for complex handling of the return stack at run-time, or a complex compiler, or both.

Reforth supports precisely two local variables: **at** and **my**

* **at** is designed to hold an address and is set with **at!**. It is like the A register from Chuck's later work, except it does not have to be saved and restored when calling words that also use it for their own purposes. The words **@+** and **!+** use and increment it automatically. It also works well (and importantly, reads well) as a pointer for relative addressing.
* **my** is a general purpose bucket set with **my!**. It fills the same sort of role as **>r**, **r@**, and **r>**, but is faster, uses less code, and doesn't need to be cleaned up.

Example:

    : accept ( buf lim -- len )
        over at!
        for
            key my!
            my while
            my 10 = until
            my c!+
        end
        0 c!+ at 1- swap - ;

Since the number of locals is fixed the implementation is able to be very efficient. Entering and exiting a high-level word simply adjusts the return stack pointer by three cells instead of one.

Being limited to two locals per word combines elegantly with sub-words. Want more locals? Use more words!

## Immediate Structures

It has been common for decades for Forth programmers to implement [records](http://en.wikipedia.org/wiki/Record_(computer_science)). Something like:

    : struct 0 ;
    : field create over , + does @ + ;

    struct
        cell field alpha
        cell field beta
         100 field gamma
    constant stuff

Basic relative addressing using names which improves code readbility. The create/does overhead can be a bottleneck but fancier and faster implementations are in the wild. Forth200x [has something similar](http://www.forth200x.org/structures2.html).

Reforth implements records and fields in code for efficiency, and uses a nice clean syntax:

    record stuff
        cell field alpha
        cell field beta
         100 field gamma
    end

Furthermore, **record** is an immediate word, so one can define private records inside words. This is useful for neatly managing memory allotted by **create does** defining words:

    : fruit ( apples oranges -- )

        record fields
            cell field apples
            cell field oranges
        end

        create
            here tuck
            fields allot
            oranges !
            apples  !
        does
            ( etc... ) ;

## Sub-words as Libraries

Sub-words lend themselves to implementing APIs and libraries just like traditional wordlists. In Reforth, the parser is modified to recognise a new syntax:

    hiphip:cheer

The above tells the parser to:

1. Look for the word **hiphip** in the dictionary
1. Look for the sub-word **cheer** in **hiphip**'s list
1. Execute or compile **cheer**

This solves the second half of the problem with vocabularies. Saying **hiphip:cheer** is unambiguous, does not rely on a search order being set first, and does double duty by grouping related words with a common prefix *without requiring that prefix to be used inside the library itself*.

## Sub-words as Objects

Being able to call sub-words using the *outer:inner* notation looks a lot like calling a static method in a class in other languages. How about proper objects and methods? We already have the tools: records, sub-words, and Forth's create/does.

    : fruit-basket ( -- )

        record fields
            cell field apples
            cell field oranges
            cell field bananas
        end

        : count-fruit ( this -- n )
            at!
            at apples  @
            at oranges @ +
            at bananas @ + ;

        create fields allot does ;

    fruit-basket hamper

    5 hamper.apples  !
    3 hamper.oranges !
    2 hamper.bananas !

    hamper.count-fruit

Above, the *outer:inner* static notation has been tweaked to be **object.method**. It tells the parser to:

1. Look for the word **hamper** in the dicitonary
1. Look for the sub-word **count-fruit** in **hamper**'s list
1. Execute or compile **count-fruit**

Step #2 works because Reforth's word **does** patches the defining word's sub-word list into created words' headers, ie, **hamper** inherits **fruit-basket**'s sub-word list.

A more useful example:

    : array ( -- )

        record fields
            cell field size
            cell field data
        end

        : index ( i a -- )
            dup at! size @ 1- min 0 max cells at data @ + ;

        : get ( i a -- n )
            index @ ;

        : set ( n i a -- )
            index ! ;

        : dump ( a -- )
            dup at! size @
            at "array %x\n" print
            for
                i at get
                i " %d => %d\n" print
            end ;

        : construct ( n -- a )
            here fields allot
            over cells allocate
            over data !
            tuck size ! ;

        : destruct ( a -- )
            dup data @ free free ;

        create construct drop does
    ;

Usage:

    > 3 array test
     ok
    > test.dump
    array 62f452
     0 => 0
     1 => 0
     2 => 0
     ok
    > 17 1 test.set
     ok
    > test.dump
    array 62f452
     0 => 0
     1 => 17
     2 => 0
     ok
    > 1 test.get .
    17  ok

## String Literals

Reforth understands how to parse string literals directly, without words like **s"** or **z"**. It follows the C-like escape sequences for \t, \r, \n, etc. A circular set of string buffers are used to allow up to three string literals in use simultaneously in interpret mode.

    "hello world\n" type

## String Formatting

Reforth has the word **format** which wraps C's **sprintf()** functionality. All the usual % sequences are supported. Like string literals, formatted strings use a circular set of buffers so that formatting operations may be chained in interpret mode.

    42 "the number is: %d\n" format type

## POSIX Regular Expressions

Reforth allows **match**ing and **split**ting strings by regex:

    : taste-test ( subject -- )

        "(apple|orange|banana)" match

        if "got fruit!\n" type end ;

    : process-lines ( text -- )

        : line ( text -- text' flag )
            dup "\r?\n" split rot type ;

        begin line while cr end drop ;


