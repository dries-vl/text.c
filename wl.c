#include <sys/mman.h>

struct wl_compositor *compositor = NULL; // compositor api (creates surfaces, can have subsurfaces and overlay)
struct xdg_wm_base *wm_base = NULL; // wm api (creates 'toplevel' surfaces ~= windows)

static void registry_handler(void* data,
                                   struct wl_registry* registry,
                                   uint32_t name,
                                   const char* interface,
                                   uint32_t version)
{
    // This function gets called once for each available interface
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, name,
                                    &wl_compositor_interface, 4);
    }
    else if (strcmp(interface, "xdg_wm_base") == 0) {
        wm_base = wl_registry_bind(registry, name,
                                 &xdg_wm_base_interface, 1);
    }
    else if (strcmp(interface, "wl_subcompositor") == 0) {
        subcompositor = wl_registry_bind(registry, name,
                                       &wl_subcompositor_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0) {
        shm = wl_registry_bind(registry, name,
                             &wl_shm_interface, 1);
    }
}
// callback to handle the removal of global registry objects.
static void registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    // No action needed when a global is removed in this simple example
}
static const struct wl_registry_listener registry_listener = {
    .global = registry_handler,        // Called when a global object is announced
    .global_remove = registry_remover, // Called when a global object is removed
};

// callback for ping events from the compositor to keep the connection alive.
static void wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial); // send pong with the same serial number
}
static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping, // Called when the compositor sends a ping
};

// Drawing function for CPU rendering
void draw_to_subsurface() {
    uint32_t *pixels = buffer_data;
    // Draw something...
    for (int i = 0; i < 256 * 256; i++) {
        pixels[i] = 0xFF0000FF;  // Example: solid blue
    }
    wl_surface_attach(second_surface, cpu_buffer, 0, 0);
    wl_surface_damage_buffer(second_surface, 0, 0, width, height);
    wl_surface_commit(second_surface);
}

// Helper function to create shared memory
static int create_shared_memory(size_t size) {
    char template[] = "/tmp/wayland-shared-XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0)
        return -1;

    unlink(template);
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int create_cpu_subsurface(void)
{
    second_surface = wl_compositor_create_surface(compositor);
    subsurface = wl_subcompositor_get_subsurface(subcompositor, second_surface, first_surface);

    // Setup shared memory buffer
    const int width = 256;
    int height = 256;
    int stride = width * 4;
    int size = stride * height;

    int fd = create_shared_memory(size);
    if (fd < 0) {
        fprintf(stderr, "Failed to create shared memory\n");
        return -1;
    }

    buffer_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer_data == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap shared memory\n");
        close(fd);
        return -1;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    cpu_buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
                                         stride, WL_SHM_FORMAT_ARGB8888);
    // todo: what is this? needed here or in cleanup
    close(fd);
    wl_shm_pool_destroy(pool);
    return 0;
}

// declare draw_egl() to avoid symbol not found
void draw_egl(); // egl.c
// callback that the compositor calls when a frame is done
static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    wl_callback_destroy(callback);
    // if still running, draw the next frame
    if (running) {
        draw_to_subsurface();
        draw_egl();
    }
}
static const struct wl_callback_listener frame_listener = {
    .done = frame_done,
};
