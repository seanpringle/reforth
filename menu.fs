\ Menu

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

main