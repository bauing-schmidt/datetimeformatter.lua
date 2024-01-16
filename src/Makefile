
linux:
	gcc -O2 -g -fPIC -Wall -shared -o libdatetimeformatterlua.so libdatetimeformatterlua.c -llua -ldatetimeformatter

macos:
	gcc -O2 -g -fPIC -Wall -dynamiclib -o libdatetimeformatterlua.dylib libdatetimeformatterlua.c -llua -ldatetimeformatter

mingw:
	gcc -O2 -g -fPIC -Wall -shared -o libdatetimeformatterlua.dll libdatetimeformatterlua.c -I/ucrt64/include -I/usr/local/include/ -L/usr/local/lib -llua54 -ldatetimeformatter

install:
	mkdir -p /usr/local/lib/lua/5.4/	# just for ensuring that the dest dir exists
	mkdir -p /usr/local/share/lua/5.4/	# just for ensuring that the dest dir exists
	mv libdatetimeformatterlua.so /usr/local/lib/lua/5.4/
	cp datetimeformatter.lua /usr/local/share/lua/5.4/

install-macos:
	mkdir -p /usr/local/lib/lua/5.4/	# just for ensuring that the dest dir exists
	mkdir -p /usr/local/share/lua/5.4/	# just for ensuring that the dest dir exists
	mv libdatetimeformatterlua.dylib /usr/local/lib/lua/5.4/
	cp datetimeformatter.lua /usr/local/share/lua/5.4/

install-mingw:
	mkdir -p /usr/local/lib/lua/5.4/	# just for ensuring that the dest dir exists
	mkdir -p /usr/local/share/lua/5.4/	# just for ensuring that the dest dir exists
	mv libdatetimeformatterlua.dll /usr/local/lib/lua/5.4/
	cp datetimeformatter.lua /usr/local/share/lua/5.4/

test:
	cd ../test && lua test.lua -v #> README
