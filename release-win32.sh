mkdir -p bin/release
mkdir -p bin/o/win32

PATH=$PATH:~/c/mxe/usr/bin

# icon
i686-w64-mingw32.static-windres src/icon.rc -o bin/o/win32/icon.o

i686-w64-mingw32.static-gcc -o bin/release/zztexview-win32.exe `./common.sh` \
	-municode \
	`wowlib/deps/wow_gui_win32.sh` \
	bin/o/win32/icon.o

