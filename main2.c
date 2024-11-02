#include <stdio.h>                      // Provides input and output functions like printf and fprintf
#include <stdlib.h>                     // Includes general utility functions like malloc, free, and exit
#include <wayland-client.h>             // Contains Wayland client API functions and structures for interacting with the Wayland compositor
#include <xdg-shell-client-protocol.h>  // Defines the XDG Shell protocol used for window management (creating surfaces, handling events, etc.)
#include <string.h>                     // Offers string manipulation functions like strcmp and strlen
#include <unistd.h>                     // Provides access to POSIX operating system API, including functions like close and unlink
#include <EGL/egl.h>                    // EGL library for creating rendering contexts and surfaces
#include <GLES2/gl2.h>                  // OpenGL ES 2.0 library for rendering graphics
#include <wayland-egl.h>                // Wayland EGL integration (wl_egl_window)

enum my_enum {
  THIS,
  NEXT
};

struct wl_display *display = NULL;            // Represents the connection to the Wayland display server (compositor)
struct wl_registry *registry = NULL;          // Used to interact with the registry, which lists all global objects available from the compositor
struct wl_compositor *compositor = NULL;      // Allows the creation of surfaces (windows) where graphics can be drawn
struct wl_surface *surface = NULL;            // Represents a drawable area (window) created by the compositor
struct xdg_wm_base *wm_base = NULL;           // Provides the XDG Shell interface for window management (creating toplevel surfaces)
struct xdg_surface *xdg_surface = NULL;       // Wraps a wl_surface with XDG-specific functionality
struct xdg_toplevel *xdg_toplevel = NULL;     // Represents a top-level window (like an application window) with decorations and title
int running = 1;                               // Flag to control the main event loop; when set to 0, the program will exit
int width = 640;                               // Width of the window in pixels
int height = 480;                              // Height of the window in pixels

EGLDisplay egl_display_var = EGL_NO_DISPLAY;   // Represents the EGL display connection
EGLContext egl_context = EGL_NO_CONTEXT;       // Represents the EGL rendering context
EGLSurface egl_surface = EGL_NO_SURFACE;       // Represents the EGL window surface
EGLConfig egl_config;                          // Holds the EGL frame buffer configuration
struct wl_egl_window *egl_window = NULL;       // Represents the Wayland EGL window

GLuint shader_program = 0;                     // OpenGL shader program identifier
GLuint vbo = 0;                                // Vertex Buffer Object identifier

// Forward declarations of functions
void init_egl();       // Initializes EGL and creates an OpenGL ES context
void draw();           // Renders a frame using OpenGL ES
void cleanup_egl();    // Cleans up EGL resources before exiting the program
GLuint compile_shader(const char *source, GLenum type); // Compiles a shader from source
GLuint link_program(GLuint vertex_shader, GLuint fragment_shader); // Links vertex and fragment shaders into a program

void print_egl_error(const char *msg) {
    EGLint error = eglGetError();
    fprintf(stderr, "%s: EGL error 0x%X\n", msg, error);
}

// callback to handle global registry events by binding to the required interfaces.
static void registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        // Bind to the wl_compositor interface version 4 (use the highest version supported)
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    }
    else if (strcmp(interface, "xdg_wm_base") == 0) {
        // Bind to the xdg_wm_base interface version 1
        wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
    }
}
// callback to handle the removal of global registry objects.
static void registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    // No action needed when a global is removed in this simple example
}
// create the registry listener with the handler and remover callbacks
static const struct wl_registry_listener registry_listener = {
    .global = registry_handler,        // Called when a global object is announced
    .global_remove = registry_remover, // Called when a global object is removed
};

// callback for ping events from the compositor to keep the connection alive.
static void wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial); // send pong with the same serial number
}
// create the xdg_wm_base listener with the ping callback
static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping, // Called when the compositor sends a ping
};

