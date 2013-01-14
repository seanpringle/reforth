\ Reforth Editor

: nop ;

: place+ ( s d -- d' )
	tuck place dup count + ;

: copy ( s -- a )
	dup count 1+ allocate tuck place ;

: extract ( s n - a )
	my! my 1+ allocate at! at my cmove 0 at my + c! at ;

: rtrim ( s -- s )
	dup if dup "\s*$" split drop drop end ;

: color ( r g b -- )
	create swap rot , , , does
		at! @+ @+ @+ ;

 9 value \t
10 value \n
13 value \r
27 value \e

\ Zenburn
D8h D8h D8h color fg-normal
55h DDh 55h color fg-active
CCh 99h 99h color fg-string
88h CCh CCh color fg-number
FFh DDh AAh color fg-keyword
88h AAh 88h color fg-comment
BBh BBh CCh color fg-coreword

33h 33h 33h color bg-normal
39h 39h 39h color bg-active
11h 11h 11h color bg-lineno

create last_fg 20 allot
create last_bg 20 allot
create file   100 allot
create tib   1000 allot
create escseq 100 allot

: fg ( r g b -- )
	swap rot "\e[38;2;%d;%d;%dm" format
	dup last_fg compare if dup type end last_fg place ;

: bg ( r g b -- )
	swap rot "\e[48;2;%d;%d;%dm" format
	dup last_bg compare if dup type end last_bg place ;

0 value edit
0 value line
0 value word
0 value rows
0 value cols

0 array lines
0 array words
0 array spacing

34 value DQUOTE
92 value BSLASH

0 value yank_word
0 value yank_line

: erase ( -- )
	"\e[K" type ;

: page ( -- )
	"\e[2J" type 0 0 at-xy ;

: fgbg ( -- )
	last_fg type last_bg type ;

: plain ( -- )
	"\e[0m" type fgbg ;

: bold ( -- )
	"\e[1m" type fgbg ;

: underline ( -- )
	"\e[4m" type fgbg ;

: cursor ( state -- )
	if "\e[?25h" else "\e[?25l" end type ;

: wrap ( state -- )
	if "\e[?7h" else "\e[?7l" end type ;

: listen ( -- flag )
	0 0 at-xy shell:listen shell:tib tib place shell:tib c@ 0<> ;

: sane_range ( n m -- n' m' )
	: bounds ( n -- n' )
		lines.size @ 1- min 0 max ;
	push bounds pop bounds over max ;

: import ( text line -- )
	at! begin
		dup "\n" split my! 1 lines.grow
		swap rtrim copy i at + lines.ins
		my while
	end
	drop ;

: export ( start stop -- text )
	sane_range over - 1+ my!
	0 my
	for
		over i + lines.get count + 1+
	end
	1+ allocate tuck at! my
	for
		dup i + lines.get
		at place+ at! 10 c!+
	end
	drop 0 over at 1- max c! ;

: escape ( jump -- flag )
	0 50
	for
		key? or dup until
		1000 usec
	end
	0= if drop false exit end

	escseq at!
	key my! my c!+
	my 91 =
	if
		begin
			key dup c!+ dup while
			dup 63 > over 127 < and until
			drop
		end my!
	end
	0 c!+ my
	swap array:get execute true ;

: skip_spaces ( a -- a' )
	begin
		dup c@ my! my while
		my space? while
		1+
	end ;

: skip_string ( a -- a' )
	1+ begin
		dup c@ my! my while
		1+

		my DQUOTE = until
		my BSLASH =
		over c@ 0<> and
		if 1+ end
	end ;

: skip_normal ( a -- a' )
	begin
		dup c@ my! my while
		my space? until
		1+
	end ;

: skip_word ( a -- a' )
	skip_spaces
	dup c@ my! my
	if
		my DQUOTE =
		if	skip_string
			exit
		end

		skip_normal
	end ;

: lineno_width ( -- n )
	1 lines.size @
	begin
		10 / dup while push 1+ pop
	end
	drop ;

: display ( -- )

	record local
		0 value column
		0 value this
		0 value item
		create parsed 100 allot
		create lineno 10 allot
	end

	: item+ ( -- )
		item 1+ 'item vary ;

	: column+ ( -- )
		column 1+ 'column vary ;

	: whitespace ( a -- a' )
		plain
		begin
			column cols < while
			dup c@ my!
			my while
			my space? while
			my 9 =
			if
				5 column lineno_width - 5 mod -
				for 32 emit column+ end 1+
			else
				my emit
				column+ 1+
			end
		end ;

	: stringlit ( a -- a' )
		fg-string fg
		begin
			column cols < while
			dup c@ my!
			my while
			my emit
			column+ 1+

			column cols < while
			i 0> my DQUOTE = and until

			my BSLASH =
			if	dup c@ DQUOTE =
				if	dup c@ emit
					column+ 1+
				end
			end
		end ;

	: parse_word ( a -- a' )
		parsed at!
		begin
			dup c@ my! my while
			my space? until
			my c!+ 1+
		end
		0 c!+ ;

	: print_word ( r g b -- )
		fg parsed at!
		begin
			column cols < while
			c@+ my! my while
			my space? until
			my emit
			column+
		end ;

	: item_check ( -- )
		item word = this edit = and
		if underline else plain end ;

	: active? ( -- )
		edit this = ;

	: print_line ( a n -- )

		0 'item vary
		lineno_width 'column vary
		  'this vary

		plain

		active? if fg-active else fg-normal end fg
		bg-lineno bg this lineno print

		fg-normal fg
		active? if bg-active else bg-normal end bg

		begin dup while

			column cols < while
			dup c@ my! my while

			my space?
			if	whitespace
				next
			end

			item_check

			my DQUOTE =
			if	stringlit
				item+
				next
			end

			parse_word

			parsed "\\" compare 0=
			if	fg-comment print_word item+
				begin
					whitespace item_check parse_word
					fg-comment print_word item+
					parsed c@ while
				end
				leave
			end

			parsed "(" compare 0=
			if	fg-comment print_word item+
				begin
					whitespace item_check parse_word
					fg-comment print_word item+
					parsed ")" compare while
					parsed c@ while
				end
				next
			end

			parsed "^(if|else|end|begin|for|:|;|value|string|array|create|does|record|while|until|leave|next)$" match nip
			if	fg-keyword print_word item+
				next
			end

			parsed 'nop sys:xt-head sys:find
			if	fg-coreword print_word item+
				next
			end

			parsed c@ digit?
			if	fg-number print_word item+
				next
			end

			fg-normal print_word item+
		end
		drop erase ;

	lineno_width
	"%%%dd " format lineno place

	0 0 at-xy
	max-xy 'rows vary 1+ 'cols vary
	false cursor

	fg-normal fg bg-lineno bg
	false cursor

	rows
	for
		cr
		line i + lines.get my! my
		if
			my line i + print_line
		else
			bg-normal bg erase
		end
	end

	plain fg-normal fg bg-lineno bg
	0 0 at-xy .s erase ;

: yank_line! ( text -- )
	yank_line free 'yank_line vary ;

: yank_word! ( text -- )
	yank_word free 'yank_word vary ;

: word_jump ( words -- )
	my! words.size @ if my word + words.size @ 1- min 0 max to word end ;

: words_array ( -- )

	: empty ( a -- )
		dup my! array:size @ for i my array:get free end
		my array:size @ neg my array:grow ;

	words empty spacing empty

	edit lines.get
	begin
		dup while dup c@ while 1 words.grow 1 spacing.grow
		dup skip_spaces tuck over - extract i spacing.set
		dup skip_word   tuck over - extract i words.set
	end
	drop 0 word_jump ;

: words_flush ( -- )
	0 words.size @
	for
		i spacing.get count +
		i   words.get count +
	end
	1+ allocate dup
	words.size @
	for
		i spacing.get swap place+
		i   words.get swap place+
	end
	drop rtrim
	edit lines.get free
	edit lines.set ;

: word_left ( -- )
	word 0>
	if
		word words.get
		word 1- words.get swap
		word 1- words.set
		word words.set
		-1 word_jump
	end
	words_flush ;

: word_right ( -- )
	word words.size @ <
	if
		word words.get
		word 1+ words.get swap
		word 1+ words.set
		word words.set
		1 word_jump
	end
	words_flush ;

: word_yank ( -- )
	words.size @ 0>
	if
		word words.get copy yank_word!
	end ;

: word_delete ( -- )
	words.size @ 0>
	if
		word_yank
		word words.del free
		word 1+ spacing.del free
		-1 words.grow
		-1 spacing.grow
		0 word_jump
	end
	words_flush ;

: word_paste ( -- )
	yank_word
	if
		\ no leading space for first word on empty line
		words.size @ if " " else "" end
		1 words.grow 1 spacing.grow 1 word_jump
		yank_word copy word words.ins
		copy word spacing.ins
	end
	words_flush ;

: word_indent ( -- )
	words.size @
	if
		word spacing.get dup here place free
		here dup count + at! 32 c!+ 0 c!+
		here copy word spacing.set
	end
	words_flush ;

: word_dedent ( -- )
	words.size @
	if
		word spacing.get at! at c@
		if
			0 at count at + 1- c!
		end
	end
	words_flush ;

: line_jump ( lines -- )
	rows 2/ 2/ my!
	edit + lines.size @ 1- min 0 max 'edit vary
	edit my - line min edit rows my - - 1+ max 0 max 'line vary
	words_array ;

: line_yank ( -- )
	edit lines.get copy yank_line! ;

: line_delete ( -- )
	edit lines.del yank_line! -1 lines.grow words_array ;

: lines_delete ( start stop -- )
	sane_range over - 1+ for dup lines.del free -1 lines.grow end drop words_array ;

: line_paste ( -- )
	yank_line if 1 line_jump yank_line edit import words_array end ;

: lines_paste ( start -- )
	my! yank_line if yank_line my import words_array end ;

: line_first ( -- )
	edit neg line_jump ;

: line_last ( -- )
	lines.size @ 1- edit - line_jump ;

: line_insert ( line -- )
	1 lines.grow 1 line_jump "" edit import words_array ;

: line_indent ( -- )
	words.size @
	if
		0 spacing.get dup here place free
		here dup count + at! 9 c!+ 0 c!+
		here copy 0 spacing.set
	end
	words_flush ;

: line_split ( -- )
	word my! line_yank line_paste
	word neg word_jump my 1+ for word_delete end
	-1 line_jump my 1+ word_jump begin word my > while word_delete end ;

: line_join ( -- )
	word my!
	begin
		1 line_jump word neg word_jump words.size @ while
		word_delete -1 line_jump words.size @ word_jump word_paste
	end
	line_delete -1 line_jump
	word neg word_jump my word_jump ;

: line_edit ( -- )

	record local
		variable caret
		1024 array input
		 256 array keys
		 256 array ekeys
	end

	: length ( -- )
		0 1024 for i input.get while 1+ end ;

	: caret_move ( n -- )
		caret @ + 0 max length min 1023 min caret ! ;

	: addc ( c -- )
		caret @ input.ins 1 caret_move ;

	: delc ( -- )
		caret @ input.del drop ;

	: bakc ( -- )
		caret @ if -1 caret_move delc end ;

	: calc_col ( n -- x )
		0 swap
		for
			i input.get my! my while
			my 9 =
			if
				dup 5 mod 5 swap - +
				next
			end
			1+
		end ;

	: update ( -- )
		here at! 1024 for i input.get c!+ end

		edit lines.get free
		here rtrim copy edit lines.set
		words_array display

		caret @ calc_col lineno_width 1+ +
		edit line - 1+ at-xy true cursor ;

	: insert ( a -- )
		at! at if begin c@+ my! my while my addc end end ;

	: setup ( -- )
		0 caret ! 1024 for 0 i input.set end

		1024
		for
			i spacing.get insert
			i   words.get insert
		end

		0 word 1+
		for
			i spacing.get count +
			i   words.get count +
		end
		0 caret ! caret_move

		\ auto-indent
		words.size @ 0= edit 0> and
		if
			\ copy leading white-space from previous or next line
			edit 1- lines.get at! at c@ space? 0= if edit 1+ lines.get at! end
			begin at while c@+ my! my while my space? while my addc end
		end ;

	: k_up ( -- )
		-1 line_jump setup ;

	: k_down ( -- )
		1 line_jump setup ;

	: k_left ( -- )
		-1 caret_move ;

	: k_right ( -- )
		1 caret_move ;

	: k_home ( -- )
		0 caret ! ;

	: k_end ( -- )
		1023 caret_move ;

	: esc_tilde ( -- )
		escseq 1+ c@ my!

		my 51 = \ del
		if
			delc
			exit
		end ;

	'k_up       65 ekeys.set
	'k_down     66 ekeys.set
	'k_right    67 ekeys.set
	'k_left     68 ekeys.set
	'k_home     72 ekeys.set
	'k_end      70 ekeys.set
	'esc_tilde 126 ekeys.set

	'bakc  127 keys.set

	setup
	begin
		update key my!

		my \e =
		if	ekeys escape while
			next
		end

		my 31 > my 127 < and my \t = or
		if	my addc \ character
		else my keys.get execute
		end
	end ;

: word_next ( -- )

	: line_adv ( n -- )
		line_jump 0 to word ;

	: remainder ( -- )
		word words.size @ 1- = if 1 line_adv else 1 word_jump end ;

	: beginning ( -- )
		edit neg line_adv ;

	: word? ( f -- f' )
		drop word words.get yank_word compare 0= ;

	: line? ( f -- f' )
		words.size @ for word? dup until 1 word_jump end ;

	: search ( -- f )
		false lines.size @ line - for line? dup until 1 line_adv end ;

	: save ( -- l e w )
		line edit word ;

	: clean ( l e w -- )
		drop drop drop ;

	: restore ( l e w -- )
		to word to edit to line ;

	save

	remainder search if clean exit end
	beginning search if clean exit end

	restore ;

: word_find ( -- )
	listen if tib copy yank_word! word_next end ;

: close ( -- )
	lines.size @ dup for 0 lines.del free end neg lines.grow ;

: open ( name -- )
	close dup file place slurp dup edit import free words_array ;

: write ( name -- )
	my! 0
	lines.size @ for i lines.get count + 1+ end 1+ allocate dup
	lines.size @ for i lines.get swap place+ 10 over c! 1+ end
	drop rtrim dup my blurt drop free ;

: command ( -- )
	listen if tib evaluate drop end display ;

10 array undo

: snapshot ( -- )
	9 undo.get free 0 lines.size @ export 0 undo.ins ;

: rollback ( -- )
	0 undo.del dup if close dup 0 import words_array end free ;

: file_edit ( -- )

	record local
		256 array keys
		256 array ekeys
	end

	: pgup ( -- )
		rows 2/ line_jump ;

	: pgdn ( -- )
		rows 2/ neg line_jump ;

	: k_up ( -- )
		-1 line_jump ;

	: k_down ( -- )
		1 line_jump ;

	: k_right ( -- )
		1 word_jump ;

	: k_left ( -- )
		-1 word_jump ;

	: k_home ( -- )
		word neg word_jump ;

	: k_end ( -- )
		words.size @ word_jump ;

	: k_move ( -- )
		escseq 1+ c@ my!

		my 53 = \ 5
		if
			pgdn
			exit
		end

		my 54 = \ 6
		if
			pgup
			exit
		end

		my 50 = \ ins
		if
			line_edit
			exit
		end

		my 51 = \ del
		if
			word_delete
			exit
		end ;

	: k_edit ( -- )
		snapshot line_edit ;

	: k_redit ( -- )
		snapshot word_delete k_left line_edit ;

	: kl_edit ( -- )
		snapshot line_insert line_edit ;

	: k_delete ( -- )
		snapshot word_delete ;

	: k_paste ( -- )
		snapshot word_paste ;

	: k_wleft ( -- )
		snapshot word_left ;

	: k_wright ( -- )
		snapshot word_right ;

	: k_indent ( -- )
		snapshot word_indent ;

	: k_dedent ( -- )
		snapshot word_dedent ;

	: kl_indent ( -- )
		snapshot line_indent ;

	: kl_insert ( -- )
		snapshot line_insert ;

	: kl_delete ( -- )
		snapshot line_delete ;

	: kl_paste ( -- )
		snapshot line_paste ;

	: kl_split ( -- )
		snapshot line_split ;

	: kl_join ( -- )
		snapshot line_join ;

	'k_up    65 ekeys.set
	'k_down  66 ekeys.set
	'k_right 67 ekeys.set
	'k_left  68 ekeys.set
	'k_home  72 ekeys.set
	'k_end   70 ekeys.set
	'k_move 126 ekeys.set

	'command       `: keys.set
	'rollback      `u keys.set
	'k_edit        `i keys.set
	'k_redit       `r keys.set
	'word_yank     `c keys.set
	'k_delete      `x keys.set
	'k_paste       `v keys.set
	'k_wleft       `- keys.set
	'k_wright      `+ keys.set
	'k_indent      32 keys.set
	'k_dedent     127 keys.set
	'word_find     `f keys.set
	'word_next     `n keys.set
	'kl_indent     \t keys.set
	'kl_insert     \n keys.set
	'kl_edit       `o keys.set
	'line_yank     `y keys.set
	'kl_delete     `d keys.set
	'kl_paste      `p keys.set
	'kl_split      `s keys.set
	'kl_join       `j keys.set

	begin
		display key my!

		my \e =
		if	ekeys escape drop
			next
		end

		my keys.get execute
	end ;

: quit ( -- )
	true cursor plain bye ;

: w file write ;
: q plain page quit ;
: wq w q ;
: g edit - line_jump ;
: y export yank_line! ;
: d over over y snapshot lines_delete ;
: p snapshot lines_paste ;

1 arg if 1 arg open end
page false wrap file_edit
