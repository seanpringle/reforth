\ Reforth

normal

0 value null
0 value false
-1 value true

: cell+ cell + ;
: cell- cell - ;

: <= my! 1- my < ;
: >= my! 1+ my > ;
: 2dup  over over ;
: 2drop drop drop ;

: what ( s -- f )
	" what? %s" format error false ;

'what sys:on-what !

: variable create 0 , ;
: enum push dup value 1+ pop ;

: included ( name -- )
	slurp dup my! evaluate drop my free ;

: include ( -- )
	sys:parse included ;

: word, sys:compile ;
: token,  'sys:lit_tok word, sys:compile  ;
: number, 'sys:lit_num word, sys:ncompile ;
: string, 'sys:lit_str word, sys:scompile ;
: place, here over count 1+ allot place ;
: string create place, ;

macro

: to ( n -- )
	sys:parse sys:normals sys:find sys:mode @
	if token, 'vary word, else vary end ;

: is ( xt -- )
	sys:parse sys:normals sys:find sys:mode @
	if 'sys:xt-body word, '@ word, token, 'sys:xt-body word, '! word,
	else swap sys:xt-body @ swap sys:xt-body ! end ;

normal

: nop ;

10 value \n
13 value \r
 9 value \t
 7 value \a
27 value \e
 8 value \b
32 value \s

: digit? ( c -- f )
	dup 47 > swap 58 < and ;

: alpha? ( c -- f )
	dup my! 64 > my 91 < and my 96 > my 123 < and or ;

: space? ( c -- f )
	dup my! 32 = my 9 = or my 10 = or my 13 = or ;

: hex? ( c -- f )
	dup my! 64 > my 71 < and my 96 > my 103 < and or my digit? or ;

: basename ( a -- a' )
	dup "/[^/]+$" match if nip 1+ else drop end ;

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

: array ( n -- )
	create dup , for 0 , end does
		at! @+ 1- min 0 max cells at + ;

: stack ( -- )

	record fields
		cell field size
		cell field data
	end

	: last ( a -- b )
		at! at size @ 1- 0 max cells at data @ + ;

	: inc ( a -- )
		at! 1 at size +! at data @ at size @ cells resize at data ! ;

	: dec ( a -- )
		size dup @ 1- 0 max swap ! ;

	: push ( n a -- )
		dup inc last ! ;

	: pop ( a -- n )
		dup size @ if dup last @ swap dec else drop 0 end ;

	: top ( a -- n )
		dup size @ if last @ else drop 0 end ;

	: base ( a -- b )
		data @ ;

	: depth ( a -- n )
		size @ ;

	create here fields allot cell allocate swap data ! does ;

: sort ( xt a n -- )

	static vars
		0 value cmp
	end

	: mid ( l r -- mid )
		over - 2/ cell neg and + ;

	: exch ( a1 a2 -- )
		dup @ push over @ swap ! pop swap ! ;

	: part ( l r -- l r r2 l2 )
		2dup mid @ push
		2dup
		begin
			swap begin dup @ top  cmp execute while cell+ end
			swap begin top over @ cmp execute while cell- end
			2dup <= if 2dup exch push cell+ pop cell- end
			2dup > until
		end
		pop drop ;

	: qsort ( l r -- )
		part swap rot
		2dup < if qsort else 2drop end
		2dup < if qsort else 2drop end ;

	rot to cmp dup 1 > if 1- cells over + qsort else 2drop end ;

: edit ( -- )

	static vars
		0 value caret
		0 value input
		0 value limit
		create escseq 50 allot
	end

	: escape ( -- c )

		: last ( -- c )
			 escseq dup dup count + 1- max c@ ;

		: read ( -- )
			 key escseq dup count + at! c!+ 0 c!+ ;

		: done? ( -- f )
			 last dup `@ >= swap `~ <= and ;

		: start? ( -- f )
			 last `[ = ;

		: more ( -- )
			 read start? if begin read done? until end end ;

		: more? ( -- f )
			 0 25 for key? or dup until 1000 usec end ;

		"\e" escseq place more? if more end last ;

	: escseq? ( s -- f )
		escseq 1+ dup count compare 0= ;

	: length ( -- n )
		input count ;

	: point ( -- a )
		input caret + ;

	: home ( -- )
		0 to caret ;

	: away ( -- )
		input count to caret ;

	: ins ( c -- )
		point dup dup 1+ place c! ;

	: del ( --  c )
		point at! at c@ dup if at 1+ at place end ;

	: tilde ( -- )
		"[1~" escseq? if home exit end
		"[7~" escseq? if home exit end
		"[4~" escseq? if away exit end
		"[8~" escseq? if away exit end
		"[3~" escseq? if del  exit end ;

	: left ( -- )
		caret 1- 0 max to caret ;

	: right ( -- )
		caret 1+ length min to caret ;

	static vars
		256 array ekeys
		'right `C ekeys !
		'left  `D ekeys !
		'tilde `~ ekeys !
	end

	: start ( buf lim -- )
		to limit to input length to caret ;

	: show ( -- )
		input type "\e[K" type length caret - dup if dup "\e[%dD" print end drop ;

	: step ( -- c )

		key my!

		begin

			my while
			my 10 = until

			my 27 =
			if
				escape
				ekeys @ execute
				leave
			end

			my 8 = my 127 = or
			if
				left del drop
				leave
			end

			length limit <
			my 31 > and my 127 < and
			if
				my ins right
				leave
			end
		end
		my ;

	: stop ( -- len )
		input count ;
;

: accept ( buf lim -- len )
	"\e7" type
	edit:start
	begin
		"\e8" type
		edit:show
		edit:step my!
		my 27 =
		"" edit:escseq? and
		if
			0 edit:input !
			leave
		end
		my 10 = until
	end
	edit:stop ;

