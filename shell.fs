\ Reforth Shell

"Reforth Language, MIT/X11 License\n" type
"Copyright (c) 2012 Sean Pringle <sean.pringle@gmail.com>\n" type
"Reforth comes with no warranty; For details type 'license'\n" type

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
	. "IN NO EVENT SHALL THE AUTHOrs OR COPYRIGHT HOLDErs BE LIABLE FOR ANY"
	. "CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,"
	. "TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE"
	. "SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE." . ;

: shell ( -- )

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

	: ok ( -- )
		" ok " type .s cr ;

	'ok    sys:on-ok    !
	'what  sys:on-what  !
	'error sys:on-error !

	100 allocate at!

	begin
		0 at c! "> " type
		at 100 accept cr
		if
			at evaluate drop
		else
			ok
		end
	end
;

shell