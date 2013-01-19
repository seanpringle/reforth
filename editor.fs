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
\ **********************************************************************
\
\ Sean Pringle <sean.pringle@gmail.com>, Jan 2013:
\
\ This editor is influenced by vi(m) but much simplified and designed to
\ take advantage of the Forth shell. It's also small and easy to hack!
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
\ | :            | Access the Forth shell                    |
\ | Insert       | Enter INSERT mode (same as 'i')           |
\ ------------------------------------------------------------
\
\ Convenient Forth words:
\
\ ------------------------------------------------------------
\ | w            | Save the file                             |
\ | q            | Exit the editor                           |
\ | wq           | w q                                       |
\ | g ( n -- )   | Jump to line 'n'                          |
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


\ ***** Globals *****

 10 value \n
 13 value \r
 32 value \s
  9 value \t
 27 value \e
127 value \b
  0 value mode
  5 value tabsize
 20 array backups

"HOME" getenv "%s/.redclip"
format string clipboard

\ ***** Utility Words *****

: copy ( a -- b )
	dup count 1+ allocate tuck place ;

: ncopy ( a n -- b )
	my! at! my 1+ allocate at over my cmove 0 over my + c! ;

: match? ( subject pattern -- f )
	match nip ;

: msec ( n -- )
	1000 * usec ;

\ ***** Theme Colors *****

: color ( r g b -- )
	create swap rot , , , does at! @+ @+ @+ ;

\ Zenburn
D8h D8h D8h color fg-normal
55h DDh 55h color fg-active
CCh 99h 99h color fg-string
88h CCh CCh color fg-number
FFh DDh AAh color fg-keyword
88h AAh 88h color fg-comment
BBh BBh CCh color fg-coreword
FFh FFh FFh color fg-define
FFh FFh FFh color fg-status

39h 39h 39h color bg-normal
40h 40h 40h color bg-active
40h 39h 40h color bg-marked
30h 30h 30h color bg-status

\ ***** ANSI Escape Sequences *****

create last_fg 20 allot
create last_bg 20 allot

: fg ( r g b -- )
	swap rot "\e[38;2;%d;%d;%dm" format
	dup last_fg compare if dup type end last_fg place ;

: bg ( r g b -- )
	swap rot "\e[48;2;%d;%d;%dm" format
	dup last_bg compare if dup type end last_bg place ;

: erase ( -- )
	"\e[K" type ;

: fgbg ( -- )
	last_fg type last_bg type ;

: plain ( -- )
	"\e[0m" type fgbg ;

: underline ( -- )
	"\e[4m" type fgbg ;

: cursor ( state -- )
	if "\e[?25h" else "\e[?25l" end type ;

: wrap ( state -- )
	if "\e[?7h" else "\e[?7l" end type ;

: white? ( c -- f )
	dup \s = swap \t = or ;

: printable? ( c -- f )
	my! my 31 > my 127 < and my 9 = or ;

\ ***** File Handling *****

0 value file
0 value length
create name 100 allot

: load ( text -- )
	file free copy dup to file count to length ;

: rename ( name -- )
	name place ;

: save ( -- )
	file name blurt drop ;

: open ( name -- )
	rename name slurp load ;

: backup ( -- )
	backups.size @ 1- backups.get free file copy 0 backups.ins ;

: undo ( -- )
	0 backups.del dup if dup load end free ;

\ ***** Caret Manipulation *****

0 value caret
0 value caret-row
0 value caret-col

: pointer ( -- a )
	file caret + ;

: current ( -- c )
	pointer c@ ;

: sanity ( -- )
	caret 0 max length min to caret ;

: right ( -- )
	caret 1+ length min to caret ;

: left ( -- )
	caret 1- 0 max to caret ;

: length+ ( n -- )
	file swap length + dup to length 1+ resize to file ;

: length- ( n -- )
	length swap - 0 max to length sanity ;

: previous ( -- c )
	pointer file = if 0 else pointer 1- c@ end ;

: home ( -- )
	caret for left current \n = until end caret 0> if right end ;

: ending ( -- )
	length caret - for current \n = until right end ;

: position ( -- n )
	caret home caret over to caret - ;

: reposition ( n -- )
	home for current \n = until right end ;

: up ( -- )
	position home left reposition ;

: down ( -- )
	position ending right reposition ;

: pgup ( -- )
	max-xy nip 2/ for up end ;

: pgdown ( -- )
	max-xy nip 2/ for down end ;

: status ( -- )
	max-xy my! drop 0 my at-xy fg-status fg bg-status bg ;

\ ***** Text Manipulation *****

-1 value marked

: insert ( c -- )
	1 length+ pointer dup dup 1+ place c! ;

: remove ( -- c )
	pointer c@ caret length < if pointer 1+ pointer place 1 length- end ;

: mark ( -- )
	caret home caret to marked to caret ;

: remark ( -- )
	marked 0< if mark end ;

: unmark ( -- )
	-1 to marked ;

: inject ( str -- )
	at! backup caret ending right at count my! my length+
	pointer dup dup my + place at swap my cmove to caret ;