// callback to configure events for the XDG surface.
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    // Acknowledge the configure event by sending an ack with the same serial number
    xdg_surface_ack_configure(xdg_surface, serial);

    // Initialize EGL after the surface is ready
    if (egl_display_var == EGL_NO_DISPLAY) {
        // Initialize EGL only once
        init_egl();

        // Start rendering
        draw();
    }
}
// create xdg_surface listener with the configure callback
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure, // Called when the compositor configures the surface
};

// callback to handle configure events for the toplevel surface.
static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                               int32_t new_width, int32_t new_height,
                               struct wl_array *states) {
    // Update the window size if provided
    if (new_width > 0 && new_height > 0) {
        width = new_width;
        height = new_height;
        // Update the viewport if EGL is initialized
        if (egl_display_var != EGL_NO_DISPLAY) {
            glViewport(0, 0, width, height);
            // Resize the wl_egl_window
            wl_egl_window_resize(egl_window, width, height, 0, 0);
        }
    }
}
// callback to handle the close event for the toplevel surface.
static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    running = 0; // Set the running flag to 0 to exit the main event loop
}
// create xdg_toplevel listener with the configure and close callbacks
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure, // Called when the compositor configures the toplevel surface
    .close = toplevel_close,         // Called when the user requests to close the window
};

// callback function to synchronize rendering with the compositor.
static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    wl_callback_destroy(callback); // Destroy the previous callback

    if (running) {
        draw(); // Render the next frame
    }
}
// create frame listener with the done callback
static const struct wl_callback_listener frame_listener = {
    .done = frame_done,
};

