
-include src/private/Makefile

.PHONY: js
js:
	emscons scons -j8 mode=release

.PHONY: js-debug
js-debug:
	emscons scons -j8 mode=debug

.PHONY: js-prof
js-prof:
	emscons scons -j8 mode=profile

.PHONY: js-es6
js-es6:
	emscons scons -j8 mode=release es6=1
  
.PHONY: js-es6-debug
js-es6-debug:
	emscons scons -j8 mode=debug es6=1

.PHONY: js-es6-prof
js-es6-prof:
	emscons scons -j8 mode=profile es6=1

# Make the doc using natualdocs.  On debian, we only have an old version
# of naturaldocs available, where it is not possible to exclude files by
# pattern.  I don't want to parse the C files (only the headers), so for
# the moment I use a tmp file to copy the sources and remove the C files.
# It's a bit ugly.
.PHONY: doc
doc:
	rm -rf /tmp/swe_src
	cp -rf src /tmp/swe_src
	./build/stellarium-web-engine --gen-doc > /tmp/swe_src/generated-doc.h
	find /tmp/swe_src -name '*.c' | xargs rm
	mkdir -p build/doc/ndconfig
	naturaldocs -nag -i /tmp/swe_src -o html doc -p build/doc/ndconfig

clean:
	scons -c
