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

: match? ( s p -- f )
	match nip ;

: unit ( -- )

	static fields
		cell field x
		cell field y
		cell field z
		cell field dx
		cell field dy
		cell field dz
		cell field radius
	end

	: create ( -- a ) fields allocate ;
	: destroy ( a -- ) free ;

;

: heightmap ( x y z buffer -- )

	static locals
		 0 value x
		 0 value y
		 0 value z
		 0 value buff
	end

	to buff
	to z
	to y
	to x

	: tile ( x y -- a )
		0 max y 1- min push
		0 max x 1- min pop
		x * + buff + ;

	: randomize ( n -- )
		my!
		0 0 tile at!
		x for
			y for
				my random at c@ + c!+
			end
		end ;

	: smooth ( -- )
		x for
			i my!
			y for 0
				my 1- i 1- tile c@ +
				my 1- i    tile c@ +
				my 1- i 1+ tile c@ +
				my    i 1- tile c@ +
				my    i    tile c@ +
				my    i 1+ tile c@ +
				my 1+ i 1- tile c@ +
				my 1+ i    tile c@ +
				my 1+ i 1+ tile c@ +
				9 / my i tile c!
			end
		end ;

	: zoom ( -- )

		static locals
			0 value tmp
			0 value xs
			0 value ys
		end

		: scale ( n -- n' )
			8 / ;

		x scale to xs
		y scale to ys

		: ztile ( x y -- a )
			xs * + tmp + ;

		xs ys * allocate to tmp

		xs for
			i my!
			ys for
				my i tile  c@
				my i ztile c!
			end
		end

		x for
			i my!
			y for
				my scale i scale ztile c@
				my i tile c!
			end
		end

		tmp free ;

	: lowest ( -- n )
		0 0 tile c@
		x for
			i my!
			y for
				my i tile c@ min
			end
		end ;

	: adjust ( -- )
		lowest neg
		0 0 tile at!
		x for
			y for
				at c@ over + c!+
			end
		end drop ;

	z randomize
	smooth
	zoom
	z 2/ randomize
	smooth
	adjust ;

: arena ( -- )

	static locals
		256 value max_x
		256 value max_y
		 16 value max_z
		max_x max_y * buffer map
	end

	: tile ( x y -- a )
		max_x * + map ;

	: generate ( -- )
		max_x max_y max_z 0 map heightmap
	;
;


: display ( -- )

	: rows ( -- n )
		max-xy nip 1+ ;

	: cols ( -- n )
		max-xy drop 1+ ;

	: blat ( s -- )
		here swap place, create , does @ type ;

	static locals
		"\e[K"    blat erase
		"\e[?25h" blat cursor-on
		"\e[?25l" blat cursor-off
		"\e7"     blat cursor-save
		"\e8"     blat cursor-back
		"\e[?7h"  blat wrap-on
		"\e[?7l"  blat wrap-off

		\ Default 16 color mode
		"\e[39;22m" blat fg-normal
		"\e[34;22m" blat fg-comment
		"\e[35;22m" blat fg-string
		"\e[36;22m" blat fg-number
		"\e[33;22m" blat fg-keyword
		"\e[39;22m" blat fg-coreword
		"\e[39;22m" blat fg-variable
		"\e[39;22m" blat fg-constant
		"\e[31;22m" blat fg-define
		"\e[39;22m" blat fg-status
		"\e[49;22m" blat bg-normal
		"\e[49;1m"  blat bg-active
		"\e[47;22m" blat bg-marked
		"\e[49;1m"  blat bg-status

		0 value col
		0 value row
	end

	: colors ( -- )

		: xterm-color ( r g b -- n )

			: xcolor? ( n -- f )
				dup 0= over 5Eh > or swap 55 - 0 max 40 mod 0= and ;

			: color? ( r g b -- r g b f )
				dup xcolor? push rot
				dup xcolor? push rot
				dup xcolor? push rot
				pop pop pop and and ;

			: grey? ( r g b -- r g b f )
				push over over = my! pop over over = my and ;

			: scale ( n -- n' )
				55 - 0 max 20 + 40 / ;

			: to-color ( r g b -- n )
				scale swap scale 6 * + swap scale 36 * + 16 + ;

			: to-grey ( r g b -- n )
				swap 8 shl or swap 16 shl or 657930 / 232 + ;

			grey? push color? 0= pop and
			if to-grey else to-color end
			0 max 255 min ;

		: escseq ( n -- s )
			"\e[%d;5;%dm" format here swap place, ;

		: fg-256 ( r g b -- a )
			xterm-color 38 escseq ;

		: bg-256 ( r g b -- a )
			xterm-color 48 escseq ;

		\ xterm 256-color mode
		"TERM"      getenv "256color"       match?
		"COLORTERM" getenv "gnome-terminal" match? or
		if

			: re ( s xt -- )
				sys:xt-body @ @ ! ;

			D8h D8h D8h fg-256 'fg-normal   re
			66h 66h 66h fg-256 'fg-comment  re
	\		82h AAh 8Ch fg-256 'fg-comment  re
			82h AAh 8Ch fg-256 'fg-string   re
			8Ch C8h C8h fg-256 'fg-number   re
			F0h DCh AFh fg-256 'fg-keyword  re
			FFh FFh FFh fg-256 'fg-coreword re
			C0h BEh D0h fg-256 'fg-variable re
			C8h 91h 91h fg-256 'fg-constant re
			F0h F0h 8Ch fg-256 'fg-define   re
			FFh FFh FFh fg-256 'fg-status   re
			30h 30h 30h bg-256 'bg-normal   re
			40h 40h 40h bg-256 'bg-active   re
			40h 40h 40h bg-256 'bg-marked   re
			00h 00h 00h bg-256 'bg-status   re
		end ;

	: gray ( n -- )
		25 * dup dup colors:xterm-color 48 "\e[%d;5;%dm" print ;

	: start ( -- )
		sys:pseudo-terminal
		sys:unbuffered
		wrap-off
		cursor-off
		colors ;

	: stop ( -- )
		wrap-on
		cursor-on
		sys:default-stdio
		sys:buffered
		;

	: col+ ( -- )
		col 1+ to col ;

	: row+ ( -- )
		row 1+ to row ;

	: render ( -- )

		0 0 at-xy fg-normal bg-normal

		rows
		for	i to row
			0 row at-xy
			cols 2/
			for	i to col
				col row arena:tile c@ dup gray "%02d" print
			end
			bg-normal erase
		end ;

;

: main ( -- )

	display:start
	arena:generate

	begin
		display:render
		1000 usec

		key? until
	end

	display:stop ;

main .s