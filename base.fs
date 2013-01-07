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
		at! dup at size @ + my!
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

	: construct ( n -- a )
		here fields allot
		over 1+ cells allocate
		over data !
		tuck size ! ;

	: destruct ( a -- )
		dup data @ free free ;

	create construct drop does
;

: assoc ( -- )

	record fields
		cell field keys
		cell field vals
	end

	: size ( a -- n )
		keys @ array:size @ ;

	: used ( n -- n )
		at! 0 at size
		for
			i at keys @ array:get
			if 1+ end
		end ;

	: index ( k a -- i )
		keys @ at! my!
		-1 at array:size @
		for
			my i at array:get compare
			0= if drop i leave end
		end ;

	: empty ( a -- i )
		keys @ at!
		-1 at array:size @
		for
			i at array:get 0=
			if drop i leave end
		end ;

	: get ( k a -- v )
		dup at! index my!
		my 0< if 0 else
			my at vals @ array:get
		end ;

	: set ( v k a -- )
		at! dup my! at index
		dup 0< if drop at empty end
		dup 0< if drop drop else
			my over
			at keys @ array:set
			at vals @ array:set
		end ;

	: del ( k a -- )
		dup at! index my!
		my 1+ 0>
		if
			0 my at keys @ array:set
			0 my at vals @ array:set
		end ;

	: try ( d k a -- v )
		dup at! index my! my 1+ 0>
		if	drop
			my at vals @ array:get
		end ;

	: dump ( a -- )
		dup at! size
		at "assoc %x\n" print
		for
			i at vals @ array:get
			i at keys @ array:get
			" %s => %d\n" print
		end ;

	: construct ( n -- a )
		here at! fields allot dup
		array:construct at keys !
		array:construct at vals !
		at ;

	: destruct ( a -- )
		dup keys @ array:destruct
		dup vals @ array:destruct
		free ;

	create construct drop does
;

: shell ( -- )

	record fields
		256 array keys
		256 array ekeys
		100 array input
		create tib 100 allot
		variable caret
	end

	: caret+ ( n -- )
		caret @ + 98 min 0 max caret ! ;

	: addc ( c -- )
		caret @ input.set 1 caret+ ;

	: draw ( -- )
		"\r\e[?25l\e[K> " type
		100 for
			i input.get my!
			my while my emit
		end
		"\e[K\e[?25h" type ;

	: escape ( -- )
		key 91 =
		if
			begin
				key dup 63 > swap 127 < and until
			end
		end ;

	: listen ( -- )
		0 caret !
		100 for
			0 i input.set
		end

		begin
			draw
			key my!

			my 27 =
			if	escape
				next
			end

			my 31 > my 127 < and
			if	my addc
				next
			end

			my 10 = until
		end
		tib at!
		100 for
			i input.get my!
			my while my c!+
		end
		0 c!+ ;

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

	: ok ( -- f )
		" ok " type .s cr ;

	'ok    sys:on-ok    !
	'what  sys:on-what  !
	'error sys:on-error !

	begin
		listen cr
		tib evaluate drop
	end
;

