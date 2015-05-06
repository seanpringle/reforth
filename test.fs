50 array data

: cmp < ;

: main ( -- )
	1000000 for
		50 for
			50 i - i data !
		end
		'cmp 0 data 50 sort
	end ;

main
