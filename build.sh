
# -O2
clang_args=" -Wno-parentheses -Wshadow -Wno-null-dereference -Wno-format-security -Wno-pragma-pack -fno-caret-diagnostics -fdiagnostics-absolute-paths  -fno-exceptions"
clang_linker="" #clang_linker="-lstdc++ -lubsan"
game_defs="-DDEBUG"

printf "Building game..."
clang $clang_args  src/main.cpp -g $game_defs  -Lthirdparty/ -Ithirdparty/ -lm -lX11 -ldl -lglfw -lraylib $clang_linker -o bin/ld48
printf "done.\n"

