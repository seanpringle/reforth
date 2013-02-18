\ Reforth Editor -- re
\
\ **********************************************************************
\
\ MIT/X11 License
\ Copyright (c) 2012 Sean Pringle <sean.pringle@gmail.com>
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
\ IN NO EVENT SHALL THE AUTHOrs OR COPYRIGHT HOLDErs BE LIABLE FOR ANY
\ CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
\ TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
\ SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
\
\
\ Sean Pringle <sean.pringle@gmail.com>, Jan 2013:
\
\ This editor is influenced by vi(m) but much simplified.
\
\ I was never a vim power-user but did become attached to some of the
\ basic command-mode controls. The subset supported here allow me to
\ move between here and vim without tripping up... much. YMMV.
\
\ COMMAND mode uses macro editing controls and word-based navigation.
\
\ ------------------------------------------------------------
\ | i            | Enter INSERT mode at the caret            |
\ | o            | Insert a blank line and enter INSERT mode |
\ | /            | Search for text by posix regex            |
\ | n            | Search for next occurrence                |
\ | \            | Set replacement text                      |
\ | r            | Replace current search match              |
\ | m            | Mark the current line                     |
\ | y            | Copy from mark to current line            |
\ | d            | Copy and delete from mark to current line |
\ | p            | Paste copied text after current line      |
\ | g            | Go to the marked line                     |
\ | :            | Access the Forth shell                    |
\ | Insert       | Enter INSERT mode (same as 'i')           |
\ | Tab          | Open the menu                             |
\ ------------------------------------------------------------
\
\ INSERT Mode uses "normal" text editing controls and navigation.
\
\ ------------------------------------------------------------
\ | Escape       | Exit INSERT mode                          |
\ | Insert       | Exit INSERT mode                          |
\ ------------------------------------------------------------
\
\ An example status line:
\
\ (0) -- INSERT -- 77,35 s[alpha] r[beta]
\
\ ------------------------------------------------------------
\ | (0)          | Forth Stack (zero items)                  |
\ | -- INSERT -- | Editor mode                               |
\ | 77           | Current line                              |
\ | 35           | Current column                            |
\ | s[alpha]     | Current search regex is "alpha"           |
\ | r[beta]      | Current replace string is "beta"          |
\ ------------------------------------------------------------

10 value \n
13 value \r
 9 value \t
 7 value \a
27 value \e
 8 value \b
32 value \s

: sort ( xt a n -- )

	rot value cmp

	: 2dup  over over ;
	: 2drop drop drop ;

	: mid ( l r -- mid )
		over - 2/ -1 cells and + ;

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

	dup 1 > if 1- cells over + qsort else 2drop end ;

: stack ( -- )

	record fields
		field size
		field data
	end

	: last ( a -- b )
		at! at size @ 1- 0 max cells at data @ + ;

	: inc ( a -- )
		at! at size @ 1+ at data @ over cells resize at data ! at size ! ;

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

	: create ( -- a )
		fields allocate at! 0 at size !
		1 cells allocate at data ! at ;

	create
;

: print ( s -- )
	format type ;

: . ( n -- )
	"%d " print ;

: cr ( -- )
	10 emit ;

: space ( -- )
	32 emit ;

: .s ( -- )
	depth dup "(%d) " print for depth 1- i - pick "%d " print end ;

: copy ( a -- b )
	dup count 1+ allocate tuck place ;

: match? ( s p -- f )
	match nip ;

: digit? ( c -- f )
	dup 47 > swap 58 < and ;

: alpha? ( c -- f )
	my! my 64 > my 91 < and my 96 > my 123 < and or ;

: space? ( c -- f )
	my! my 32 = my 9 = or my 10 = or my 13 = or ;

: hex? ( c -- f )
	my! my 64 > my 71 < and my 96 > my 103 < and or my digit? or ;

: white? ( c -- f )
	dup \s = swap \t = or ;

