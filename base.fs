normal

0 value null
0 value false
-1 value true

: cell+ cell + ;
: cell- cell - ;

: what ( s -- f )
	" what? %s" format error false ;

'what sys:on-what !

: variable create 0 , ;

: load ( name -- )
	slurp dup my! evaluate drop my free ;

: include ( -- )
	sys:parse load ;

: word, sys:compile ;
: token,  'sys:lit_tok word, sys:compile  ;
: number, 'sys:lit_num word, sys:ncompile ;
: string, 'sys:lit_str word, sys:scompile ;
: place, here over count 1+ allot place ;
: string create place, ;

macro

: to ( -- )
	sys:parse sys:normals sys:find sys:mode @
	if token, 'vary word, else vary end ;

normal

: digit? ( c -- f )
	dup 47 > swap 58 < and ;

: alpha? ( c -- f )
	dup my! 64 > my 91 < and my 96 > my 123 < and or ;

: space? ( c -- f )
	dup my! 32 = my 9 = or my 10 = or my 13 = or ;

: hex? ( c -- f )
	dup my! 64 > my 71 < and my 96 > my 103 < and or my digit? or ;

\ diagnostics

: print format type ;
: . "%d " print ;
: cr 10 emit ;
: space 32 emit ;

: .s ( -- )
	depth dup "(%d) " print
	for depth 1- i - pick "%d " print end ;

: dump ( a n -- )

	: print ( a -- )
		format error ;

	: hex ( a -- )
		at! 16
		for c@+
			FFh and "%02x " print
		end ;

	: ascii ( a -- )
		at! 16
		for c@+
			dup alpha? over digit? or 0=
			if drop 46 end "%c" print
		end ;

	16 / 1+
	for
		dup "\n%08x  " print
		dup hex space dup ascii
		16 +
	end
	drop ;

: words ( word -- )
	0 sys:latest @
	begin
		dup while sys:head-xt
		dup sys:xt-name @ type space
		sys:xt-link @ push 1+ pop
	end
	drop "(%d words)" print ;

: vector ( default cells -- )

	: index ( index this -- a )
		at! @+ 1- min 0 max cells at + ;

	: set ( xt index this -- )
		index ! ;

	: run ( index this -- )
		index @ execute ;

	create dup , for dup , end drop does ;

: array ( -- )

	record fields
		cell field size
		cell field data
	end

	: bounds ( n a -- n a f )
		over my! dup size @ my > my 1+ 0> and ;

	: index ( i a -- )
		data @ swap cells + ;

	: get ( i a -- n )
		bounds if index @ else drop drop 0 end ;

	: set ( n i a -- )
		bounds if index ! else drop drop end ;

	: dump ( a -- )
		dup at! size @
		at "array %x\n" print
		for
			i at get
			i " %d => %d\n" print
		end ;

	: grow ( n a -- )
		at! dup at size @ + 0 max my!
		at data @ my 1+ cells resize at data ! my at size !
		0 max for 0 at size @ 1- i - at set end ;

	: ins ( n i a -- )
		bounds
		if
			at! my! my at index dup cell+
			at size @ my - 1- 0 max move
			my at set
		else
			drop drop drop
		end ;

	: del ( i a -- n )
		bounds
		if
			at! my! my at get
			my at index dup cell+ swap
			at size @ my - 0 max move
		else
			drop drop 0
		end ;

	: construct ( n a -- )
		at! dup at size ! cells allocate at data ! ;

	: destruct ( a -- )
		data @ free ;

	create here fields allot construct does
;

: accept ( buf lim -- flag )

	static vars

		0 value tib
		0 value lim
		0 value caret
		0 array input

		'drop 256 vector ekeys
		create escseq 100 allot

	end

	: escape ( vector -- flag )

		: lone-key? ( -- f )
			0 50 for key? or dup until 1000 usec end 0= ;

		: read-seq ( a -- c )
			at! key my! my c!+ my 91 =
			if
				begin
					key dup c!+ dup while
					dup 63 > over 127 < and until
					drop
				end
				my!
			end
			0 c!+ my ;

		at! lone-key? if false exit end
		escseq dup read-seq at vector:run true ;

	: caret+ ( -- )
		caret 1+ lim 1- min to caret ;

	: caret- ( -- )
		caret 1- 0 max to caret ;

	: addc ( c -- )
		caret input.set caret+ ;

	: bakc ( -- )
		caret- 0 addc caret- ;

	: draw ( -- )
		"\e8\e[?25l\e[K" type
		lim
		for
			i input.get my!
			my while my emit
		end
		"\e[K\e[?25h" type ;

	: listen ( -- )

		caret
		for
			tib i + c@ my! my while
			my i input.set
		end

		begin
			draw
			key my!

			my 27 =
			if	ekeys escape 0=
				if
					false exit
				end
				next
			end

			my 127 =
			if	bakc
				next
			end

			my 31 > my 127 < and
			if	my addc
				next
			end

			my 10 = until
		end

		tib at! lim 1-
		for
			i input.get my!
			my while my c!+
		end
		0 c!+ true ;

	to lim to tib lim input.grow
	"\e7" type \ save cursor

	tib count to caret

	listen

	lim neg input.grow ;

: shell ( -- )

	: what ( s -- f )
		" what? %s\n" print
		depth for drop end false ;

	: error ( n -- f ) my!
		begin
			my 1 =
			if	" stack underflow!\n" type
				false leave
			end
			false leave
		end ;

	: ok ( -- )
		" ok " type .s cr ;

	'ok    sys:on-ok    !
	'what  sys:on-what  !
	'error sys:on-error !

	100 allocate at!

	begin
		0 at c! "> " type
		at 100 accept cr
		if
			at evaluate drop
		else
			ok
		end
	end
;

