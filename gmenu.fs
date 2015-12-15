\ MIT/X11 License
\ Copyright (c) 2015 Sean Pringle <sean.pringle@gmail.com>
\
\ Permission is hereby granted, free of charge, to any person obtaining
\ a copy of this software and associated documentation files (the
\ "Software"), to deal in the Software without restriction, including
\ without limitation the rights to use, copy, modify, merge, publish,
\ distribute, sublicense, and/or sell copies of the Software, and to
\ permit persons to whom the Software is furnished to do so, subject to
\ the following conditions:
\
\ The above copyright notice and this permission notice shall be
\ included in all copies or substantial portions of the Software.
\
\ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
\ OR IMPLIED, ADD1LUDING BUT NOT LIMITED TO THE WARRANTIES OF
\ MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
\ IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
\ CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
\ TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
\ SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

: help ( -- )

	macro

	\ Compile a comment as a string, and type it.
	: \ ( -- )
		sys:source @ dup "\n" match drop at! 0 c!+ dup at 1- <>
		if 1+ end string, 'type word, 'cr word, at sys:source ! ;

	normal

	\ Usage: $EDITOR $(find -type f | gmenu)
	\
	\ Interactive grep for a line of text from stdin.

;

: blat ( s -- )
	here swap place, create , does @ type ;

"\e[K"    blat erase
"\e[2J"   blat page
"\e[?25h" blat cursor-on
"\e[?25l" blat cursor-off
"\e7"     blat cursor-save
"\e8"     blat cursor-back
"\e[?7h"  blat wrap-on
"\e[?7l"  blat wrap-off

"\e[39;22m" blat fg-normal
"\e[36;22m" blat fg-select

stack lines
stack matches
0 value aborted
0 value highlight
create input 100 allot
create pattern 100 allot

: rows ( -- n )
	max-xy nip ;

: cols ( -- n )
	max-xy drop ;

: match? ( s p -- f )
	match nip ;

: readlines ( -- )
	begin here at!
		begin key my!
			my while
			my \n = until
			my c!+
		end
		here at <
		if
			0 c!+
			here at over - allocate
			dup lines.push place
		end
		my while
	end ;

: cmp ( a b -- f )
	dup count compare 0< ;

: sortlines ( -- )
	'cmp lines.base lines.depth sort ;

: process ( -- )

	input at! pattern
	begin
		c@+ my!
		my while
		my space?
		if
			`. over c! 1+
			`* over c! 1+
		else
			my over c! 1+
		end
	end
	0 swap c!

	0 matches.size !
	rows 1-
	begin
		i lines.depth < while
		matches.depth over < while

		input c@ 0=
		i lines.get pattern match? or
		if i lines.get matches.push end
	end
	drop
	highlight matches.depth 1- min 0 max to highlight ;

: display ( -- )
	cursor-save
	cursor-off
	process

	0 1 at-xy rows 1-
	matches.depth min
	for
		i highlight =
		if fg-select else fg-normal end

		i matches.get type erase cr
	end

	rows 1-
	matches.depth - 0 max
	for
		erase cr
	end

	cursor-back
	cursor-on ;

: up ( -- )
	highlight 1- 0 max to highlight ;

: dn ( -- )
	highlight 1+ to highlight ;

: main ( -- )

	readlines
	sortlines

	sys:pseudo-terminal
	sys:unbuffered

	wrap-off

	'up `A edit:ekeys !
	'dn `B edit:ekeys !

	input 100 edit:start

	begin

		0 0 at-xy
		fg-normal
		edit:show
		display

		edit:step my!
		edit:done until
		my \e =
		"" edit:escseq? and
		if
			0 edit:input !
			1 to aborted
			leave
		end
		my \n = until
	end
	edit:stop drop

	wrap-on

	sys:buffered
	sys:default-stdio

	aborted if aborted die end

	highlight matches.depth <
	if highlight matches.get type end ;

\ Be friendly
1 arg "^--help$" match?
1 arg "^-h$"     match? or
if
	help bye
end

main