: cscan ( a c -- a' )
	my! begin dup b@ dup my = swap 0= or until 1+ end ;

: cskip ( a c -- a' )
	my! begin dup b@ my = while 1+ end ;

: basename ( a -- a' )
	dup "/[^/]+$" match if nip 1+ else drop end ;

: rows ( -- n )
	max-xy nip ;

: cols ( -- n )
	max-xy drop ;

 0 value file
 0 value caret
 0 value size
 0 value mode
-1 value marker
 4 value tabsize

  100 allocate value name
  100 allocate value tmp
32768 allocate value here

"HOME" getenv "%s/.reclip"
format copy value clipboard

: edit ( -- )

	0 value caret
	0 value input
	0 value limit

	 50 allocate value escseq
	256 cell array ekeys

	: escape ( -- c )

		: last ( -- c )
			 escseq dup dup count + 1- max b@ ;

		: read ( -- )
			 key escseq dup count + at! b!+ 0 b!+ ;

		: done? ( -- f )
			 last dup `@ >= swap `~ <= and ;

		: start? ( -- f )
			 last `[ = ;

		: more ( -- )
			 read start? if begin read done? until end end ;

		: more? ( -- f )
			 0 25 for key? or dup until 1000 microsleep end ;

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
		point dup dup 1+ place b! ;

	: del ( --  c )
		point at! at b@ dup if at 1+ at place end ;

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

	'right `C ekeys !
	'left  `D ekeys !
	'tilde `~ ekeys !

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
; edit

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

stack 'stack wrap undos
stack 'stack wrap redos

: undo! ( -- )
	undos.top file 0 compare if file copy undos.push end ;

: redo! ( -- )
	redos.top file 0 compare if file copy redos.push end ;

: expand ( -- )
	size 1+ to size file size 1+ resize to file ;

: shrink ( -- )
	size 1- to size caret size min 0 max to caret ;

: point ( -- a )
	file caret + ;

: char ( -- c )
	point b@ ;

: line ( -- n )
	0 file at! caret for b@+ \n = if 1+ end end ;

: lines ( -- n )
	1 file at! size for b@+ \n = if 1+ end end ;

: insert ( c -- )
	expand point dup 1+ place point b! ;

: remove ( -- c )
	char caret size < if point dup 1+ swap place shrink end ;

: left ( -- )
	caret 1- 0 max to caret ;

: right ( -- )
	caret 1+ size min to caret ;

: home ( -- )
	caret for left char right \n = until left end ;

: away ( -- )
	point \n cscan file - to caret ;

: position ( -- n )
	caret home caret over to caret - ;

: reposition ( n -- )
	home for caret size < while char \n = until right end ;

: up ( -- )
	position home left reposition ;

: down ( -- )
	position away right reposition ;

: pgup ( -- )
	rows 2/ for up end ;

: pgdown ( -- )
	rows 2/ for down end ;

: inserts ( text -- )
	at! caret begin b@+ dup while insert right end drop to caret ;

: mark ( -- )
	caret home caret to marker to caret ;

: unmark ( -- )
	-1 to marker ;

: remark ( -- )
	marker 0< if mark end marker ;

: range ( -- n )
	remark away right caret over over min to caret - abs unmark ;

: clip ( s -- )
	clipboard blurt drop ;

: yank ( -- )
	caret here at! at range for char b!+ right end 0 b!+ clip to caret ;

: paste ( -- )
	away right clipboard slurp dup inserts free ;

: delete ( -- )
	here at! at range for remove b!+ end 0 b!+ clip ;

: rename ( name -- )
	name place name basename "\e]0;%s\a" print ;

: close ( -- )
	0 to caret unmark 0 to size 0 file b! ;

: read ( -- )
	name slurp at! at if at inserts end ;

: open ( name -- )
	close rename read ;

: reopen ( -- )
	close read ;

: write ( -- )
	file name blurt drop ;

: revert ( s -- )
	caret swap close inserts size min to caret ;

: undo ( -- )
	redo! undos.pop dup if revert else drop end ;

: redo ( -- )
	undo! redos.pop dup if revert else drop end ;

: blat ( a -- )
	type ;

"\e[K"      'blat bind erase
"\e[?25h"   'blat bind cursor-on
"\e[?25l"   'blat bind cursor-off
"\e7"       'blat bind cursor-save
"\e8"       'blat bind cursor-back
"\e[?7h"    'blat bind wrap-on
"\e[?7l"    'blat bind wrap-off

\ Default 16 color mode
"\e[39;22m" 'blat bind fg-normal
"\e[34;22m" 'blat bind fg-comment
"\e[35;22m" 'blat bind fg-string
"\e[36;22m" 'blat bind fg-number
"\e[33;22m" 'blat bind fg-keyword
"\e[39;22m" 'blat bind fg-coreword
"\e[39;22m" 'blat bind fg-variable
"\e[31;22m" 'blat bind fg-define
"\e[39;22m" 'blat bind fg-status
"\e[49;22m" 'blat bind bg-normal
"\e[49;1m"  'blat bind bg-active
"\e[47;22m" 'blat bind bg-marked
"\e[49;1m"  'blat bind bg-status

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
		"\e[%d;5;%dm" format copy ;

	: fg-256 ( r g b -- a )
		xterm-color 38 escseq ;

	: bg-256 ( r g b -- a )
		xterm-color 48 escseq ;

	\ xterm 256-color mode
	"TERM"      getenv "256color"       match?
	"COLORTERM" getenv "gnome-terminal" match? or
	if
		D8h D8h D8h fg-256 to fg-normal
		82h AAh 8Ch fg-256 to fg-comment
		C8h 91h 91h fg-256 to fg-string
		8Ch C8h C8h fg-256 to fg-number
		F0h DCh AFh fg-256 to fg-keyword
		D8h D8h D8h fg-256 to fg-coreword
		C0h BEh D0h fg-256 to fg-variable
		F0h F0h 8Ch fg-256 to fg-define
		FFh FFh FFh fg-256 to fg-status
		30h 30h 30h bg-256 to bg-normal
		40h 40h 40h bg-256 to bg-active
		40h 40h 40h bg-256 to bg-marked
		00h 00h 00h bg-256 to bg-status
	end ;

: status ( -- )
	fg-status bg-status 0 rows at-xy ;

100 allocate value input

: menu ( options -- item )

	  value options
	0 value select

	0 input !

	: eol ( a -- a' )
		\n cscan ;

	: sol ( a -- a' )
		\n cskip ;

	: hit? ( a -- f )
		input dup count compare 0= ;

	: hit+ ( a -- a' )
		input b@ if begin dup b@ while dup hit? until eol sol end end ;

	: input! ( a n -- )
		my! input my move 0 input my + b! ;

	: hit! ( -- )
		options hit+ select for eol hit+ end at! at eol at - my! my if at my input! end ;

	: sel! ( -- )
		options select for eol sol end dup eol over - input! ;

	: hits ( -- n )
		0 options begin hit+ at! at b@ while 1+ at eol sol end ;

	: items ( -- n )
		0 options begin at! at b@ while 1+ at eol sol end ;

	: item. ( a i -- )
		select = if bg-active end space at! at eol at - for b@+ emit end space bg-status ;

	: draw ( -- n )
		space space options begin hit+ dup b@ while dup i item. eol sol end drop ;

	: select+ ( n -- )
		select + input b@ if hits else items end 1- min 0 max to select ;

	input 100 edit:start
	begin
		status "menu> " type edit:show
		cursor-save draw cursor-back

		edit:step my!

		my \e =
		if
			edit:escseq dup count 1- + b@ my!
			my \e = if 0 edit:input ! leave end

			edit:away
			my `C = if  1 select+ next end
			my `D = if -1 select+ next end

			next
		end

		my \n =
		if
			input b@ 0<> hits 0> and if hit! leave end
			input b@ 0= if sel! leave end
			leave
		end
	end
	edit:stop drop input ;

: listen ( s -- f )
	status type 0 input ! input 100 accept ;

100 allocate value srch
100 allocate value repl

: stringlit ( s -- a )

	: put ( c -- )
		tmp count tmp + at! b!+ 0 b!+ ;

	0 tmp ! at!
	begin
		b@+ my!
		my while
		my `\ = at "^[nreat]" match? and
		if
			b@+ my!
			my `n = if \n put next end
			my `r = if \r put next end
			my `e = if \e put next end
			my `a = if \a put next end
			my `t = if \t put next end
		else
			my put
		end
	end
	tmp ;

: search ( -- )

	: goto ( a -- )
		file - size min 0 max to caret ;

	srch stringlit at!

	right
	point at match if goto exit end drop
	file  at match if goto exit end drop
	left ;

: replace ( -- )
	point srch stringlit match swap point = and
	if
		point dup srch stringlit split
		drop swap place repl stringlit inserts
	end ;

: srch! ( -- )
	"search> " listen if input srch place end ;

: repl! ( -- )
	"replace> " listen if input repl place end ;


0 value do-indent   : indent   do-indent   execute ;
0 value do-complete : complete do-complete execute ;
0 value do-syntax   : syntax   do-syntax   execute ;
0 value do-clean    : clean    do-clean    execute ;
0 value do-gap?     : gap?     do-gap?     execute ;
0 value do-put      : put      do-put      execute ;

: default ( -- )

	: syn ( -- )
		char put right ;

	: ind ( -- )
		caret up home tmp at!
		begin char white? while char b!+ right end
		0 b!+ to caret tmp at!
		begin b@+ dup while insert right end drop ;

	: rtrim ( -- )
		caret
		0 to caret
		begin
			char while away 0
			begin left char white? while 1+ end
			right my! my for remove drop end
			caret over < if my - end right
		end
		to caret ;

	: rtabs ( -- )
		caret
		0 to caret
		begin
			char while
			char \t =
			if
				remove drop
				tabsize for \s insert end
				caret over < if tabsize 1- + end
			end
			right
		end
		to caret ;

	: cln ( -- )
		rtrim ;

	: com ( -- a )

		0 value words

		words 0= if stack to words end

		: keep ( s -- )
			at! words stack:depth
			for
				i cells words stack:base + @ at at count compare 0=
				if exit end
			end
			at copy words stack:push ;

		begin
			words stack:depth while
			words stack:pop free
		end

		file copy at! at
		begin
			at b@ while
			at "\s+" split
			at keep swap at! while
		end
		free

		: cmp ( s1 s2 -- f )
			dup count compare 0< ;

		'cmp words stack:base words stack:depth sort

		here
		words stack:depth
		for
			i cells words stack:base + @
			dup count push over place pop +
			at! \n b!+ at
		end
		drop here ;

	'white? to do-gap?
	'ind to do-indent
	'cln to do-clean
	'com to do-complete
	'syn to do-syntax ;

: reforth ( -- )

	: syn ( -- )

		: shunt ( -- )
			char put right ;

		: cword? ( c -- )
			point at! b@+ = b@+ space? and ;

		: whites ( -- )
			begin char while char space? while shunt end ;

		: comment ( -- )
			whites begin char while char shunt `) = until end ;

		: comment2 ( -- )
			begin char while char \n = until shunt end ;

		: word ( -- )
			whites begin char while char space? until shunt end ;

		: string ( -- )
			shunt begin char my! my while shunt my `" = until my `\ = if shunt end end ;

		whites char 0= if exit end

		`( cword?
		if fg-comment comment exit end

		`\ cword?
		if fg-comment comment2 exit end

		`" char =
		if fg-string string exit end

		point "^:\s+[^[:blank:]]+" match?
		if fg-keyword word fg-define word exit end

		point "^(if|else|end|for|i|begin|while|until|exit|leave|next|value|bind|wrap|array|field|record|to|is|;)\s" match?
		if fg-keyword word exit end

		point "^[-]?[0-9a-fA-F]+[hb]?\s" match?
		if fg-number word exit end

		fg-normal word ;

	default
	'syn to do-syntax ;

: php ( -- )

	: syn ( -- )

		: shunt ( -- )
			char put right ;

		: name? ( c -- f )
			my! my alpha? my digit? my `_ = or or ;

		: whites ( -- )
			begin char while char space? while shunt end ;

		: comment ( -- )
			begin char while char \n = until shunt end ;

		: word ( -- )
			whites begin char name? while shunt end ;

		: string ( delim -- )
			char shunt begin char my! my while shunt my over = until my `\ = if shunt end end drop ;

		whites char 0= if exit end
		point at! b@+ my!

		my `/ = at b@ `/ = and
		if fg-comment comment exit end

		my `" = my `' = or
		if fg-string string exit end

		my `$ =
		if fg-variable shunt word exit end

		point "^(function|class)\s+[^[:blank:]]+" match?
		if fg-keyword word fg-define word exit end

		point "^(if|else|elsif|for|foreach|while|function|class|return|var|new)[^a-zA-Z0-9_]" match?
		if fg-keyword word exit end

		point "^[-]{0,1}(0x|[0-9]){1}[0-9a-fA-F]*" match?
		if fg-number shunt word exit end

		fg-normal

		my name?
		if word exit end

		shunt ;

	: gap ( c -- f )
		my! my white? my `, = or my `. = or my `( = or my `) = or ;

	: cln ( -- )
		default:rtrim name "corvus" match? 0= if default:rtabs end ;

	default
	'gap to do-gap?
	'cln to do-clean
	'syn to do-syntax ;

: c99 ( -- )

	: syn ( -- )

		: shunt ( -- )
			char put right ;

		: name? ( c -- f )
			my! my alpha? my digit? my `_ = or or ;

		: whites ( -- )
			begin char while char space? while shunt end ;

		: comment ( -- )
			begin char while char \n = until shunt end ;

		: comment2 ( -- )
			begin char while point shunt "^\*/" match? if shunt leave end end ;

		: word ( -- )
			whites begin char name? while shunt end ;

		: string ( delim -- )
			char shunt begin char my! my while shunt my over = until my `\ = if shunt end end drop ;

		whites char 0= if exit end
		point at! b@+ my!

		my `/ = at b@ `/ = and
		if fg-comment comment exit end

		my `/ = at b@ `* = and
		if fg-comment comment2 exit end

		my `" = my `' = or
		if fg-string string exit end

		my `# =
		if fg-define shunt word exit end

		point "^(if|else|elsif|for|while|return|int|char|void|typedef|struct|unsigned)[^a-zA-Z0-9_]" match?
		if fg-keyword word exit end

		point "^[-]{0,1}(0x|[0-9]){1}[0-9a-fA-F]*" match?
		if fg-number shunt word exit end

		fg-normal

		my name?
		if word exit end

		shunt ;

	: gap ( c -- f )
		my! my white? my `, = or my `. = or my `( = or my `) = or ;

	default
	'gap to do-gap?
	'syn to do-syntax ;

: html ( -- )

	: syn ( -- )

		: shunt ( -- )
			char put right ;

		: whites ( -- )
			begin char while char space? while shunt end ;

		: tag ( -- )
			whites begin char alpha? char digit? or  while shunt end ;

		: string ( delim -- )
			char shunt begin char my! my while shunt my over = until my `\ = if shunt end end drop ;

		whites char my!
		my 0= if exit end

		my `" = my `' = or
		if fg-string string exit end

		my `< =
		if
			point "^<[a-zA-Z]+.*>" match?
			if shunt fg-keyword tag exit end

			point "^</[a-zA-Z]+>" match?
			if shunt shunt fg-keyword tag exit end
		end

		point "^[a-zA-Z]+=" match?
		if fg-variable tag exit end

		fg-normal shunt ;

	: gap ( c -- f )
		my! my white? my `> = or my `< = or my `= = or ;

	: cln ( -- )
		default:rtrim name "corvus" match? 0= if default:rtabs end ;

	default
	'gap to do-gap?
	'cln to do-clean
	'syn to do-syntax ;

: detect ( -- )
	name "\.fs$"
	match? if reforth exit end
	name "\.php$"
	match? if php     exit end
	name "\.c$"
	match? if c99     exit end
	name "\.(html|htm|tpl)$"
	match? if html    exit end
	default ;

: display ( -- )

	0 value row
	0 value col
	0 value start
	0 value sline
	0 value cline
	0 value mline
	0 value cursor
	0 value cur-row
	0 value cur-col
	0 value discard
	0 value discarded

	: counter ( -- )
		caret cursor = if col to cur-col row to cur-row end ;

	: col+ ( -- )
		col 1+ to col ;

	: spaces ( n -- )
		for 32 emit col+ end ;

	: bg ( -- )
		sline row + dup cline = swap mline = or if bg-active else bg-normal end ;

	: row+ ( -- )
		row 1+ to row erase 0 to col 0 to discarded \n emit bg ;

	: tab+ ( -- )
		tabsize col tabsize mod - spaces ;

	: discard? ( -- f )
		discarded 1+ discard < dup if discarded 1+ to discarded end ;

	: cput ( c -- ) my! counter
		row rows = if      exit end
		my \n    = if row+ exit end
		discard?   if      exit end
		col cols = if      exit end
		my \t    = if tab+ exit end
		my emit col+ ;

	: finish ( -- n )
		start to caret rows for away right end caret 1- cursor to caret ;

	: restart ( rows -- )
		home for left home end caret to start line to sline cursor to caret ;

	: jump ( -- rows )
		rows 2/ 2/ ;

	: start! ( -- )
		'cput to do-put

		    0 to row
		    0 to col
		caret to cursor

		caret  start < if jump     restart exit end
		finish caret < if jump 3 * restart exit end ;

	: cline! ( -- )
		sline file start + at! caret start - for b@+ \n = if 1+ end end to cline ;

	: mline! ( -- )
		marker start >
		marker start - rows cols * < and
		if
			sline file start + at! marker start -
			for b@+ \n = if 1+ end end to mline
		else
			-1 to mline
		end ;

	: discard! ( -- )
		caret tabsize + home caret - cols - to discard cursor to caret ;

	: setup ( -- )
		start to caret col row at-xy cursor-off wrap-off fg-normal bg ;

	: text ( -- )
		begin char while row rows < while syntax end \n put ;

	: gap ( -- )
		begin row rows < while row+ end cursor to caret ;

	: statusbar ( -- )
		fg-status bg-status .s
		mode if "-- INSERT --" else "-- COMMAND --" end type
		cur-col cline " %d,%d" print
		srch b@ if srch " n[%s]" print end
		srch b@ if repl " r[%s]" print end
		name basename " %s" print erase ;

	: cleanup ( -- )
		cursor to caret cur-col cur-row at-xy cursor-on ;

	: draw ( -- )
		start! cline! mline! discard! setup text gap statusbar cleanup ;

; display

: main ( -- )

	10 allocate value digits

	: digit! ( c -- )
		digits dup count + at! b!+ 0 b!+ ;

	: times ( -- n )
		digits number drop ;

	: times1 ( -- n )
		times 1 max ;

	: reset ( -- )
		0 digits ! ;

	: imode ( -- )
		reset undo! 1 to mode unmark ;

	: cmode ( -- )
		reset 0 to mode clean unmark ;

	: imode1 ( -- )
		right imode ;

	: enter ( -- )
		undo! \n insert right indent ;

	: tab ( -- )
		\t insert right ;

	: del ( -- )
		remove drop ;

	: back ( -- )
		left del ;

	: idiots ( -- )
		edit:escseq dup count + 1- b@ `H = if home else away end ;

	: search! ( -- )
		srch! search ;

	: replace! ( -- )
		repl! ;

	: newline ( -- )
		away enter caret times1 1- for enter end to caret imode ;

	: goto ( -- )
		digits b@ if 0 to caret times for down end exit end
		marker 1+ 0> if marker to caret exit end 0 to caret ;

	: gend ( -- )
		size to caret home ;

	: tagpick ( -- )
		undo! complete menu dup inserts count for right end ;

	: go-up ( -- )
		times1 for up end ;

	: go-down ( -- )
		times1 for down end ;

	: go-right ( -- )
		times1 for
			char gap? right char gap? and
			if   begin char gap? while right end left
			else begin char while char gap? until char \n = until right end
			end
		end ;

	: go-left ( -- )
		times1 for
			left char gap?
			if   begin caret 0> while char gap? while left end right
			else begin caret 0> while char gap? until char \n = until left end
			end
		end ;

	: pastes ( -- )
		undo! times1 for paste end ;

	: deletes ( -- )
		undo! times1 for delete end ;

	: searches ( -- )
		times1 for search end ;

	: replaces ( -- )
		undo!
		digits b@
		if
			times1 for
				i if search end replace
			end
			exit
		end
		marker 1+ 0>
		if
			range caret tuck + my! 0
			begin search caret my < while 1+ end
			swap to caret for search replace end
			exit
		end
		replace ;

	: itilde ( -- )
		"[1~" edit:escseq? if home   exit end
		"[7~" edit:escseq? if home   exit end
		"[4~" edit:escseq? if away   exit end
		"[8~" edit:escseq? if away   exit end
		"[2~" edit:escseq? if cmode  exit end
		"[3~" edit:escseq? if del    exit end
		"[5~" edit:escseq? if pgup   exit end
		"[6~" edit:escseq? if pgdown exit end ;

	: ctilde ( -- )
		"[1~" edit:escseq? if home   exit end
		"[7~" edit:escseq? if home   exit end
		"[4~" edit:escseq? if away   exit end
		"[8~" edit:escseq? if away   exit end
		"[2~" edit:escseq? if imode  exit end
		"[5~" edit:escseq? if pgup   exit end
		"[6~" edit:escseq? if pgdown exit end ;

	: command ( -- )
		0 value quit
		"> " listen
		if
			input "^[[:digit:]]+$" match?
			if
				input digits place goto
				exit
			end
			input "^undo$" match? if undo exit end
			input "^redo$" match? if redo exit end
			input at!
			begin
				b@+ my! my while
				my `w = if write     next end
				my `q = if 1 to quit next end
			end
		end
		quit if cr "\ec" type bye end ;

	256 cell array ckey
	256 cell array cekey
	256 cell array ikey
	256 cell array iekey

	'mark     `m ckey !
	'mark     `M ckey !
	'yank     `y ckey !
	'yank     `Y ckey !
	'pastes   `p ckey !
	'pastes   `P ckey !
	'deletes  `d ckey !
	'deletes  `D ckey !
	'undo     `u ckey !
	'undo     `U ckey !
	'searches `n ckey !
	'searches `N ckey !
	'replaces `r ckey !
	'replaces `R ckey !
	'imode    `i ckey !
	'imode    `I ckey !
	'imode1   `a ckey !
	'imode1   `A ckey !
	'newline  `o ckey !
	'newline  `O ckey !
	'goto     `g ckey !
	'goto     `G ckey !
	'gend     `e ckey !
	'gend     `E ckey !

	'command  `: ckey !
	'tagpick  \t ckey !
	'search!  `/ ckey !
	'replace! `\ ckey !

	'go-up    `k ckey !
	'go-up    `K ckey !
	'go-down  `j ckey !
	'go-down  `J ckey !
	'go-right `l ckey !
	'go-right `L ckey !
	'go-left  `h ckey !
	'go-left  `H ckey !

	'go-up    `A cekey !
	'go-down  `B cekey !
	'go-right `C cekey !
	'go-left  `D cekey !
	'home     `H cekey !
	'away     `F cekey !
	'idiots   `O cekey !
	'ctilde   `~ cekey !

	'enter    \n ikey !
	'tab      \t ikey !
	'back     \b ikey !
	'back    127 ikey !

	'up      `A iekey !
	'down    `B iekey !
	'right   `C iekey !
	'left    `D iekey !
	'home    `H iekey !
	'away    `F iekey !
	'idiots  `O iekey !
	'itilde  `~ iekey !

	begin
		reset display:draw
		key my!

		mode
		if
			my \e =
			if
				edit:escape my!
				my \e =
				if
					cmode
					next
				end

				my iekey @ execute
				next
			end

			my 31 > my 127 < and
			if
				my insert right
				next
			end

			my ikey @ execute
			next
		end

		begin
			my digit? while
			my digit! key my!
		end

		my \e =
		if
			edit:escape my!
			my \e =
			if
				cmode
				next
			end

			my cekey @ execute
			next
		end

		my ckey @ execute
	end

;

default colors
1 cells allocate to file
1 arg if 1 arg open detect end

main

