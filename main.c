// File: simple_wayland.c
#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

// Global Wayland objects
struct wl_display *display = NULL;
struct wl_registry *registry = NULL;
struct wl_compositor *compositor = NULL;
struct wl_surface *surface = NULL;
struct xdg_wm_base *wm_base = NULL;
struct xdg_surface *xdg_surface = NULL;
struct xdg_toplevel *xdg_toplevel = NULL;
struct wl_shm *shm = NULL;
struct wl_buffer *buffer = NULL;
int running = 1;
int width = 640;
int height = 480;

// Function to create a shared memory file
int create_shm_file(size_t size) {
    char template[] = "/wayland-shm-XXXXXX";
    const char *path = getenv("XDG_RUNTIME_DIR");
    if (!path) {
        path = "/tmp";
    }
    char *name = malloc(strlen(path) + strlen(template) + 1);
    sprintf(name, "%s%s", path, template);
    int fd = mkstemp(name);
    unlink(name);
    free(name);
    if (fd < 0) {
        return -1;
    }
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Function to create and fill the buffer
void create_buffer() {
    int stride = width * 4; // 4 bytes per pixel (RGBA)
    int size = stride * height;
    int fd = create_shm_file(size);
    if (fd < 0) {
        fprintf(stderr, "Failed to create shared memory file\n");
        exit(1);
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "Failed to map shared memory\n");
        close(fd);
        exit(1);
    }

    // Fill the buffer with a color (e.g., red)
    uint32_t *pixel = data;
    for (int i = 0; i < width * height; ++i) {
        pixel[i] = 0xFFFF0000; // ARGB format (opaque red)
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    munmap(data, size);
    close(fd);
}

// Registry handler to bind global interfaces
static void registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(interface, "xdg_wm_base") == 0) {
        wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0) {
        shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    }
}

// Registry remover (not used)
static void registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    // No action needed
}

// Registry listener
static const struct wl_registry_listener registry_listener = {
    .global = registry_handler,
    .global_remove = registry_remover,
};

// Listener for xdg_wm_base to handle ping events
static void wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

// Listener for xdg_surface to handle configure events
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);

    // Attach the buffer and commit the surface
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// Listener for xdg_toplevel to handle toplevel events
static void toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states) {
    // Handle window resize or state changes here if needed
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    running = 0; // Exit the event loop
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
};

int main(int argc, char **argv) {
    // Connect to the Wayland display
    display = wl_display_connect(NULL);
    if (display == NULL) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return -1;
    }

    // Get the registry and add a listener
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    // Check if necessary interfaces are available
    if (compositor == NULL || wm_base == NULL || shm == NULL) {
        fprintf(stderr, "Required Wayland interfaces not found\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Add listener for wm_base
    xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);

    // Create a surface
    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create surface\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Create an xdg_surface
    xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
    if (xdg_surface == NULL) {
        fprintf(stderr, "Failed to get xdg_surface\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Add listener for xdg_surface
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

    // Create an xdg_toplevel
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    if (xdg_toplevel == NULL) {
        fprintf(stderr, "Failed to get xdg_toplevel\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Add listener for xdg_toplevel
    xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, NULL);

    // Set window title
    xdg_toplevel_set_title(xdg_toplevel, "Simple Wayland Window");

    // Create the buffer
    create_buffer();

    // Commit the surface to initiate the initial configure event
    wl_surface_commit(surface);

    // Enter the main event loop
    while (running && wl_display_dispatch(display) != -1) {
        // Loop until 'running' is set to 0
    }

    // Cleanup
    if (buffer) {
        wl_buffer_destroy(buffer);
    }
    if (shm) {
        wl_shm_destroy(shm);
    }
    xdg_toplevel_destroy(xdg_toplevel);
    xdg_surface_destroy(xdg_surface);
    wl_surface_destroy(surface);
    xdg_wm_base_destroy(wm_base);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    return 0;
}
