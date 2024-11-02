// TODO: 
#include "framework.h"          // Ensure this is correctly implemented and included
#include "wgpu.h"               // WebGPU header
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h> // Ensure this header is generated and available

#define LOG_PREFIX "[triangle]"

// Forward declarations for Wayland interfaces
struct wl_compositor;
struct xdg_wm_base;
struct wl_seat;
struct wl_keyboard;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;

// Structure to hold Wayland and WebGPU related objects
struct demo {
    // Wayland objects
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface; // Ensure this is a pointer
    struct xdg_toplevel *xdg_toplevel;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;

    // WebGPU objects
    WGPUInstance instance;
    WGPUSurface surface;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUSurfaceConfiguration config;

    // Window properties
    int width;
    int height;

    // Serial number for configure acknowledgment
    uint32_t current_serial;

    // Flags
    bool running;
};

// Function prototypes for request handlers
static void handle_request_adapter(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, char const *message,
                                   void *userdata);
static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device, char const *message,
                                  void *userdata);

// Wayland registry listener to bind to global interfaces
static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
    struct demo *demo = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        demo->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        demo->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        demo->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
                                          uint32_t name) {
    // No action needed for global remove in this simple example
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

// XDG WM Base listener for ping event
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial) {
    struct demo *demo = data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

// XDG Toplevel listener for configure event (e.g., resize)
static void xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                                         int32_t width, int32_t height,
                                         struct wl_array *states) {
    struct demo *demo = data;

    if (width > 0 && height > 0) {
        demo->width = width;
        demo->height = height;
        // Reconfigure WebGPU surface with new size
        demo->config.width = width;
        demo->config.height = height;
        wgpuSurfaceConfigure(demo->surface, &demo->config);
    }

    // Acknowledge the configure event with the current serial
    xdg_surface_ack_configure(demo->xdg_surface, demo->current_serial++);
}

static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    struct demo *demo = data;
    demo->running = false;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = xdg_toplevel_handle_configure,
    .close = xdg_toplevel_handle_close,
};

// Keyboard event handlers
static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                                   uint32_t format, int fd, uint32_t size) {
    // Not implemented
    close(fd);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys) {
    // Not implemented
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface) {
    // Not implemented
}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state) {
    struct demo *demo = data;

    // Key press handling
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // Assuming key codes are similar to Linux's key codes
        // 'R' key is usually key code 26
        if (key == 26) { // 'R' key
            printf(LOG_PREFIX " 'R' key pressed. Report generation not implemented.\n");
        }

        // Handle Escape key to close the window
        if (key == 9) { // Escape key
            demo->running = false;
        }
    }
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked,
                                      uint32_t group) {
    // Not implemented
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *keyboard,
                                        int32_t rate, int32_t delay) {
    // Not implemented
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info,
};

// Function to handle Wayland events and dispatch them
static void handle_wayland_events(struct demo *demo) {
    while (wl_display_dispatch_pending(demo->display) != -1) {
        // Dispatch all pending events
    }
}

// Function to create Wayland surface and XDG surface
static bool setup_wayland_surface(struct demo *demo) {
    if (!demo->compositor || !demo->xdg_wm_base) {
        fprintf(stderr, LOG_PREFIX " Missing Wayland compositor or xdg_wm_base\n");
        return false;
    }

    demo->wl_surface = wl_compositor_create_surface(demo->compositor);
    if (!demo->wl_surface) {
        fprintf(stderr, LOG_PREFIX " Failed to create Wayland surface\n");
        return false;
    }

    demo->xdg_surface = xdg_wm_base_get_xdg_surface(demo->xdg_wm_base, demo->wl_surface);
    if (!demo->xdg_surface) {
        fprintf(stderr, LOG_PREFIX " Failed to create XDG surface\n");
        return false;
    }

    demo->xdg_toplevel = xdg_surface_get_toplevel(demo->xdg_surface);
    if (!demo->xdg_toplevel) {
        fprintf(stderr, LOG_PREFIX " Failed to get XDG toplevel\n");
        return false;
    }

    xdg_toplevel_add_listener(demo->xdg_toplevel, &toplevel_listener, demo);
    // Note: wm_base_listener is added in initialize_wayland

    xdg_toplevel_set_title(demo->xdg_toplevel, "triangle [wgpu-native + Wayland]");
    wl_surface_commit(demo->wl_surface); // Commit the surface

    wl_display_roundtrip(demo->display); // Ensure all requests are processed

    return true;
}

// Function to initialize Wayland and set up the window
static bool initialize_wayland(struct demo *demo) {
    demo->display = wl_display_connect(NULL);
    if (!demo->display) {
        fprintf(stderr, LOG_PREFIX " Failed to connect to Wayland display\n");
        return false;
    }

    demo->registry = wl_display_get_registry(demo->display);
    wl_registry_add_listener(demo->registry, &registry_listener, demo);
    wl_display_roundtrip(demo->display);

    if (!setup_wayland_surface(demo)) {
        return false;
    }

    // Set up XDG WM base listener
    xdg_wm_base_add_listener(demo->xdg_wm_base, &wm_base_listener, demo);
    wl_display_roundtrip(demo->display);

    // Get the keyboard from the seat
    if (demo->seat) {
        demo->keyboard = wl_seat_get_keyboard(demo->seat);
        if (demo->keyboard) {
            wl_keyboard_add_listener(demo->keyboard, &keyboard_listener, demo);
        }
    }

    // Initial window size
    demo->width = 640;
    demo->height = 480;

    return true;
}

