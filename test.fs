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

50 cell array data

: cmp < ;

1000000
for
	50 for 50 i - i data ! end
	'cmp 0 data 50 sort
end
