: sort ( xt a n -- )

	rot value cmp

	: 2dup  over over ;
	: 2drop drop drop ;

	: mid ( l r -- mid )
		over - 2/ -1 cells and + ;

	: exch ( a1 a2 -- )
		at! at @ my! dup @ !+ at! my !+ ;

	: part ( l r -- l r r2 l2 )
		2dup mid @ my! 2dup
		begin
			swap at! begin @+ my cmp execute while end at cell-
			swap at! begin my @- cmp execute while end at cell+
			2dup <= if 2dup exch at! cell+ at cell- end
			2dup > until
		end ;

	: qsort ( l r -- )
		part swap rot
		2dup < if qsort else 2drop end
		2dup < if qsort else 2drop end ;

	dup 1 > if 1- cells over + qsort else 2drop end ;

50 cell array data

: cmp < ;

1000000
for
	50 for 50 i - i data ! end
	'cmp 0 data 50 sort
end