// Function to handle window resizing
static void handle_resize(struct demo *demo, int width, int height) {
    if (width == 0 || height == 0) {
        return;
    }

    demo->config.width = width;
    demo->config.height = height;

    wgpuSurfaceConfigure(demo->surface, &demo->config);
}

// Handler for adapter requests
static void handle_request_adapter(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, char const *message,
                                   void *userdata) {
    if (status == WGPURequestAdapterStatus_Success) {
        struct demo *demo = userdata;
        demo->adapter = adapter;
        printf(LOG_PREFIX " Adapter requested successfully.\n");
    } else {
        printf(LOG_PREFIX " request_adapter failed with status=%#.8x message=%s\n", status, message);
        // Optionally, set demo->running to false to exit on failure
    }
}

// Handler for device requests
static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device, char const *message,
                                  void *userdata) {
    if (status == WGPURequestDeviceStatus_Success) {
        struct demo *demo = userdata;
        demo->device = device;
        printf(LOG_PREFIX " Device requested successfully.\n");
    } else {
        printf(LOG_PREFIX " request_device failed with status=%#.8x message=%s\n", status, message);
        // Optionally, set demo->running to false to exit on failure
    }
}

// Main function
int main(int argc, char *argv[]) {
    UNUSED(argc)
    UNUSED(argv)

    // Initialize logging if using the framework
    frmwrk_setup_logging(WGPULogLevel_Warn);

    struct demo demo = {0};
    demo.running = true;
    demo.current_serial = 1; // Initialize serial counter

    // Initialize Wayland
    if (!initialize_wayland(&demo)) {
        fprintf(stderr, LOG_PREFIX " Wayland initialization failed\n");
        exit(EXIT_FAILURE);
    }

    // Create WebGPU instance
    demo.instance = wgpuCreateInstance(NULL);
    assert(demo.instance);

    // Create WebGPU surface using Wayland surface
    WGPUSurfaceDescriptorFromWaylandSurface wayland_desc = {
        .chain = {
            .sType = WGPUSType_SurfaceDescriptorFromWaylandSurface,
            .next = NULL,
        },
        .display = demo.display,
        .surface = demo.wl_surface,
    };

    WGPUSurfaceDescriptor surface_desc = {
        .nextInChain = (const WGPUChainedStruct *)&wayland_desc,
        .label = "Wayland Surface",
    };

    demo.surface = wgpuInstanceCreateSurface(demo.instance, &surface_desc);
    assert(demo.surface);

    // Request WebGPU adapter
    wgpuInstanceRequestAdapter(demo.instance,
                               &(WGPURequestAdapterOptions){
                                   .compatibleSurface = demo.surface,
                               },
                               handle_request_adapter, &demo);

    // Process events until adapter is set
    while (!demo.adapter && demo.running) {
        handle_wayland_events(&demo);
    }

    if (!demo.adapter) {
        fprintf(stderr, LOG_PREFIX " Failed to obtain WebGPU adapter.\n");
        exit(EXIT_FAILURE);
    }

    // Print adapter information using the framework
    frmwrk_print_adapter_info(demo.adapter);

    // Request WebGPU device
    wgpuAdapterRequestDevice(demo.adapter, NULL, handle_request_device, &demo);

    // Process events until device is set
    while (!demo.device && demo.running) {
        handle_wayland_events(&demo);
    }

    if (!demo.device) {
        fprintf(stderr, LOG_PREFIX " Failed to obtain WebGPU device.\n");
        exit(EXIT_FAILURE);
    }

    WGPUQueue queue = wgpuDeviceGetQueue(demo.device);
    assert(queue);

    WGPUShaderModule shader_module =
        frmwrk_load_shader_module(demo.device, "shader.wgsl");
    assert(shader_module);

    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(
        demo.device, &(const WGPUPipelineLayoutDescriptor){
                         .label = "pipeline_layout",
                     });
    assert(pipeline_layout);

    WGPUSurfaceCapabilities surface_capabilities = {0};
    wgpuSurfaceGetCapabilities(demo.surface, demo.adapter, &surface_capabilities);

    WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(
        demo.device,
        &(const WGPURenderPipelineDescriptor){
            .label = "render_pipeline",
            .layout = pipeline_layout,
            .vertex =
                (const WGPUVertexState){
                    .module = shader_module,
                    .entryPoint = "vs_main",
                },
            .fragment =
                &(const WGPUFragmentState){
                    .module = shader_module,
                    .entryPoint = "fs_main",
                    .targetCount = 1,
                    .targets =
                        (const WGPUColorTargetState[]){
                            (const WGPUColorTargetState){
                                .format = surface_capabilities.formats[0],
                                .writeMask = WGPUColorWriteMask_All,
                            },
                        },
                },
            .primitive =
                (const WGPUPrimitiveState){
                    .topology = WGPUPrimitiveTopology_TriangleList,
                },
            .multisample =
                (const WGPUMultisampleState){
                    .count = 1,
                    .mask = 0xFFFFFFFF,
                },
        });
    assert(render_pipeline);

    demo.config = (const WGPUSurfaceConfiguration){
        .device = demo.device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = surface_capabilities.formats[0],
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = surface_capabilities.alphaModes[0],
    };

    demo.config.width = demo.width;
    demo.config.height = demo.height;

    wgpuSurfaceConfigure(demo.surface, &demo.config);

    // Main event and rendering loop
    while (demo.running) {
        // Handle Wayland events
        handle_wayland_events(&demo);

        // Acquire the next WebGPU surface texture
        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(demo.surface, &surface_texture);
        switch (surface_texture.status) {
        case WGPUSurfaceGetCurrentTextureStatus_Success:
            // All good, could check for `surface_texture.suboptimal` here.
            break;
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Lost: {
            // Skip this frame, and re-configure surface.
            if (surface_texture.texture != NULL) {
                wgpuTextureRelease(surface_texture.texture);
            }
            // Here you should query the actual window size from Wayland
            // For simplicity, we use the existing `demo.width` and `demo.height`
            handle_resize(&demo, demo.width, demo.height);
            continue;
        }
        case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
        case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        case WGPUSurfaceGetCurrentTextureStatus_Force32:
            // Fatal error
            printf(LOG_PREFIX " get_current_texture status=%#.8x\n",
                   surface_texture.status);
            abort();
        }
        assert(surface_texture.texture);

        WGPUTextureView frame =
            wgpuTextureCreateView(surface_texture.texture, NULL);
        assert(frame);

        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
            demo.device, &(const WGPUCommandEncoderDescriptor){
                             .label = "command_encoder",
                         });
        assert(command_encoder);

        WGPURenderPassEncoder render_pass_encoder =
            wgpuCommandEncoderBeginRenderPass(
                command_encoder,
                &(const WGPURenderPassDescriptor){
                    .label = "render_pass_encoder",
                    .colorAttachmentCount = 1,
                    .colorAttachments =
                        (const WGPURenderPassColorAttachment[]){
                            (const WGPURenderPassColorAttachment){
                                .view = frame,
                                .loadOp = WGPULoadOp_Clear,
                                .storeOp = WGPUStoreOp_Store,
                                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                                .clearValue =
                                    (const WGPUColor){
                                        .r = 0.0,
                                        .g = 1.0,
                                        .b = 0.0,
                                        .a = 1.0,
                                    },
                            },
                        },
                });
        assert(render_pass_encoder);

        wgpuRenderPassEncoderSetPipeline(render_pass_encoder, render_pipeline);
        wgpuRenderPassEncoderDraw(render_pass_encoder, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(render_pass_encoder);
        wgpuRenderPassEncoderRelease(render_pass_encoder);

        WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
            command_encoder, &(const WGPUCommandBufferDescriptor){
                                 .label = "command_buffer",
                             });
        assert(command_buffer);

        wgpuQueueSubmit(queue, 1, (const WGPUCommandBuffer[]){command_buffer});
        wgpuSurfacePresent(demo.surface);

        wgpuCommandBufferRelease(command_buffer);
        wgpuCommandEncoderRelease(command_encoder);
        wgpuTextureViewRelease(frame);
        wgpuTextureRelease(surface_texture.texture);
    }

    // Cleanup WebGPU resources
    wgpuRenderPipelineRelease(render_pipeline);
    wgpuPipelineLayoutRelease(pipeline_layout);
    wgpuShaderModuleRelease(shader_module);
    wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(demo.device);
    wgpuAdapterRelease(demo.adapter);
    wgpuSurfaceRelease(demo.surface);
    wgpuInstanceRelease(demo.instance);

    // Cleanup Wayland resources
    if (demo.keyboard) {
        wl_keyboard_destroy(demo.keyboard);
    }
    if (demo.xdg_toplevel) {
        xdg_toplevel_destroy(demo.xdg_toplevel);
    }
    if (demo.xdg_surface) { // Ensure xdg_surface is a pointer
        xdg_surface_destroy(demo.xdg_surface);
    }
    if (demo.wl_surface) {
        wl_surface_destroy(demo.wl_surface);
    }
    if (demo.xdg_wm_base) {
        xdg_wm_base_destroy(demo.xdg_wm_base);
    }
    if (demo.compositor) {
        wl_compositor_destroy(demo.compositor);
    }
    if (demo.registry) {
        wl_registry_destroy(demo.registry);
    }
    if (demo.display) {
        wl_display_disconnect(demo.display);
    }

    printf(LOG_PREFIX " Application terminated successfully.\n");
    return 0;
}
