: test ( -- )
	: a 1 dup drop drop ;
	: b a a a ;
	100000000 for i drop b b b end
;
'test sys:xt-body @ 100 dump
test