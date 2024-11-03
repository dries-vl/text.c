#include <stdio.h>                      // Provides input and output functions like printf and fprintf
#include <stdlib.h>                     // Includes general utility functions like malloc, free, and exit
#include <wayland-client.h>             // Contains Wayland client API functions and structures for interacting with the Wayland compositor
#include <xdg-shell-client-protocol.h>  // Defines the XDG Shell protocol used for window management (creating surfaces, handling events, etc.)
#include <string.h>                     // Offers string manipulation functions like strcmp and strlen
#include <unistd.h>                     // Provides access to POSIX operating system API, including functions like close and unlink
#include <EGL/egl.h>                    // EGL library for creating rendering contexts and surfaces
#include <GLES2/gl2.h>                  // OpenGL ES 2.0 library for rendering graphics
#include <wayland-egl.h>                // Wayland EGL integration (wl_egl_window)

// Wayland state (more in wl.c)
struct wl_display *display = NULL; // Represents the connection to the Wayland display server (compositor)
struct wl_registry *registry = NULL; // Used to interact with the registry, which lists all global objects available from the compositor

// Wayland main surface
struct wl_surface *first_surface = NULL; // Represents a drawable area (window) created by the compositor

// Wayland subsurface
struct wl_surface *second_surface;
struct wl_subsurface *subsurface;
struct wl_buffer *cpu_buffer;
void *buffer_data;
struct wl_shm *shm = NULL;
struct wl_subcompositor *subcompositor = NULL;

// XDG state
struct xdg_surface *xdg_surface = NULL; // Wraps a wl_surface with XDG-specific functionality
struct xdg_toplevel *window = NULL; // Represents a top-level window (like an application window) with decorations and title

// Global state
int running = 1; // Flag to control the main event loop; when set to 0, the program will exit
int width = 640; // Width of the window in pixels
int height = 480; // Height of the window in pixels

#include "wl.c"
#include "egl.c"

// callback to configure events for the XDG surface.
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    if (egl_display_var == EGL_NO_DISPLAY) {
        init_egl();
        draw_to_subsurface();
        draw_egl(); // initial call to draw, after that the frame_done callback keeps calling the next frame
    }
}
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure, // Called when the compositor configures the surface
};

// callback for resize events and state changes
static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                               int32_t new_width, int32_t new_height,
                               struct wl_array *states) {
    if (new_width > 0 && new_height > 0) {
        width = new_width;
        height = new_height;
        // reconfigure egl viewport and window for new size
        if (egl_display_var != EGL_NO_DISPLAY) {
            glViewport(0, 0, width, height);
            wl_egl_window_resize(egl_window, width, height, 0, 0);
        }
    }
}
static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    running = 0; // Set the running flag to 0 to exit the main event loop
}
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure, // Called when the compositor configures the toplevel surface
    .close = toplevel_close,         // Called when the user requests to close the window
};

int main(int argc, char **argv) {
    display = wl_display_connect(NULL); // NULL means to connect to the default Wayland display
    if (display == NULL) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return -1;
    }
    registry = wl_display_get_registry(display); // registry is list of available interfaces to connect
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display); // make sure the registry events are received
    if (compositor == NULL || wm_base == NULL) {
        fprintf(stderr, "Required Wayland interfaces not found\n");
        wl_display_disconnect(display);
        return -1;
    }
    xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
    first_surface = wl_compositor_create_surface(compositor);
    if (first_surface == NULL) {
        fprintf(stderr, "Failed to create surface\n");
        wl_display_disconnect(display);
        return -1;
    }
    xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, first_surface);
    if (xdg_surface == NULL) {
        fprintf(stderr, "Failed to get xdg_surface\n");
        wl_display_disconnect(display);
        return -1;
    }

    if (create_cpu_subsurface() != 0)
    {
        return -1;
    }

    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    window = xdg_surface_get_toplevel(xdg_surface);
    if (window == NULL) {
        fprintf(stderr, "Failed to get xdg_toplevel\n");
        wl_display_disconnect(display);
        return -1;
    }
    xdg_toplevel_add_listener(window, &toplevel_listener, NULL);
    xdg_toplevel_set_title(window, "OpenGL ES Triangle");
    wl_surface_commit(first_surface);
    wl_display_roundtrip(display); // make sure the surface is configured
    // event loop that listens to events like mouseclicks, keyboard, resize, ...
    while (running && wl_display_dispatch(display) != -1) {
        // The event loop processes Wayland events and keeps the application responsive
    }

    // cleanup resources before exiting the program todo: why? isn't everything in the process destroyed anyway?
    cleanup_egl();
    cleanup_wl_xdg();
    return 0;
}
