\ Reforth CGI experiment

1   dup value HTTP_GET
1 + dup value HTTP_HEAD
1 + dup value HTTP_PUT
1 + dup value HTTP_POST
1 +     value HTTP_DELETE

0 value http_method
0 value request_uri
0 value user_agent

1000 value BUFFER

\ format and write to stderr
: log ( ... pattern -- )
	time "%c " date error format error "\n" error ;

: match? ( a p -- f )
	match nip ;

: within? ( a b n -- f )
	my! - my 1- < ;

\ read a line from stdin
: read_line ( -- a )

	static locals
		BUFFER buffer line
	end

	0 line BUFFER 0 cfill

	BUFFER 1-
	for key my!
		my \r = if key my! end
		my \n = until
		my while
		my i line c!
	end

	begin my while my \n <> while
		key my!
	end

	0 line ;

\ read and process client headers
: read_headers ( -- )

	begin
		read_line dup at! c@ while
		at "request: %s" log

		at "^GET [^ ]+ HTTP/1" match?
		if	HTTP_GET to http_method
			at 4 + dup " " split drop drop
			here to request_uri place, next
		end

		at "^User-Agent: .+" match?
		if	at 12 +
			here to user_agent place, next
		end
	end ;

\ parser hook for HTML tag literals
: pre_eval ( -- f )

	static locals
		BUFFER buffer item
	end

	0 item BUFFER 0 cfill

	sys:source @ dup at!
	"^<[/!]?[a-zA-Z]+[^>]*>" match? dup
	if	BUFFER 1-
		for	c@+ my!
			my while
			my i item c!
			my `> = until
		end
		at sys:source !
		0 item sys:mode @
		if	string, 'print word,
		else print
		end
	end ;

'pre_eval sys:on-eval !

read_headers

http_method HTTP_GET <>
request_uri "/" match? 0= or
if	request_uri "404 %s" log
	"HTTP/1.1 404 Not Found\n" type
	"Connection: close\n" type
	\n emit bye
end

request_uri "200 %s" log
"HTTP/1.1 200 OK\n" type
"Connection: close\n" type
"Content-type: text/html\n" type
\n emit

<!DOCTYPE html>
<html>
<head>
	<title> "reforth" type </title>
</head>
<body>


	<pre>
	.s
	</pre>

</body>
</html>
