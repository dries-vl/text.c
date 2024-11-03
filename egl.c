EGLDisplay egl_display_var = EGL_NO_DISPLAY;   // Represents the EGL display connection
EGLContext egl_context = EGL_NO_CONTEXT;       // Represents the EGL rendering context
EGLSurface egl_surface = EGL_NO_SURFACE;       // Represents the EGL window surface
EGLConfig egl_config;                          // Holds the EGL frame buffer configuration
struct wl_egl_window *egl_window = NULL;       // Represents the Wayland EGL window
GLuint shader_program = 0;                     // OpenGL shader program identifier
GLuint vbo = 0;                                // Vertex Buffer Object identifier

void print_egl_error(const char *msg) {
    EGLint error = eglGetError();
    fprintf(stderr, "%s: EGL error 0x%X\n", msg, error);
}

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
    int i = 'a';
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
    egl_window = wl_egl_window_create(first_surface, width, height);
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

void draw_egl() {
    // Clear the color buffer NEEDED EVERY FRAME?
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
    struct wl_callback *callback = wl_surface_frame(first_surface);
    wl_callback_add_listener(callback, &frame_listener, NULL);

    // Commit the surface to display the frame
    wl_surface_commit(first_surface);
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

void cleanup_wl_xdg(void)
{
    if (window) {
        xdg_toplevel_destroy(window);
    }
    if (xdg_surface) {
        xdg_surface_destroy(xdg_surface);
    }
    if (first_surface) {
        wl_surface_destroy(first_surface);
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
}