: paste ( -- )
	clipboard slurp at! at if at inject at free down end ;

: yank ( -- )
	remark caret marked file + at! ending right
	at caret marked - ncopy dup clipboard blurt drop
	free to caret unmark ;

: delete ( -- )
	backup remark
	marked file + at! ending right caret marked - my!
	at my ncopy dup clipboard blurt drop free
	pointer at place my length-
	marked to caret unmark ;

: complete ( -- )

	: go ( c -- )
		right insert left ;

	current `" = previous space? and
	if `" go exit end

	current `: = previous space? and
	if `; go \s go exit end

	current `( = previous space? and
	if `) go \s go exit end ;

\ ***** Search and Replace *****

create pattern 100 allot
create target  100 allot
create source  100 allot

: pattern@ ( s -- a )
	sys:source @ at! sys:source ! sys:sparse at sys:source ! ;

: target@ ( -- a )
	pattern target "\e%s\e" format pattern@ over place ;

: source@ ( -- a )
	pattern source "\e%s\e" format pattern@ over place ;

: search ( -- )

	: goto ( a -- )
		file - to caret ;

	right

	pointer target@ match if goto exit end drop
	file    target@ match if goto exit end drop

	left ;

: replace ( -- )
	pointer target@ match swap pointer = and
	if
		backup pointer copy at! at target@ split drop
		at - for remove drop end at free source@ at!
		begin c@+ dup while insert right end drop
	end ;

\ ***** Housekeeping *****

: clean ( -- )
	caret 0 to caret
	begin
		caret length < while ending
		begin left current white? while
			remove drop caret over < if 1- end
		end
		right right
	end
	to caret ;

