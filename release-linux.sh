mkdir -p bin/release

gcc -o bin/release/zztexview-linux `./common.sh` `wowlib/deps/wow_gui_x11.sh`

