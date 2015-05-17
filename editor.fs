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

\ Reforth Editor -- re
\
\ Sean Pringle <sean.pringle@gmail.com>, Jan 2013:
\
\ This editor is influenced by vi(m) but much simplified and designed to
\ take advantage of the Forth shell. It's also small and easy to hack!
\
\ I was never a vim power-user but did become attached to some of the
\ basic command-mode controls. The subset supported here allow me to
\ move between here and vim without tripping up... much. YMMV.

: help ( -- )

	macro

	\ Compile a comment as a string, and type it.
	: \ ( -- )
		sys:source @ dup "\n" match drop at! 0 c!+ dup at 1- <>
		if 1+ end string, 'type word, 'cr word, at sys:source ! ;

	normal

	\ Usage: re [file]
	\
	\ COMMAND mode uses macro editing controls and word-based navigation.
	\
	\ ------------------------------------------------------------
	\ | i            | Enter INSERT mode at the caret            |
	\ | o            | Insert a blank line and enter INSERT mode |
	\ | m            | Mark the current line                     |
	\ | c            | Add a virtual cursor                      |
	\ | s            | Remove virtual cursors                    |
	\ | y            | Copy from mark to current line            |
	\ | /            | Search for text by posix regex            |
	\ | \            | Set replacement text                      |
	\ | n            | Search for next occurrence                |
	\ | r            | Replace current search match              |
	\ | d            | Copy and delete from mark to current line |
	\ | p            | Paste copied text after current line      |
	\ | g            | Go to the marked line                     |
	\ | u            | Undo last change                          |
	\ | :            | Access the Forth shell                    |
	\ | Insert       | Enter INSERT mode (same as 'i')           |
	\ ------------------------------------------------------------
	\
	\ COMMAND mode keys can be modified with prefixes:
	\
	\ ------------------------------------------------------------
	\ | 10g          | Go to line 10                             |
	\ | $g           | Go to last line (end of file)             |
	\ | 5d           | Delete 5 lines after the cursor           |
	\ ------------------------------------------------------------
	\
	\ Convenient Forth shorthand words:
	\
	\ ------------------------------------------------------------
	\ | w            | Save the file                             |
	\ | q            | Exit the editor                           |
	\ | wq           | w q                                       |
	\ | ?            | Wait for a key stroke                     |
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
	\ (0) -- INSERT -- 77,35 s[alpha] r[beta] c[20] editor.fs
	\
	\ ------------------------------------------------------------
	\ | (0)          | Forth Stack (zero items)                  |
	\ | -- INSERT -- | Editor mode                               |
	\ | 77           | Current line                              |
	\ | 35           | Current column                            |
	\ | n[alpha]     | Current search regex is "alpha"           |
	\ | r[beta]      | Current replace string is "beta"          |
	\ | c[20]        | Current command mode key modifier         |
	\ | <path>       | Current file                              |
	\ ------------------------------------------------------------
;

 0 value file
 0 value caret
 0 value size
 0 value mode
-1 value marker
 4 value tabsize

create name 100 allot
create tmp  100 allot
create cmd  100 allot

"HOME" getenv "%s/.reclip"
format string clipboard

stack undos
stack redos

