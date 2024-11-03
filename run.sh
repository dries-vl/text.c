tcc -g -O0 main2.c xdg-shell-client-protocol.c -I. -lwayland-client -lGLESv2 -lEGL -lwayland-egl -o a.out
./a.out > /dev/null 2>&1