: indent ( -- )

	static vars
		create pad 100 allot
	end

	: indent? ( c -- )
		dup white? swap `\ = or ;

	: indent@ ( -- )
		pad at! 99 for current indent? while current c!+ right end 0 c!+ ;

	: indent! ( -- )
		pad at! begin c@+ my! my while my insert right end ;

	caret up home indent@ to caret indent! ;

\ ***** Command Interpreter *****

create input 100 allot

: prompt ( s -- f )
	status erase type 0 input ! input 100 accept ;

: command ( -- )
	"> " prompt if true wrap \s emit input evaluate drop end ;

\ ***** Screen Rendering *****

: display ( -- )

	static vars

		0 value row
		0 value col
		0 value rows
		0 value cols

		0 value from
		0 value upto
		0 value last
		0 value line
		0 value active
		0 value counter
		0 value discard

		create pad 100 allot

	end

	: col+ ( -- )
		col 1+ cols min to col ;

	: row+ ( -- )
		erase 10 emit row 1+ rows min to row 0 to col ;

	: tab+ ( -- )
		tabsize col over mod - for \s emit col+ end ;

	: last-row? ( -- f )
		row rows 1- > ;

	: last-col? ( -- f )
		col cols 1- > ;

	: discard- ( -- )
		discard 1- 0 max to discard ;

	: counter+ ( -- )
		counter dup caret = if col to caret-col row to caret-row end 1+ to counter ;

	: put ( c -- ) my! counter+
		discard   if discard- exit end
		last-row? if          exit end
		my \n =   if row+     exit end
		last-col? if          exit end
		my \t =   if tab+     exit end
		my emit col+ ;

	: put+ ( a c -- a' )
		dup c@ if dup c@ put 1+ end ;

	: puts ( s -- )
		at! begin c@+ dup while put end drop ;

	: skip ( a -- a' )
		begin dup c@ dup while dup white? while put 1+ end drop ;

	: parse ( a -- a' )
		0 pad ! pad at! begin dup c@ dup while dup space? until c!+ 1+ end drop 0 !+ ;

	: sparse ( a -- a' )
		fg-string fg put+ at! begin c@+ dup while dup put dup `" = until `\ = if at put+ at! end end drop at ;

	: comment ( -- )
		fg-comment fg pad puts ;

	: comment1 ( a -- a' )
		comment begin dup c@ while skip dup c@ \n = until parse pad puts pad c@ `) = until end ;

	: comment2 ( a -- a' )
		comment at! begin c@+ dup while dup \n = until put end drop at 1- ;

	: colon ( a -- a' )
		skip parse fg-define fg pad puts ;

	: color ( -- )

		: go ( r g b -- )
			fg pad puts ;

		: is ( c -- f )
			pad at! c@+ = c@+ 0= and ;

		`( is if comment1 exit end
		`\ is if comment2 exit end

		\ detect colon definitions
		`: is if fg-keyword go colon exit end

		pad sys:macros sys:find if fg-keyword go exit end
		pad 'caret sys:xt-head sys:find if fg-coreword go exit end

		pad number nip if fg-number go exit end

		fg-normal go ;

	: word ( a -- a' )
		skip dup c@ `" = if sparse else parse color end ;

	: bcolor ( -- )
		counter active = if bg-active bg exit end
		counter marked = if bg-marked bg exit end
		bg-normal bg ;

	: width ( a -- n )
		at! 0 begin dup counter + caret = if leave end c@+ my! my 0= my \n = or if drop 0 leave end 1+ end ;

	: one-line ( a -- a' )
		bcolor dup width cols - 0 max to discard
		begin skip dup c@ while dup c@ \n = if 1+ leave end word end \n put ;

	: setup ( -- a )

		0 to row 0 to col
		max-xy to rows to cols

		col row at-xy
		plain false wrap false cursor

		: jump ( -- n )
			rows 2/ 2/ ;

		caret my!
		begin
			home caret to active my to caret

			\ can we skip recalc of display boundaries?
			caret last <> while
			caret last < caret from > and until
			caret last > caret upto < and upto from > and until

			\ caret has moved off screen. recalc screen boundaries
			caret from > if rows jump - else jump end
			home for up end caret to from my to caret
			0 file at! from for c@+ \n = if 1+ end end to line
			leave
		end
		-1    to caret-col
		-1    to caret-row
		caret to last
		from  to counter

		file from + ;

	: draw ( a -- a' )
		begin one-line row rows < while dup c@ while end ;

	: cleanup ( a -- )
		dup file - to upto bcolor \n put skip drop bg-normal bg
		rows row - 0 max for erase cr end ;

	: caret-line ( -- n )
		line from file + at! caret from - for c@+ \n = if 1+ end end ;

	: status-line ( -- )
		status .s
		mode if "-- INSERT --" else "-- COMMAND --" end type
		position caret-line " %d,%d" print
		target c@ if target " s[%s]" print end
		source c@ if source " r[%s]" print end
		erase ;

	: place-caret
		true cursor caret-col caret-row at-xy ;

	setup draw cleanup status-line place-caret ;

\ ***** Program Main Loop *****

: cycle ( -- )

	: imode ( -- ) true to mode ;
	: cmode ( -- ) false to mode clean ;

	: ekey_right ( -- ) drop right  ;
	: ekey_left  ( -- ) drop left   ;
	: ekey_up    ( -- ) drop up     ;
	: ekey_down  ( -- ) drop down   ;
	: ekey_home  ( -- ) drop home   ;
	: ekey_end   ( -- ) drop ending ;

	: key_del    ( -- ) remove drop ;
	: key_back   ( -- ) left key_del ;
	: key_enter  ( -- ) \n insert right indent ;
	: key_o      ( -- ) ending imode key_enter ;

 	: ekey_tilde ( escseq -- )
 		1+ c@ my!

 		my `5 = if pgup    exit end
 		my `6 = if pgdown  exit end
 		my `2 = if cmode   exit end
 		my `3 = if key_del exit end ;

	: com_insert  ( -- ) backup imode ;

	: com_search  ( -- ) "search> "  prompt if input target place search end ;
	: com_replace ( -- ) "replace> " prompt if input source place end ;

	: com_ekey_tilde ( escseq -- )
 		1+ c@ my!
		my `2 = if com_insert exit end
 		my `5 = if pgup   exit end
 		my `6 = if pgdown exit end ;

	static vars

		'drop 128 vector ekeys
		'nop  128 vector keys
		'drop 128 vector cekeys
		'nop  128 vector ckeys

		\ INSERT mode
 		'ekey_up     65 ekeys.set
 		'ekey_down   66 ekeys.set
 		'ekey_right  67 ekeys.set
 		'ekey_left   68 ekeys.set
 		'ekey_home   72 ekeys.set
 		'ekey_end    70 ekeys.set
 		'ekey_tilde 126 ekeys.set

		'key_back    \b keys.set
		'key_enter   \n keys.set

		\ COMMAND mode
 		'ekey_up     65 cekeys.set
 		'ekey_down   66 cekeys.set
 		'ekey_right  67 cekeys.set
 		'ekey_left   68 cekeys.set
 		'ekey_home   72 cekeys.set
 		'ekey_end    70 cekeys.set

 		'com_ekey_tilde 126 cekeys.set

		'com_insert  `i ckeys.set
		'com_search  `/ ckeys.set
		'com_replace `\ ckeys.set
		'search      `n ckeys.set
		'replace     `r ckeys.set
		'command     `: ckeys.set
		'undo        `u ckeys.set
		'mark        `m ckeys.set
		'delete      `d ckeys.set
		'yank        `y ckeys.set
		'paste       `p ckeys.set
		'key_o       `o ckeys.set

	end

	begin
		display
		key my! my while

		mode
		if
			my \e =
			if	ekeys accept:escape
				0= if cmode end
				next
			end

			my printable?
			if
				my insert complete right
				next
			end

			my keys.run
			next
		end

		my \e =
		if	cekeys accept:escape drop
			next
		end

		my ckeys.run
	end ;

\ ***** Short Commands *****

: q cr "\ec" type bye ;
: w save ;
: wq w q ;
: ? key drop ;

: g ( line -- )
	0 to caret for down end ;

\ ***** Go! *****

"HOME" getenv "%s/.redrc" format
slurp dup push
if top evaluate drop end
pop free

"" load
1 arg if 1 arg open end
cycle