: bounds? ( n -- n' )
	dup 0 < swap size >= or ;

: bounds ( n -- n' )
	size min 0 max ;

: ncopy ( a n -- b )
	dup my! 1+ allocate dup at! my cmove 0 at my + c! at ;

: fcopy ( -- a )
	file size ncopy ;

: copy ( a -- b )
	dup count ncopy ;

: match? ( s p -- f )
	match nip ;

: white? ( c -- f )
	dup \s = swap \t = or ;

: cscan ( a c -- a' )
	my! begin dup c@ dup my = swap 0= or until 1+ end ;

: cskip ( a c -- a' )
	my! begin dup c@ my = while 1+ end ;

: rows ( -- n )
	max-xy nip ;

: cols ( -- n )
	max-xy drop ;

: undo! ( -- )
	undos.top file 0 compare if caret undos.push fcopy undos.push end ;

: redo! ( -- )
	redos.top file 0 compare if caret redos.push fcopy redos.push end ;

: expand ( -- )
	size 1+ to size file size 1+ resize to file ;

: shrink ( -- )
	size 1- to size caret bounds to caret ;

: point ( -- a )
	file caret + ;

: char ( -- c )
	point c@ ;

: line ( -- n )
	0 file at! caret for c@+ \n = if 1+ end end ;

: lines ( -- n )
	1 file at! size for c@+ \n = if 1+ end end ;

: insert ( c -- )
	expand point dup 1+ place point c! ;

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

: goto ( l -- )
	0 to caret for away right end ;

: inserts ( text -- )
	at! caret begin c@+ dup while insert right end drop to caret ;

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
	caret here at! at range for char c!+ right end 0 c!+ clip to caret ;

: paste ( -- )
	away right clipboard slurp dup inserts free ;

: delete ( -- )
	here at! at range for remove c!+ end 0 c!+ clip ;

: rename ( name -- )
	name place name basename "\e]0;%s\a" print ;

: close ( -- )
	0 to caret unmark 0 to size 0 file c! ;

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
	redo! undos.pop dup if revert undos.pop to caret else drop end ;

: redo ( -- )
	undo! redos.pop dup if revert redos.pop to caret else drop end ;

create srch 100 allot
create repl 100 allot

: stringlit ( s -- a )
	sys:source @ swap "/%s/" format sys:source ! sys:sparse swap sys:source ! ;

: search ( -- )

	: forward ( a -- )
		file - bounds to caret ;

	srch stringlit tmp place

	right
	point tmp match if forward exit end drop
	file  tmp match if forward exit end drop
	left ;

: replace ( -- )
	point srch stringlit match swap point = and
	if
		point dup srch stringlit split
		drop swap place repl stringlit inserts
	end ;

: blat ( s -- )
	here swap place, create , does @ type ;

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
"\e[31;22m" blat fg-define
"\e[39;22m" blat fg-status
"\e[49;22m" blat bg-normal
"\e[49;1m"  blat bg-active
"\e[47;22m" blat bg-marked
"\e[49;1m"  blat bg-status

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
		82h AAh 8Ch fg-256 'fg-comment  re
		C8h 91h 91h fg-256 'fg-string   re
		8Ch C8h C8h fg-256 'fg-number   re
		F0h DCh AFh fg-256 'fg-keyword  re
		D8h D8h D8h fg-256 'fg-coreword re
		C0h BEh D0h fg-256 'fg-variable re
		F0h F0h 8Ch fg-256 'fg-define   re
		FFh FFh FFh fg-256 'fg-status   re
		30h 30h 30h bg-256 'bg-normal   re
		40h 40h 40h bg-256 'bg-active   re
		40h 40h 40h bg-256 'bg-marked   re
		00h 00h 00h bg-256 'bg-status   re
	end ;

: status ( -- )
	fg-status bg-status 0 rows at-xy ;

create input 100 allot

: menu ( options -- item )

	static locals
		0 value select
		0 value options
	end

	to options 0 to select 0 input !

	: eol ( a -- a' )
		\n cscan ;

	: sol ( a -- a' )
		\n cskip ;

	: hit? ( a -- f )
		input dup count compare 0= ;

	: hit+ ( a -- a' )
		input c@ if begin dup c@ while dup hit? until eol sol end end ;

	: input! ( a n -- )
		my! input my cmove 0 input my + c! ;

	: hit! ( -- )
		options hit+ select for eol hit+ end at! at eol at - my! my if at my input! end ;

	: sel! ( -- )
		options select for eol sol end dup eol over - input! ;

	: hits ( -- n )
		0 options begin hit+ at! at c@ while 1+ at eol sol end ;

	: items ( -- n )
		0 options begin at! at c@ while 1+ at eol sol end ;

	: item. ( a i -- )
		select = if bg-active end space at! at eol at - for c@+ emit end space bg-status ;

	: draw ( -- n )
		space space options begin hit+ dup c@ while dup i item. eol sol end drop ;

	: select+ ( n -- )
		select + input c@ if hits else items end 1- min 0 max to select ;

	input 100 edit:start
	begin
		status "menu> " type edit:show
		cursor-save draw cursor-back

		edit:step my!

		my \e =
		if
			edit:escseq dup count 1- + c@ my!
			my \e = if 0 edit:input ! leave end

			edit:away
			my `C = if  1 select+ next end
			my `D = if -1 select+ next end

			next
		end

		my \n =
		if
			input c@ 0<> hits 0> and if hit! leave end
			input c@ 0= if sel! leave end
			leave
		end
	end
	edit:stop drop input ;

: accept ( buf lim -- f )
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
			false leave
		end
		my 10 =
		if
			true leave
		end
	end
	edit:stop drop ;

: listen ( s -- f )
	status type 0 input ! input 100 accept ;

: command ( -- )
	"> " listen if wrap-on space input evaluate drop end ;

: srch! ( -- )
	"search> " listen if input srch place end ;

: repl! ( -- )
	"replace> " listen if input repl place end ;

: put ( c -- )
	drop ;

\ Hooks

: huh ( -- ) ;
: indent   ( -- ) ;
: complete ( -- a ) "notags!" ;
: syntax   ( -- ) ;
: clean    ( -- ) ;
: gap?     ( c -- f ) white? ;

: default ( -- )

	: syn ( -- )
		char put right ;

	: ind ( -- )
		caret up home tmp at!
		begin char white? while char c!+ right end
		0 c!+ to caret tmp at!
		begin c@+ dup while insert right end drop ;

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

		static locals
			stack words
		end

		: keep ( s -- )
			at! words.depth
			for
				i cells words.base + @ at at count compare 0=
				if exit end
			end
			at copy words.push ;

		begin
			words.depth while
			words.pop free
		end

		file copy dup at!
		begin
			at c@ while
			at "\s+" split
			at keep swap at! while
		end
		free

		: cmp ( s1 s2 -- f )
			dup count compare 0< ;

		'cmp words.base words.depth sort

		here
		words.depth
		for
			i cells words.base + @
			dup count push over place pop +
			at! \n c!+ at
		end
		drop here ;

	'ind is indent
	'cln is clean
	'com is complete
	'syn is syntax ;

: reforth ( -- )

	: syn ( -- )

		: shunt ( -- )
			char put right ;

		: cword? ( c -- )
			point at! c@+ = c@+ space? and ;

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

		point "^(if|else|end|for|i|begin|while|until|exit|leave|next|value|variable|create|does|record|field|static|to|is|;)\s" match?
		if fg-keyword word exit end

		point "^[-]?[0-9a-fA-F]+[hb]?\s" match?
		if fg-number word exit end

		fg-normal word ;

	default
	'syn is syntax ;

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

		: comment2 ( -- )
			begin char while point shunt "^\*/" match? if shunt leave end end ;

		: word ( -- )
			whites begin char name? while shunt end ;

		: string ( delim -- )
			char shunt begin char my! my while shunt my over = until my `\ = if shunt end end drop ;

		whites char 0= if exit end
		point at! c@+ my!

		my `/ = at c@ `/ = and
		if fg-comment comment exit end

		my `/ = at c@ `* = and
		if fg-comment comment2 exit end

		my `" = my `' = or
		if fg-string string exit end

		my `$ =
		if fg-variable shunt word exit end

		point "^(function|class)\s+[^[:blank:]]+" match?
		if fg-keyword word fg-define word exit end

		point "^(if|else|elsif|switch|case|for|foreach|while|function|class|return|var|new|public|private|static|const)[^a-zA-Z0-9_]" match?
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
		default:rtrim default:rtabs ;

	default
	'gap is gap?
	'cln is clean
	'syn is syntax ;

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
		point at! c@+ my!

		my `/ = at c@ `/ = and
		if fg-comment comment exit end

		my `/ = at c@ `* = and
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
	'gap is gap?
	'syn is syntax ;

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
	'gap is do-gap?
	'cln is do-clean
	'syn is do-syntax ;

: detect ( -- )
	name "\.fs$"    match? if reforth exit end
	name "\.php$"   match? if php     exit end
	name "\.c$"     match? if c99     exit end
	name "\.html?$" match? if html    exit end
	default ;

: display ( -- )

	static locals
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
	end

	: counter ( -- )
		caret cursor = if col to cur-col row to cur-row end ;

	: col+ ( -- )
		col 1+ to col ;

	: spaces ( n -- )
		for \s emit col+ end ;

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

	'cput is put

	    0 to row
	    0 to col
	caret to cursor

	: finish ( -- n )
		start to caret rows for away right end caret 1- cursor to caret ;

	: restart ( rows -- )
		home for left home end caret to start line to sline cursor to caret ;

	: jump ( -- rows )
		rows 2/ 2/ ;

	: start! ( -- )
		caret  start < if jump     restart exit end
		finish caret < if jump 3 * restart exit end ;

	: cline! ( -- )
		sline file start + at! caret start - for c@+ \n = if 1+ end end to cline ;

	: mline! ( -- )
		marker start >
		marker start - rows cols * < and
		if
			sline file start + at! marker start -
			for c@+ \n = if 1+ end end to mline
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
		srch c@ if srch " n[%s]" print end
		srch c@ if repl " r[%s]" print end
		cmd  c@ if cmd  " c[%s]" print end
		name basename " %s" print erase ;

	: cleanup ( -- )
		cursor to caret cur-col cur-row at-xy cursor-on ;

	start! cline! mline! discard! setup text gap statusbar cleanup ;

: main ( -- )

	: cmd! ( c -- )
		my! cmd my `$ <> if dup count + end at! my c!+ 0 c!+ ;

	: times ( -- n )
		cmd c@ `$ = if lines exit end cmd number drop ;

	: times1 ( -- n )
		times 1 max ;

	: reset ( -- )
		0 cmd ! ;

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
		edit:escseq dup count + 1- c@ `H = if home else away end ;

	: search! ( -- )
		srch! search ;

	: replace! ( -- )
		repl! ;

	: newline ( -- )
		away enter caret times1 1- for enter end to caret imode ;

	: gline ( -- )
		cmd c@ if times goto exit end
		marker 1+ 0> if marker to caret exit end 0 to caret ;

	: tagpick ( -- )
		undo! complete menu dup inserts count for right end imode ;

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
		cmd c@
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

	static locals

		256 array ckey
		256 array cekey
		256 array ikey
		256 array iekey

		'reset    \b ckey !
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
		'gline    `g ckey !
		'gline    `G ckey !

		'command  `: ckey !
		'tagpick  \t ckey !
		'search!  `/ ckey !
		'replace! `\ ckey !

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

	end

	sys:pseudo-terminal
	sys:unbuffered

	begin
		reset display
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
			my digit? my `$ = or while
			my cmd! display key my!
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
	end ;

: q! bye ;
: q q! ;
: w write ;
: wq w q ;
: ? key drop ;

colors default
1000 allocate to file
close "untitled" rename

\ Be friendly
1 arg "^--help$" match?
1 arg "^-h$"     match? or
if
	help bye
end

"HOME" getenv "%s/.rerc" format included

\ Try to open a file
1 arg
if
	1 arg open detect
end

\ Accept initial stdin as content
begin
	key? while
	key dup my! while
	my insert right
end

main