GLuint compile_shader(const char *source, GLenum type) {
    GLuint shader = glCreateShader(type);
    if (!shader) {
        fprintf(stderr, "Failed to create shader of type %d\n", type);
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for compilation errors
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        char *log = malloc(log_length);
        glGetShaderInfoLog(shader, log_length, NULL, log);
        fprintf(stderr, "Shader compilation failed: %s\n", log);
        free(log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}
// links the shaders together into a 'shader program'
GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    if (!program) {
        fprintf(stderr, "Failed to create shader program\n");
        return 0;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    // Check for linking errors
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        char *log = malloc(log_length);
        glGetProgramInfoLog(program, log_length, NULL, log);
        fprintf(stderr, "Program linking failed: %s\n", log);
        free(log);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}
// initialize EGL, compile shaders, set up OpenGL ES resources
void init_egl() {
    // Get the EGL display connection
    egl_display_var = eglGetDisplay((EGLNativeDisplayType)display);
    if (egl_display_var == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display\n");
        exit(1);
    }
    // Initialize the EGL display connection
    if (!eglInitialize(egl_display_var, NULL, NULL)) {
        print_egl_error("Failed to initialize EGL");
        exit(1);
    }
    // Check available EGL extensions
    const char *extensions = eglQueryString(egl_display_var, EGL_EXTENSIONS);
    if (!extensions || !strstr(extensions, "EGL_KHR_platform_wayland")) {
        printf("EGL_KHR_platform_wayland not supported. Falling back to wl_egl_window.\n");
        // Proceed with wl_egl_window
    }
    // Choose an appropriate EGL frame buffer configuration
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLint num_configs;
    if (!eglChooseConfig(egl_display_var, config_attribs, &egl_config, 1, &num_configs) || num_configs == 0) {
        print_egl_error("Failed to choose EGL config");
        eglTerminate(egl_display_var);
        exit(1);
    }
    // Create a wl_egl_window
    egl_window = wl_egl_window_create(surface, width, height);
    if (!egl_window) {
        fprintf(stderr, "Failed to create wl_egl_window\n");
        eglTerminate(egl_display_var);
        exit(1);
    }
    // Create an EGL window surface using wl_egl_window
    egl_surface = eglCreateWindowSurface(egl_display_var, egl_config, (EGLNativeWindowType)egl_window, NULL);
    if (egl_surface == EGL_NO_SURFACE) {
        print_egl_error("Failed to create EGL surface");
        wl_egl_window_destroy(egl_window);
        eglTerminate(egl_display_var);
        exit(1);
    }
    // Create an EGL rendering context
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, // Request OpenGL ES 2.0
        EGL_NONE
    };
    egl_context = eglCreateContext(egl_display_var, egl_config, EGL_NO_CONTEXT, context_attribs);
    if (egl_context == EGL_NO_CONTEXT) {
        print_egl_error("Failed to create EGL context");
        eglDestroySurface(egl_display_var, egl_surface);
        wl_egl_window_destroy(egl_window);
        eglTerminate(egl_display_var);
        exit(1);
    }
    // Make the context current
    if (!eglMakeCurrent(egl_display_var, egl_surface, egl_surface, egl_context)) {
        print_egl_error("Failed to make EGL context current");
        eglDestroyContext(egl_display_var, egl_context);
        eglDestroySurface(egl_display_var, egl_surface);
        wl_egl_window_destroy(egl_window);
        eglTerminate(egl_display_var);
        exit(1);
    }
    printf("EGL initialized successfully with wl_egl_window.\n");
    // Define shader source code
    const char *vertex_shader_source =
        "attribute vec2 position;\n"
        "attribute vec3 color;\n"
        "varying vec3 v_color;\n"
        "void main() {\n"
        "    v_color = color;\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "}\n";
    const char *fragment_shader_source =
        "precision mediump float;\n"
        "varying vec3 v_color;\n"
        "void main() {\n"
        "    gl_FragColor = vec4(v_color, 1.0);\n"
        "}\n";
    // Compile shaders
    GLuint vertex_shader = compile_shader(vertex_shader_source, GL_VERTEX_SHADER);
    if (!vertex_shader) {
        fprintf(stderr, "Vertex shader compilation failed\n");
        exit(1);
    }
    GLuint fragment_shader = compile_shader(fragment_shader_source, GL_FRAGMENT_SHADER);
    if (!fragment_shader) {
        fprintf(stderr, "Fragment shader compilation failed\n");
        glDeleteShader(vertex_shader);
        exit(1);
    }
    // Link shaders into a program
    shader_program = link_program(vertex_shader, fragment_shader);
    if (!shader_program) {
        fprintf(stderr, "Shader program linking failed\n");
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        exit(1);
    }
    // Shaders are linked into the program; they can be deleted now
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    // Define vertex data for a colored triangle
    GLfloat vertices[] = {
        // Positions    // Colors
         0.0f,  0.5f,    1.0f, 0.0f, 0.0f, // Top vertex (Red)
        -0.5f, -0.5f,    0.0f, 1.0f, 0.0f, // Bottom left vertex (Green)
         0.5f, -0.5f,    0.0f, 0.0f, 1.0f  // Bottom right vertex (Blue)
    };
    // Generate and bind a vertex buffer ('vertex buffer object')
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo); // becomes the currently bound buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // Use the shader program
    glUseProgram(shader_program);
    // find attributes in the shader program (need to use program first)
    GLint pos_attrib = glGetAttribLocation(shader_program, "position");
    GLint col_attrib = glGetAttribLocation(shader_program, "color");
    // set these attributes as being found in an array
    glEnableVertexAttribArray(pos_attrib);
    glEnableVertexAttribArray(col_attrib);
    // specify that the data for the attributes can be found in certain offsets and size in the array
    // gl knows that this concerns the vertex buffer, because it is the currently bound buffer
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0);
    glVertexAttribPointer(col_attrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
}

