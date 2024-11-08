#define _GNU_SOURCE
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <linux/input-event-codes.h>

#include "xdg-shell-client-protocol.h"
#include "viewporter-client-protocol.h"

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_surface *surface;
static struct wl_shm *shm;
static struct wl_buffer *buffer;
static struct xdg_wm_base *xdg_wm_base;
static struct xdg_surface *xdg_surface;
static struct xdg_toplevel *xdg_toplevel;
static struct wp_viewporter *viewporter;
static struct wp_viewport *viewport;
static struct wl_seat *seat;
struct wl_pointer *pointer;

static volatile uint32_t *frame_buffer;
static int width = 100;
static int height = 100;
static bool running = true;
static bool configured = false;

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                 int32_t w, int32_t h, struct wl_array *states)
{
    if (w > 0 && h > 0 && viewport) {
        wp_viewport_set_destination(viewport, w, h);
    }
}

static void pointer_enter(void *data, struct wl_pointer *pointer,
                         uint32_t serial, struct wl_surface *surface,
                         wl_fixed_t sx, wl_fixed_t sy) {
    // Cursor enters the surface
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
                         uint32_t serial, struct wl_surface *surface) {
    // Cursor leaves the surface
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                         uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    // Mouse movement
}

static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                         uint32_t time, uint32_t button, uint32_t state) {
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
        xdg_toplevel_move(xdg_toplevel, seat, serial);
    }
    else if (button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
        xdg_toplevel_resize(xdg_toplevel, seat, serial, XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT);
    }
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
                        uint32_t time, uint32_t axis, wl_fixed_t value) {
    // Scroll wheel
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
};

static void seat_capabilities(void *data, struct wl_seat *wl_seat,
                            uint32_t capabilities) {
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        pointer = wl_seat_get_pointer(wl_seat);
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);
    }
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
};


static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    running = false;
}
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}
static const struct xdg_wm_base_listener shell_listener = {
    .ping = xdg_wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    xdg_surface_ack_configure(xdg_surface, serial);
    if (!configured) {
        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, width, height);
        wl_surface_commit(surface);
        configured = true;
    }
}
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface, uint32_t version)
{
    if (strcmp(interface, wl_seat_interface.name) == 0) {
        seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
        viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(xdg_wm_base, &shell_listener, NULL);
    }
}
static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
};

static inline void draw_to_buffer(uint32_t color) {
    // draw to the buffer, a simple loop to simulate some drawing logic, needs not be optimal
    for (int i = 0; i < width * height; i++) {
        frame_buffer[i] = color;
    }
}

int main() {
    display = wl_display_connect(NULL);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display); // second round-trip

    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, NULL);
    viewport = wp_viewporter_get_viewport(viewporter, surface);

    xdg_toplevel_set_app_id(xdg_toplevel, "MAIN2.C");
    xdg_toplevel_set_title(xdg_toplevel, "MAIN2.C");

    // use shared buffer for direct writes to wayland frame buffer
    const int stride = width * 4; // 4 bytes, RGBA
    const int size = stride * height;
    const int fd = memfd_create("buffer", 0); // memory file descriptor
    ftruncate(fd, size); // allocate memory for memory file
    frame_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // shared memory region
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size); // wayland buffer that reference the shared memory
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                     WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    draw_to_buffer(0xFFFF0000);
    wl_surface_commit(surface);

    // Wait for the first configure event
    wl_display_roundtrip(display);

    while (running && wl_display_dispatch(display) != -1) {
        // Just handle events
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        draw_to_buffer(0xFF000000 | (time(NULL) * 1000));
        // need to tell Wayland about the update
        // (happens without too it seems, but implicitly by compositor reacting to event too, unreliable/latency?)
        wl_surface_damage(surface, 0, 0, width, height);
        wl_surface_commit(surface);
        wl_display_flush(display);  // ensure changes are sent to compositor immediately
        clock_gettime(CLOCK_MONOTONIC, &end);
        long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000 +
                         (end.tv_nsec - start.tv_nsec);
        printf("Elapsed time: %ld\n", elapsed_ns);
    }
    return 0;
}
