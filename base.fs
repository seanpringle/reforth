normal

0 value null
0 value false
-1 value true

: what ( s -- f )
	" what? %s" format error false ;

'what sys:on-what !

: load ( name -- )
	slurp dup my! evaluate drop my free ;

: digit? ( c -- f )
	dup 47 > swap 58 < and ;

: alpha? ( c -- f )
	dup my! 64 > my 91 < and my 96 > my 123 < and or ;

: space? ( c -- f )
	dup my! 32 = my 9 = or my 10 = or my 13 = or ;

\ diagnostics

: print format type ;
: . "%d " print ;
: cr 10 emit ;
: space 32 emit ;

: .s ( -- )
	depth dup "(%d) " print
	for depth 1- i - pick . end ;

: dump ( a n -- )

	: hex ( a -- )
		at! 16
		for c@+
			FFh and "%02x " print
		end ;

	: ascii ( a -- )
		at! 16
		for c@+
			dup alpha? over digit? or 0=
			if drop 46 end emit
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

: accept ( buf lim -- len )
	over at!
	for	key my!
		my while
		my 10 = until
		my c!+
	end	0 c!+
	at 1- swap - ;

: shell ( -- )

	record fields
		100 field tib
	end

	fields allocate my!

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
		"> " type
		my tib 100 accept
		if	my tib evaluate drop
		end
	end
;

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
