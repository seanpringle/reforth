\ MIT/X11 License
\ Copyright (c) 2012-2015 Sean Pringle <sean.pringle@gmail.com>
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

\ Reforth Base

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
: on -1 swap ! ;
: off 0 swap ! ;

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
: strdup dup count 1+ allocate tuck place ;

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
	dup my! \s = my \t = or my \n = or my \r = or ;

: hex? ( c -- f )
	dup my! 64 > my 71 < and my 96 > my 103 < and or my digit? or ;

: match? ( s p -- f )
	match nip ;

: basename ( a -- a' )
	dup "/[^/]+$" match if nip 1+ else drop end ;

\ diagnostics

: print format type ;
: . "%d " print ;
: cr \n emit ;
: space \s emit ;

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

: buffer ( n -- )
	create dup , for 0 c, end does
		at! @+ 1- min 0 max at + ;

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

	: get ( p a -- n )
		at! 0 max at size @ 1- min cells at data @ + @ ;

	: construct ( -- a )
		fields allocate cell allocate over data ! ;

	: destruct ( a -- )
		dup data @ free free ;

	create here fields allot cell allocate swap data ! does ;

: list ( -- )

	record fields
		cell field first
		cell field last
		cell field nodes
	end

	record node_fields
		cell field prev
		cell field post
		cell field payload
	end

	: length ( a -- n )
		nodes @ ;

	: link_before ( old new -- )
		my! at! at prev @ if my at prev @ post ! end at my post ! at prev @ my prev ! my at prev ! ;

	: link_after ( old new -- )
		my! at! at post @ if my at post @ prev ! end at my prev ! at post @ my post ! my at post ! ;

	: node_by_index ( pos a -- node )
		at! my! at first @ begin dup while i my < while post @ end ;

	: index_by_node ( node a -- pos )
		at! my! at first @ begin dup while dup my = if drop i exit end post @ end drop -1 ;

	: insert ( payload position a -- )
		at! node_fields allocate my!
		0 max at length min
		begin
			dup 0=
			if
				at first @
				if
					at first @ my link_before
				else
					my at last !
				end
				my at first !
				leave
			end
			dup at length =
			if
				at last @ my link_after
				my at last !
				leave
			end
			dup at node_by_index my link_before
			leave
		end
		drop my payload !
		1 at nodes +! ;

	: remove_node ( node a -- payload )
		at! push

		top post @
		top prev @

		over if dup if over over post ! end else dup at last  ! dup if 0 over post ! end end

		swap

		over if dup if over over prev ! end else dup at first ! dup if 0 over prev ! end end

		drop drop

		top payload @
		pop free
		-1 at nodes +! ;

	: remove ( position a -- payload )
		at! my! false
		my 0 >= my at length < and
		if
			drop my at node_by_index at remove_node
		end ;

	: push ( payload a -- )
		dup length swap insert ;

	: pop ( a -- payload )
		dup length 1- swap remove ;

	: shove ( payload a -- )
		0 swap insert ;

	: shift ( a -- payload )
		0 swap remove ;

	: get ( position a -- payload )
		at! at node_by_index dup if payload @ end ;

	: set ( payload position a -- )
		at! my! my at node_by_index dup if payload ! else drop my at insert end ;

	: dump ( a -- )
		"[ " type
		first @ at!
		begin at while
			at post @
			at prev @
			at
			at payload @
			"%d (node: %x prev: %x post: %x)" print
			at post @ at!
			at if ", " type end
		end
		" ]" type ;

	: construct ( -- a )
		fields allocate ;

	: destruct ( a -- )
		dup first @ my!
		begin my while
			my post @ my free my!
		end free ;

	create fields allot does ;

: dict ( chains -- )

	record fields
		cell field width
		cell field chains
	end

	record node_fields
		cell field name
		cell field payload
	end

	: chain ( n a -- c )
		chains @ swap cells + ;

	: length ( a -- n )
		at! 0 at width @ for i at chain @ list:length + end ;

	: hash ( name -- n )
		at! 5381 begin c@+ my! my while 33 * my + end ;

	: locate_node ( name list -- node )
		list:first @ my! dup count 1+
		begin my while over over
			my list:payload @ name @ swap compare while
			my list:post @ my!
		end
		drop drop my ;

	: insert ( payload name a -- )
		at!
		dup hash at width @ mod at chain @ my! ( payload name )
		dup my locate_node dup
		if
			nip list:payload @ payload !
		else
			drop strdup
			node_fields allocate
			tuck name !
			tuck payload !
			my list:push
		end ;

	: remove ( name a -- payload )
		at!
		dup hash at width @ mod at chain @ my! ( payload name )
		my locate_node dup
		if
			my list:remove_node my!
			my name @ free
			my payload @
			my free
		end ;

	: exists ( name a -- flag )
		at!
		dup hash at width @ mod at chain @ my! ( payload name )
		my locate_node 0<> ;

	: get ( name a -- payload )
		at!
		dup hash at width @ mod at chain @ my! ( payload name )
		my locate_node dup
		if
			list:payload @ payload @
		end ;

	: set ( payload name a -- )
		insert ;

	: dump ( a -- )
		at!
		at width @
		for
			i at chain @ list:first @
			begin dup while
				dup list:payload @
				dup payload @ swap name @ "%s => %d\n" print
				list:post @
			end drop
		end ;

	: construct ( chains a -- )
		at! dup at width ! dup cells allocate at chains !
		for list:construct i at chain ! end ;

	: destruct ( a -- )
		at! at width @
		for i at chain @ list:destruct end
		at chains @ free ;

	create here fields allot construct does ;

: sort ( xt a n -- )

	static locals
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

	static locals
		0 value caret
		0 value input
		0 value limit
		0 value done
		4 value EOT
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

	static locals
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
		my EOT = my 0= or to done

		begin

			my while
			my \n = until
			done until

			my \e =
			if
				escape
				ekeys @ execute
				leave
			end

			my \b = my 127 = or
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

			leave
		end
		my ;

	: stop ( -- len )
		input count ;
;

: accept ( buf lim -- len )

	: done edit:done ;

	"\e7" type
	edit:start
	begin
		"\e8" type
		edit:show
		edit:step my!
		edit:done until
		my \e =
		"" edit:escseq? and
		if
			0 edit:input !
			leave
		end
		my \n = until
	end
	edit:stop ;

: license ( -- )

	: . type cr ;

	"Permission is hereby granted, free of charge, to any person obtaining"
	. "a copy of this software and associated documentation files (the"
	. "\"Software\"), to deal in the Software without restriction, including"
	. "without limitation the rights to use, copy, modify, merge, publish,"
	. "distribute, sublicense, and/or sell copies of the Software, and to"
	. "permit persons to whom the Software is furnished to do so, subject to"
	. "the following conditions:\n"

	. "The above copyright notice and this permission notice shall be"
	. "included in all copies or substantial portions of the Software.\n"

	. "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS"
	. "OR IMPLIED, ADD1LUDING BUT NOT LIMITED TO THE WARRANTIES OF"
	. "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT."
	. "IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY"
	. "CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,"
	. "TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE"
	. "SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE." . ;

\ Execute SOURCE as a system shell command
: sh ( -- ) 0 sys:source @ null over system type c! ;

: shell ( -- )

	static locals
		create input 1000 allot
		0 value on-ok
		0 value on-what
		0 value on-error
	end

	: what ( s -- f )
		" what? %s\n" print
		depth for drop end false ;

	: error ( n -- f ) my!
		begin
			my 1 =
			if	" stack underflow!\n" type
				false leave
			end
			my "error code %d" format type
			false leave
		end ;

	: ok ( -- )
		" ok " type .s cr ;

	sys:on-ok    @ to on-ok
	sys:on-what  @ to on-what
	sys:on-error @ to on-error

	'ok    sys:on-ok    !
	'what  sys:on-what  !
	'error sys:on-error !

	sys:unbuffered

	begin
		"> " type 0 input c!
		input 1000 accept drop
		accept:done until
		cr input evaluate drop
	end

	sys:buffered

	on-ok    sys:on-ok    !
	on-what  sys:on-what  !
	on-error sys:on-error ! ;

\ break point
: ~ ( -- )
	shell ;

