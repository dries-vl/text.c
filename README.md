wayland: tcc -g -O0 main2.c xdg-shell-client-protocol.c -Iinclude -lwayland-client

windows example: cl win_d3d.c -ld3d11 -lgdi32 -mwindows
(can omit d3d if not using it)
