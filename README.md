*wayland*: tcc -g -O0 main2.c xdg-shell-client-protocol.c -Iinclude -lwayland-client

-commands to generate the viewporter and xdg-shell headers and source code:
wayland-scanner client-header /usr/share/wayland-protocols/stable/viewporter/viewporter.xml viewporter-client-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/viewporter/viewporter.xml viewporter-client-protocol.c
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.c

*windows* example: cl win_d3d.c -ld3d11 -lgdi32 -mwindows
(can omit d3d if not using it)
