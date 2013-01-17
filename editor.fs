\ Reforth Editor

 0 value mode
 0 value caret
 0 value caret-row
 0 value caret-col
 0 value file
 0 value length
 0 value copied
-1 value marked

 10 value \n
 13 value \r
 32 value \s
  9 value \t
 27 value \e
127 value \b

5 value tabsize

10 array backups

: copy ( a -- b )
	dup count 1+ allocate tuck place ;

: extract ( a n -- b )
	my! at! my 1+ allocate at over my cmove 0 over my + c! ;

: color ( r g b -- )
	create swap rot , , , does at! @+ @+ @+ ;

: match? ( subjetc pattern -- f )
	match nip ;

: msec ( n -- )
	1000 * usec ;

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

: status ( -- )
	max-xy my! drop 0 my at-xy fg-status fg bg-status bg ;

: white? ( c -- f )
	dup \s = swap \t = or ;

: line ( index -- )

	: scan ( a -- a' )
		at! begin c@+ my! my while my \n = until end at 1- ;

	file swap for scan end ;

: word ( a index -- a' )

	: skip ( a -- a' )
		at! begin c@+ my! my while my white? while end at 1- ;

	: sparse ( a -- a' )
		at! begin c@+ my! my while my `" = until my `\ = if at c@ if c@+ drop end end end at ;

	: parse ( a -- a' )
		at! begin c@+ my! my while my space? until end at 1- ;

	: scan ( a -- a' )
		dup c@ `" = if sparse else parse end ;

	for skip scan end skip ;

: load ( text -- )
	file free copy dup to file count to length ;

create name 100 allot

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

: pointer ( -- a )
	file caret + ;

: current ( -- c )
	pointer c@ ;

: insert ( c -- )
	1 length+ pointer dup dup 1+ place c! ;

: remove ( -- c )
	pointer c@ caret length < if pointer 1+ pointer place 1 length- end ;

: replace ( c -- )
	pointer c! ;

: home ( -- )
	begin caret 0> while left current \n = until end caret 0> if right end ;

: ending ( -- )
	begin caret length < while current \n = until right end ;

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

: mark ( -- )
	caret home caret to marked to caret ;

: remark ( -- )
	marked 0< if mark end ;

: unmark ( -- )
	-1 to marked ;

: paste ( -- )
	backup
	caret ending right copied count my! my length+
	pointer dup dup my + place copied swap my cmove
	to caret ;

: yank ( -- )
	remark caret marked file + at! ending right
	at caret marked - extract copied free to copied
	to caret unmark ;

: delete ( -- )
	backup remark
	marked file + at! ending right caret marked - my!
	at my extract copied free to copied pointer at place my length-
	marked to caret unmark ;

create target 100 allot
create source 100 allot

: search ( -- )

	: goto ( a -- )
		file - to caret ;

	right

	pointer target match if goto exit end drop
	file    target match if goto exit end drop

	left ;

: replace ( -- )
	pointer target match swap pointer = and
	if
		backup pointer copy at! at target split drop
		at - for remove drop end at free source at!
		begin c@+ dup while insert right end drop
	end ;

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
	caret
	up home dup caret > current white? and
	if
		my! 0
		 begin
			current white? while
			caret current my to caret insert to caret
			right 1+
		end
		my +
	end
	to caret ;

create input 100 allot

: prompt ( s -- f )
	status erase type 0 input ! input 100 accept ;

: command ( -- )
	"> " prompt
	if
		true wrap \s emit
		input "^/[^ ]+" match?
		if
			`" input c! input evaluate drop target place
			search exit
		end
		input "^![^ ]+" match?
		if
			`" input c! input evaluate drop source place
			exit
		end
		input evaluate drop
	end ;

: display ( -- )

	static vars

		0 value row
		0 value col
		0 value rows
		0 value cols

		0 value from
		0 value upto
		0 value last
		0 value active
		0 value counter
		0 value discard

		create pad 100 allot

	end

	: col+ ( -- )
		col 1+ cols min to col ;

	: row+ ( -- )
		row 1+ rows min to row 0 to col ;

	: line+ ( -- )
		erase 10 emit row+ ;

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
		my \n =   if line+    exit end
		last-col? if          exit end
		my \t =   if tab+     exit end
		my emit col+ ;

	: put+ ( a c -- a' )
		dup c@ if dup c@ put 1+ end ;

	: puts ( s -- )
		at! begin c@+ dup while put end drop ;

	: skip ( a -- a' )
		at! begin c@+ dup while dup white? while put end drop at 1- ;

	: scan ( a -- a' )
		at! begin c@+ dup while dup space? until put end drop at 1- ;

	: sparse ( a -- a' )
		fg-string fg put+ at! begin c@+ dup while dup put dup `" = until `\ = if at put+ at! end end drop at ;

	: keep ( c -- )
		pad count pad + at! c!+ 0 c!+ ;

	: comment ( -- )
		fg-comment fg pad puts ;

	: comment1 ( a -- a' )
		comment begin dup c@ while dup c@ \n = until skip dup scan swap "^)\s+" match? until end ;

	: comment2 ( a -- a' )
		comment at! begin c@+ dup while dup \n = until put end drop at 1- ;

	: color ( -- )

		: go ( r g b -- )
			fg pad puts ;

		: is ( c -- f )
			pad c@ = pad 1+ c@ 0= and ;

		`( is if comment1 exit end
		`\ is if comment2 exit end

		pad sys:macros sys:find if fg-keyword go exit end
		pad 'caret sys:xt-head sys:find if fg-coreword go exit end

		pad number nip if fg-number go exit end

		fg-normal go ;

	: parse ( a -- a' )
		0 pad ! at! begin c@+ dup while dup space? until keep end drop at 1- color ;

	: word ( a -- a' )
		skip dup c@ `" = if sparse else parse end ;

	: bcolor ( -- )
		counter active = if bg-active bg exit end
		counter marked = if bg-marked bg exit end
		bg-normal bg ;

	: width ( a -- n )
		at! 0 begin dup counter + caret = if leave end c@+ my! my 0= my \n = or if drop 0 leave end 1+ end ;

	: line ( a -- a' )
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
			leave
		end
		-1    to caret-col
		-1    to caret-row
		caret to last
		from  to counter

		file from + ;

	: draw ( a -- a' )
		begin line row rows < while dup c@ while end ;

	: cleanup ( a -- )
		dup file - to upto bcolor \n put skip drop bg-normal bg
		rows row - 0 max for erase cr end ;

	: status-line ( -- )
		status .s
		mode if "-- INSERT --" else "-- COMMAND --" end type
		target c@ if target " /%s" print end
		source c@ if source " \%s" print end
		erase ;

	: place-caret
		true cursor caret-col caret-row at-xy ;

	setup draw cleanup status-line place-caret ;

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
	: com_search  ( -- ) "/"  prompt if input target place search end ;
	: com_replace ( -- ) "\\" prompt if input source place end ;

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

 		'ekey_up     65 ekeys.set
 		'ekey_down   66 ekeys.set
 		'ekey_right  67 ekeys.set
 		'ekey_left   68 ekeys.set
 		'ekey_home   72 ekeys.set
 		'ekey_end    70 ekeys.set
 		'ekey_tilde 126 ekeys.set

		'key_back    \b keys.set
		'key_enter   \n keys.set

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

			my 31 > my 127 < and my 9 = or
			if
				my insert right
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

: q cr bye ;
: w save ;
: wq w q ;
: ? key drop ;

"" load
2 arg if 2 arg open end
cycle