void draw() {
    // Clear the color buffer
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // Dark teal background
    glClear(GL_COLOR_BUFFER_BIT);

    // THIS PART IS NOT NEEDED EVERY FRAME, CAN AVOID IF NO CHANGES BETWEEN FRAMES
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glUseProgram(shader_program);
    GLint pos_attrib = glGetAttribLocation(shader_program, "position");
    GLint col_attrib = glGetAttribLocation(shader_program, "color");
    glEnableVertexAttribArray(pos_attrib);
    glEnableVertexAttribArray(col_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0);
    glVertexAttribPointer(col_attrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    // END PART THAT CAN BE AVOIDED EACH FRAME

    // Draw the triangle
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // ALSO NOT NEEDED EVERY FRAME IF NOTHING CHANGES
    glDisableVertexAttribArray(pos_attrib);
    glDisableVertexAttribArray(col_attrib);
    // END PART

    // Swap the front and back buffers to display the rendered image
    if (!eglSwapBuffers(egl_display_var, egl_surface)) {
        print_egl_error("Failed to swap buffers");
    }

    // Request the next frame callback
    struct wl_callback *callback = wl_surface_frame(surface);
    wl_callback_add_listener(callback, &frame_listener, NULL);

    // Commit the surface to display the frame
    wl_surface_commit(surface);
}

void cleanup_egl() {
    if (egl_display_var != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_display_var, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_context != EGL_NO_CONTEXT) {
            glDeleteProgram(shader_program); // Delete shader program
            glDeleteBuffers(1, &vbo);        // Delete VBO
            eglDestroyContext(egl_display_var, egl_context);
        }
        if (egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(egl_display_var, egl_surface);
        }
        if (egl_window) {
            wl_egl_window_destroy(egl_window);
        }
        eglTerminate(egl_display_var);
        egl_display_var = EGL_NO_DISPLAY;
    }
}

int main(int argc, char **argv) {
    // Step 1: Connect to the Wayland display server (compositor)
    display = wl_display_connect(NULL); // NULL means to connect to the default Wayland display
    if (display == NULL) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return -1;
    }

    // Step 2: Retrieve the registry object to enumerate available global interfaces
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display); // Ensure the registry events are received

    // Step 3: Check if all necessary interfaces were successfully bound
    if (compositor == NULL || wm_base == NULL) {
        fprintf(stderr, "Required Wayland interfaces not found\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Step 4: Add the listener for xdg_wm_base to handle ping events
    xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);

    // Step 5: Create a Wayland surface using the compositor
    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create surface\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Step 6: Wrap the Wayland surface with an XDG surface for window management
    xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
    if (xdg_surface == NULL) {
        fprintf(stderr, "Failed to get xdg_surface\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Step 7: Add the XDG surface listener to handle configure events
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

    // Step 8: Create an XDG toplevel object to represent a top-level window (application window)
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    if (xdg_toplevel == NULL) {
        fprintf(stderr, "Failed to get xdg_toplevel\n");
        wl_display_disconnect(display);
        return -1;
    }

    // Step 9: Add the toplevel listener to handle window events like close
    xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, NULL);

    // Step 10: Set the window title using the toplevel interface
    xdg_toplevel_set_title(xdg_toplevel, "OpenGL ES Triangle");

    // Step 11: Commit the surface to request the initial configuration from the compositor
    wl_surface_commit(surface);
    wl_display_roundtrip(display); // Ensure the surface is configured

    // Step 12: Enter the main event loop to process Wayland events
    while (running && wl_display_dispatch(display) != -1) {
        // The event loop processes Wayland events and keeps the application responsive
    }

    // Step 13: Cleanup resources before exiting the program
    cleanup_egl();

    if (xdg_toplevel) {
        xdg_toplevel_destroy(xdg_toplevel);
    }
    if (xdg_surface) {
        xdg_surface_destroy(xdg_surface);
    }
    if (surface) {
        wl_surface_destroy(surface);
    }
    if (wm_base) {
        xdg_wm_base_destroy(wm_base);
    }
    if (compositor) {
        wl_compositor_destroy(compositor);
    }
    if (registry) {
        wl_registry_destroy(registry);
    }
    if (display) {
        wl_display_disconnect(display);
    }

    return 0; // Return 0 to indicate successful execution
}
