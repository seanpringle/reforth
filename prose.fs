\ MIT/X11 License
\ Copyright (c) 2012-2016 Sean Pringle <sean.pringle@gmail.com>
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

	\ Usage: rp [file]
;

: word ( -- )

	static locals
		1 0 shl value bit-period
		1 1 shl value bit-comma
		1 2 shl value bit-dquote
		1 3 shl value bit-exclaim
		1 4 shl value bit-question
		1 5 shl value bit-hyphen
		1 6 shl value bit-colon
		1 7 shl value bit-semicolon
		1 8 shl value bit-paren

		bit-period
		bit-comma     or
		bit-exclaim   or
		bit-question  or
		bit-hyphen    or
		bit-colon     or
		bit-semicolon or
		value bit-punct
	end

	record fields
		cell field flags
		cell field col \ set by display
		cell field row \ set by display
		  32 field payload
	end

	: length ( a -- n )
		payload @ count ;

	: set ( s a -- )
		at! payload place ;

	: get ( a -- s )
		payload @ ;

	: append ( c a -- )
		payload @ dup count + at! c!+ 0 c!+ ;

	: back ( a -- )
		payload @ at! 0 c!+ ;

	: toggle ( disable-bits toggle-bits a -- )
		flags at! at @ swap xor push inv pop and !+ ;

	: period    ( a -- ) push bit-punct bit-period    inv and bit-period    pop toggle ;
	: comma     ( a -- ) push bit-punct bit-comma     inv and bit-comma     pop toggle ;
	: question  ( a -- ) push bit-punct bit-question  inv and bit-question  pop toggle ;
	: exclaim   ( a -- ) push bit-punct bit-exclaim   inv and bit-exclaim   pop toggle ;
	: hyphen    ( a -- ) push bit-punct bit-hyphen    inv and bit-hyphen    pop toggle ;
	: colon     ( a -- ) push bit-punct bit-colon     inv and bit-colon     pop toggle ;
	: semicolon ( a -- ) push bit-punct bit-semicolon inv and bit-semicolon pop toggle ;

	: dquote ( a -- )
		push 0 bit-dquote pop toggle ;

	: dparen ( a -- )
		push 0 bit-paren pop toggle ;

	: punct? ( a -- n )
		flags @ bit-punct and ;

	: quote? ( a -- n )
		flags @ bit-dquote and ;

	: paren? ( a -- n )
		flags @ bit-paren and ;

	: hyphen? ( a -- n )
		flags @ bit-hyphen and ;

	: empty? ( -- f )
		get c@ 0= ;

	: focus ( a -- )
		drop ;

	: blur ( a -- )
		drop ;

	: construct ( -- a )
		fields allocate at!
		32 allocate at payload !
		at ;

	: destruct ( a -- )
		dup payload @ free free ;

	create construct , does @ ;

