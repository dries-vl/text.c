#define _GNU_SOURCE  // Needed for some functions like memfd_create
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "xdg-shell-client-protocol.h"

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_surface *surface;
static struct wl_shm *shm;
static struct wl_buffer *buffer;
static struct xdg_wm_base *xdg_wm_base;
static struct xdg_surface *xdg_surface;
static struct xdg_toplevel *xdg_toplevel;
static bool configured;

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    if (!configured) {
        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, 1, 1);
        configured = true;
    }
    wl_surface_commit(surface);
}
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}
static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    }
}

int main() {
    if (!(display = wl_display_connect(NULL)))
        return 1;

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &(struct wl_registry_listener){
        .global = registry_global}, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !xdg_wm_base)
        return 1;

    xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);

    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    int fd = memfd_create("buffer", 0);
    ftruncate(fd, 40000);
    void *data = mmap(NULL, 40000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(data, 0xF0, 40000);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, 40000);
    buffer = wl_shm_pool_create_buffer(pool, 0, 100, 100, 400, WL_SHM_FORMAT_ARGB8888);
    wl_surface_commit(surface);

    while (wl_display_dispatch(display) != -1);
    return 0;
}
