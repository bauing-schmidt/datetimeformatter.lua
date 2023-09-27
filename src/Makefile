
linux:
	gcc -O2 -std=gnu11 -fPIC -Wall -shared -o libdatetimeformatter.so libdatetimeformatter.c -llua -lpthread

macos:
	cc -O2 -std=c11 -fPIC -Wall -dynamiclib -o libdatetimeformatter.dylib libdatetimeformatter.c -llua -lpthread

install:
	mkdir -p /usr/local/lib/lua/5.4/	# just for ensuring that the dest dir exists
	mkdir -p /usr/local/share/lua/5.4/	# just for ensuring that the dest dir exists
	mv libdatetimeformatter.so /usr/local/lib/lua/5.4/
	cp datetimeformatter.lua /usr/local/share/lua/5.4/

install-macos:
	mkdir -p /usr/local/lib/lua/5.4/	# just for ensuring that the dest dir exists
	mkdir -p /usr/local/share/lua/5.4/	# just for ensuring that the dest dir exists
	mv libdatetimeformatter.dylib /usr/local/lib/lua/5.4/
	cp datetimeformatter.lua /usr/local/share/lua/5.4/

test:
	cd ../test && lua test-libc.lua -v #> README