: paragraph ( -- )

	record fields
		cell field words
		cell field cursor
	end

	: length ( a -- n )
		words @ list:length ;

	: text ( a -- buffer )

		static locals
			0 value wrd
			0 value quoted
			0 value parens
			0 value hyphenated
			0 value self
		end

		to self

		0 self words @ list:first @ my!
		begin my while
			my list:payload @ word:length + 6 +
			my list:post @ my!
		end
		allocate dup at!

		self length
		for
			i self words @ list:get to wrd

			\ start of paren
			parens 0= wrd word:paren? 0<> and
			if
				`( c!+
				true to parens
			end

			\ start of quote
			quoted 0= wrd word:quote? 0<> and
			if
				`" c!+
				true to quoted
			end

			wrd word:get dup at place count at + at!

			wrd word:punct? my! my
			if
				my word:bit-period    and if `. c!+ end
				my word:bit-comma     and if `, c!+ end
				my word:bit-question  and if `? c!+ end
				my word:bit-exclaim   and if `! c!+ end
				my word:bit-hyphen    and if `- c!+ end
				my word:bit-colon     and if `: c!+ end
				my word:bit-semicolon and if `; c!+ end
			end

			\ end of quote
			quoted
			if
				i 1+ self words @ list:get my!
				my 0= my if my word:quote? 0= or end
				if
					`" c!+
					false to quoted
				end
			end

			\ end of paren
			parens
			if
				i 1+ self words @ list:get my!
				my 0= my if my word:paren? 0= or end
				if
					`) c!+
					false to parens
				end
			end

			wrd word:hyphen? 0= i self length 1- < and if \s c!+ end
		end ;

	: bounds ( a -- )
		at! at cursor @ at length 1- min 0 max at cursor ! ;

	: current ( a -- p )
		at! at cursor @ at words @ list:get ;

	: insert_after ( a -- )
		at! word:construct my! my at cursor @ 1+ at words @ list:insert
		at current
		if
			at current word:quote? if my word:dquote end
			at current word:paren? if my word:dparen end
		end ;

	: insert_before ( a -- )
		at! word:construct my! my at cursor @ 1- 0 max at words @ list:insert 1 at cursor +!
		at current
		if
			at current word:quote? if my word:dquote end
			at current word:paren? if my word:dparen end
		end ;

	: current_blur ( a -- )
		at! at current dup word:blur word:empty? at length 0> and
		if at cursor @ at words @ list:remove word:destruct at bounds end ;

	: current_focus ( a -- )
		at! at length 0= if at insert_after end at current word:focus ;

	: cursor_set ( n a -- )
		at! at current_blur at cursor ! at bounds at current_focus ;

	: empty? ( a -- f )
		at! at words @ list:length my! my 1 = if at current word:empty? else my 0= end ;

	: start? ( a -- f )
		cursor @ 0= ;

	: end? ( a -- f )
		at! at cursor @ at length 1- = ;

	: focus ( a -- )
		current_focus ;

	: blur ( a -- )
		current_blur ;

	: left ( a -- )
		at! at cursor @ 0= if at insert_before end
		at cursor @ 1- at cursor_set ;

	: right ( a -- )
		at! at cursor @ at words @ list:length 1- = if at insert_after end
		at cursor @ 1+ at cursor_set ;

	: home ( a -- )
		at!
		at current word:row @ 1-
		at cursor @ at words @ list:node_by_index my!
		begin my while
			dup my list:payload @ word:row @ = until
			my list:prev @ my!
		end
		drop my
		if
			my at words @ list:index_by_node 1+ my!
			my at cursor @ <> if my at cursor_set exit end
		end
		0 at cursor_set ;

	: away ( a -- )
		at!
		at current word:row @ 1+
		at cursor @ at words @ list:node_by_index my!
		begin my while
			dup my list:payload @ word:row @ = until
			my list:post @ my!
		end
		drop my
		if
			my at words @ list:index_by_node 1- my!
			my at cursor @ <> if my at cursor_set exit end
		end
		at length 1- at cursor_set ;

	: back ( a -- )
		at! at current word:empty? if at left else at current word:back end ;

	: up ( a -- f )
		at! at current word:row @
		if
			at current word:col @ at current word:length 2/ +
			at current word:row @ 1-
			at cursor @ at words @ list:node_by_index my!
			begin my while
				dup  my list:payload @ word:row @ = until
				my list:prev @ my!
			end
			begin my while my list:prev @ while
				dup  my list:prev @ list:payload @ word:row @ = while
				over my list:payload @ word:col @ > until
				my list:prev @ my!
			end
			drop drop my
			if
				my at words @ list:index_by_node at cursor_set
				true exit
			end
			0 at cursor_set
		end
		false ;

	: down ( a -- f )
		at! at current word:row @
		if
			at current word:col @ at current word:length 2/ +
			at current word:row @ 1+
			at cursor @ at words @ list:node_by_index my!
			begin my while
				dup  my list:payload @ word:row @ = until
				my list:post @ my!
			end
			begin my while my list:post @ while
				dup  my list:post @ list:payload @ word:row @ = while
				over my list:payload @ word:col @ my list:payload @ word:length + < until
				my list:post @ my!
			end
			drop drop my
			if
				my at words @ list:index_by_node at cursor_set
				true exit
			end
			at length 1- at cursor_set
		end
		false ;

	: period    ( a -- ) current word:period ;
	: comma     ( a -- ) current word:comma ;
	: question  ( a -- ) current word:question ;
	: exclaim   ( a -- ) current word:exclaim ;
	: hyphen    ( a -- ) current word:hyphen ;
	: colon     ( a -- ) current word:colon ;
	: semicolon ( a -- ) current word:semicolon ;
	: dquote    ( a -- ) current word:dquote ;
	: dparen    ( a -- ) current word:dparen ;

	: split ( b a -- )
		at! my!
		my current_blur
		begin at cursor @ at words @ list:length 1- < while
			at words @ list:pop my words @ list:shove
		end
		my current_focus ;

	: join ( b a -- )
		at! my!
		my current_blur
		my words @ list:length
		for
			my words @ list:shift at words @ list:push
		end
		0 my cursor !
		my insert_after ;

	: construct ( -- a )
		fields allocate at!
		list:construct at words !
		at insert_after
		at ;

	: destruct ( a -- )
		at! at words @ my!
		begin my list:length while
			0 my list:remove word:destruct
		end
		at free ;

	create construct , does @ ;

: document ( -- )

	record fields
		cell field paragraphs
		cell field cursor
		 100 field name
	end

	: length ( a -- n )
		paragraphs @ list:length ;

	: bounds ( a -- )
		at! at cursor @ at paragraphs @ list:length 1- min 0 max at cursor ! ;

	: insert_after ( a -- )
		at! paragraph:construct at cursor @ 1+ at paragraphs @ list:insert ;

	: insert_before ( a -- )
		at! paragraph:construct at cursor @ 1- 0 max at paragraphs @ list:insert 1 at cursor +! ;

	: current ( a -- p )
		at! at cursor @ at paragraphs @ list:get ;

	: current_node ( a -- node )
		at! at cursor @ at paragraphs @ list:node_by_index ;

	: current_blur ( a -- )
		at! at current dup paragraph:blur paragraph:empty?
		if at cursor @ at paragraphs @ list:remove paragraph:destruct end ;

	: current_focus ( a -- )
		at! at length 0= if at insert_after end at current paragraph:focus ;

	: cursor_set ( n a -- )
		at! at current_blur at cursor ! at bounds at current_focus ;

	: start? ( a -- f )
		cursor @ 0= ;

	: end? ( a -- f )
		at! at cursor @ at length 1- = ;

	: up ( a -- )
		at! at start? if at insert_before end at cursor @ 1- at cursor_set ;

	: up_line ( a -- )
		at! at current paragraph:up 0= if at up end ;

	: down ( a -- )
		at! at end? if at insert_after end at cursor @ 1+ at cursor_set ;

	: down_line ( a -- )
		at! at current paragraph:down 0= if at down end ;

	: left ( a -- )
		current paragraph:left ;

	: right ( a -- )
		current paragraph:right ;

	: home ( a -- )
		current paragraph:home ;

	: away ( a -- )
		current paragraph:away ;

	: split ( a -- )
		at! at insert_after at current if at cursor @ 1+ at paragraphs @ list:get at current paragraph:split end ;

	: join ( a -- )
		at! at cursor @ 0> if at current at cursor @ 1- at paragraphs @ list:get paragraph:join end ;

	: enter ( a -- )
		at! at current paragraph:current word:empty?
		if at split at down else at current paragraph:right end ;

	: back ( a -- )
		at! at current paragraph:start? at current paragraph:current word:empty? and
		if at join at up else at current paragraph:back end ;

	: bar ( a -- )
		at! at current paragraph:insert_after at right ;

	: period    ( a -- ) current paragraph:period ;
	: comma     ( a -- ) current paragraph:comma ;
	: question  ( a -- ) current paragraph:question ;
	: exclaim   ( a -- ) current paragraph:exclaim ;
	: hyphen    ( a -- ) current paragraph:hyphen ;
	: colon     ( a -- ) current paragraph:colon ;
	: semicolon ( a -- ) current paragraph:semicolon ;
	: dquote    ( a -- ) current paragraph:dquote ;
	: dparen    ( a -- ) current paragraph:dparen ;

	: save ( a -- )
		push 0
		top length
		for
			i top paragraphs @ list:get paragraph:text my!
			my count + 1+ my free
		end
		1+ allocate dup at!
		top length
		for
			i top paragraphs @ list:get paragraph:text my!
			my at place my count at + at! \n c!+
		end
		0 c!+
		dup pop name blurt drop free ;

	: load ( a -- )

		static locals
			0 value unquote
			0 value unparen
		end

		push top name slurp dup at!
		begin at while c@+ my! my while

			my `. = if top period    next end
			my `, = if top comma     next end
			my `? = if top question  next end
			my `! = if top exclaim   next end
			my `: = if top colon     next end
			my `; = if top semicolon next end
			my `- = if top hyphen top bar next end
			my \n = if top enter top enter next end
			my \s = if top bar       next end

			unquote
			if
				top dquote
				false to unquote
			end

			my `" =
			if
				at c@ space? dup to unquote
				0= if
					top dquote
				end
				next
			end

			unparen
			if
				top dparen
				false to unparen
			end

			my `( =
			my `) = or
			if
				at c@ space? dup to unparen
				0= if
					top dparen
				end
				next
			end

			my 32 > my 127 < and
			if
				my top current paragraph:current word:append
				next
			end
		end
		pop drop free ;

	: dump ( a -- )
		at! at "[ document %x, " print at paragraphs @ list:length "paragraphs: %d" print " ]\n" type ;

	: construct ( -- a )
		fields allocate at!
		"default.txt" at name place
		list:construct at paragraphs !
		at insert_after at current paragraph:focus
		at ;

	: destruct ( a -- )
		at! at paragraphs @ my!
		begin my list:length while
			0 my list:remove paragraph:destruct
		end
		at free ;

	create construct , does @ ;

: display ( document -- )

	: blat ( s -- )
		here swap place, create , does @ type ;

	static sequences
		"\e[K"    blat erase
		"\e[?25h" blat cursor-on
		"\e[?25l" blat cursor-off
		"\e7"     blat cursor-save
		"\e8"     blat cursor-back
		"\e[?7h"  blat wrap-on
		"\e[?7l"  blat wrap-off
		\ Default 16 color mode
		"\e[39;22m" blat fg-normal
		"\e[33;22m" blat fg-current
		"\e[34;22m" blat fg-comment
		"\e[35;22m" blat fg-string
		"\e[49;22m" blat bg-normal
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
			F0h F0h 8Ch fg-256 'fg-current  re
			66h 66h 66h fg-256 'fg-comment  re
			82h AAh 8Ch fg-256 'fg-string   re
			00h 00h 00h bg-256 'bg-normal   re
		end ;

	static locals
		0 value row
		0 value col
		0 value width
		0 value margin

		0 value doc
		0 value pgf
		0 value wrd

		0 value cache_rows
		0 value cache_cols
	end

	: rows ( -- n )
		max-xy nip ;

	: cols ( -- n )
		max-xy drop ;

	rows to cache_rows
	cols to cache_cols

	: set-width ( n -- )
		50 max to width cols width - 2/ to margin ;

	: col+ ( -- )
		col 1+ to col ;

	: put ( c -- )
		col 0> row 0> col cache_cols < row cache_rows < and and and if emit else drop end ;

	: spaces ( n -- )
		for \s put col+ end ;

	: space ( -- )
		1 spaces ;

	: output ( a -- )
		at! begin c@+ my! my while my put col+ end ;

	: indent ( -- )
		margin spaces ;

	: row+ ( -- )
		row 1+ to row erase 0 to col col row at-xy bg-normal ;

	: half ( n -- m )
		dup 2 mod if 2/ 1+ else 2/ end ;

	: start ( -- )
		colors cursor-off wrap-off fg-normal bg-normal ;

	: stop ( -- )
		cursor-on wrap-on fg-normal bg-normal ;

	: paragraphs ( -- a )
		doc document:paragraphs @ ;

	: words ( -- a )
		pgf paragraph:words @ ;

	: pgf-current? ( -- f )
		pgf doc document:current = ;

	: wrd-current? ( -- f )
		wrd pgf paragraph:current = ;

	: pgf-rows ( -- n )

		: scan-eol ( buffer -- buffer' )
			dup c@ \s = if 1+ end
			dup at!
			begin c@+ my!
				my 0= if drop at 1- end
				my while
				i width <= while
				my \s = if drop at 1- end
				my `- = if drop at end
			end ;

		pgf paragraph:text 1 over at!
		begin at c@ while
			at scan-eol at! 1+
		end
		swap free ;

	: pgf-render ( -- )

		col row at-xy

		static locals
			create pad 32 allot
			0 value quoted
			0 value parens
			0 value hyphenated
		end

		false to quoted
		false to parens
		false to hyphenated

		indent
		words list:length
		for
			i words list:get to wrd

			wrd word:get dup c@ 0= if drop "_" end

			pad place

			wrd word:punct? my! my
			if
				pad count pad + at!
				my word:bit-period    and if `. c!+ end
				my word:bit-comma     and if `, c!+ end
				my word:bit-question  and if `? c!+ end
				my word:bit-exclaim   and if `! c!+ end
				my word:bit-hyphen    and if `- c!+ end
				my word:bit-colon     and if `: c!+ end
				my word:bit-semicolon and if `; c!+ end
				0 c!+
			end
			\ start of quote
			quoted 0= wrd word:quote? 0<> and
			if
				pad dup 1+ place pad at! `" c!+
				true to quoted
			end
			\ start of paren
			parens 0= wrd word:paren? 0<> and
			if
				pad dup 1+ place pad at! `( c!+
				true to parens
			end
			\ end of quote
			quoted
			if
				i 1+ words list:get my!
				my 0= my if my word:quote? 0= or end
				if
					pad count pad + at!
					`" c!+ 0 c!+
					false to quoted
				end
			end
			\ end of paren
			parens
			if
				i 1+ words list:get my!
				my 0= my if my word:paren? 0= or end
				if
					pad count pad + at!
					`) c!+ 0 c!+
					false to parens
				end
			end
			pad

			dup count col + width margin + >=
			if row+ indent end

			hyphenated 0= i 0> and col margin > and
			if space end

			col wrd word:col !
			row wrd word:row !

			begin
				pgf-current? wrd-current? and
				if fg-current leave end

				wrd word:quote?
				if fg-string leave end

				wrd word:paren?
				if fg-comment leave end

				fg-normal
				leave
			end

			output

			wrd word:hyphen? to hyphenated
		end
		row+ row+ ;

	width set-width

	to doc
	0 to row
	0 to col

	doc document:current to pgf
	cache_rows 2/ pgf-rows half - to row
	0 to col pgf-render

	row

	doc document:current to pgf
	cache_rows 2/ pgf-rows half - to row

	doc document:current_node list:prev @ my!
	begin my while
		my list:payload @ to pgf
		0 to col row pgf-rows - to row
		col row pgf-render to row to col
		row 0> while
		my list:prev @ my!
	end

	row

	0 to col 0 to row
	col row at-xy
	0 max for row+ end

	0 to col to row

	doc document:current_node list:post @ my!
	begin my while
		my list:payload @ to pgf
		pgf-render
		row cache_rows < while
		my list:post @ my!
	end
	col row at-xy
	cache_rows row - 0 max for row+ end

;

: main ( -- )

	static locals
		list documents
	end

	: the-doc ( -- a )
		documents.first @ list:payload @ ;

	: halt ( -- )
		display:stop the-doc document:save bye ;

	: insert ( c -- )
		the-doc document:current paragraph:current word:append ;

	: enter   ( -- ) the-doc document:enter ;
	: tab ;
	: back    ( -- ) the-doc document:back ;
	: up      ( -- ) the-doc document:up_line ;
	: pgup    ( -- ) the-doc document:up ;
	: down    ( -- ) the-doc document:down_line ;
	: pgdown  ( -- ) the-doc document:down ;
	: left    ( -- ) the-doc document:left ;
	: right   ( -- ) the-doc document:right ;
	: home    ( -- ) the-doc document:home ;
	: away    ( -- ) the-doc document:away ;
	: bar     ( -- ) the-doc document:bar ;

	: period    ( -- ) the-doc document:period ;
	: comma     ( -- ) the-doc document:comma ;
	: question  ( -- ) the-doc document:question ;
	: exclaim   ( -- ) the-doc document:exclaim ;
	: hyphen    ( -- ) the-doc document:hyphen ;
	: colon     ( -- ) the-doc document:colon ;
	: semicolon ( -- ) the-doc document:semicolon ;
	: dquote    ( -- ) the-doc document:dquote ;
	: dparen    ( -- ) the-doc document:dparen ;

	: idiots ( -- )
		edit:escseq dup count + 1- c@ `H = if home else away end ;

	: tilde ( -- )
		"[1~" edit:escseq? if home   exit end
		"[7~" edit:escseq? if home   exit end
		"[4~" edit:escseq? if away   exit end
		"[8~" edit:escseq? if away   exit end
\		"[2~" edit:escseq? if cmode  exit end
\		"[3~" edit:escseq? if del    exit end
		"[5~" edit:escseq? if pgup   exit end
		"[6~" edit:escseq? if pgdown exit end
		;

	static locals

		256 array ikey
		256 array iekey

		'enter    \n ikey !
		'tab      \t ikey !
		'back     \b ikey !
		'back    127 ikey !
		'bar      \s ikey !

		'period    `. ikey !
		'comma     `, ikey !
		'question  `? ikey !
		'exclaim   `! ikey !
		'hyphen    `- ikey !
		'colon     `: ikey !
		'semicolon `; ikey !
		'dquote    `" ikey !
		'dparen    `( ikey !
		'dparen    `) ikey !

		'up      `A iekey !
		'down    `B iekey !
		'right   `C iekey !
		'left    `D iekey !
		'home    `H iekey !
		'away    `F iekey !
		'idiots  `O iekey !
		'tilde   `~ iekey !

	end

	begin i 1+ arg while
		document:construct my!
		i 1+ arg my document:name place
		my document:load
		my documents.push
	end

	documents.length 0=
	if
		document:construct my!
		my document:load
		my documents.push
	end

	sys:pseudo-terminal
	sys:unbuffered
	display:start

	begin
		the-doc display
		key my!

		my \e =
		if
			edit:escape my!
			my \e =
			if
				halt
				next
			end

			my iekey @ execute
			next
		end

		my 32 > my 127 < and
		my `. <> and
		my `, <> and
		my `? <> and
		my `! <> and
		my `" <> and
		my `( <> and
		my `) <> and
		my `- <> and
		my `: <> and
		my `; <> and
		if
			my insert
			next
		end

		my ikey @ execute
		next
	end ;

\ Be friendly
1 arg "^--help$" match?
1 arg "^-h$"     match? or
if
	help bye
end

main
