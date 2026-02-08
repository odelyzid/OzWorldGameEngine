#include "oz/render/oz_gl_modern.h"
#include "oz/oz_log.h"
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Shader sources
static const char* vertex_shader_src = 
"#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec3 aNormal;\n"
"layout (location = 2) in vec2 aTexCoord;\n"
"uniform mat4 uModel;\n"
"uniform mat4 uView;\n"
"uniform mat4 uProjection;\n"
"out vec3 FragPos;\n"
"out vec3 Normal;\n"
"out vec2 TexCoord;\n"
"void main() {\n"
"    FragPos = vec3(uModel * vec4(aPos, 1.0));\n"
"    Normal = mat3(transpose(inverse(uModel))) * aNormal;\n"
"    TexCoord = aTexCoord;\n"
"    gl_Position = uProjection * uView * vec4(FragPos, 1.0);\n"
"}\n";

static const char* fragment_shader_src =
"#version 330 core\n"
"in vec3 FragPos;\n"
"in vec3 Normal;\n"
"in vec2 TexCoord;\n"
"out vec4 FragColor;\n"
"uniform vec3 uColor;\n"
"uniform vec3 uAmbientLight;\n"
"uniform bool uUseTexture;\n"
"uniform sampler2D uTexture;\n"
"void main() {\n"
"    vec3 color = uColor;\n"
"    if (uUseTexture) {\n"
"        color *= texture(uTexture, TexCoord).rgb;\n"
"    }\n"
"    // Simple ambient lighting for now\n"
"    color *= uAmbientLight;\n"
"    FragColor = vec4(color, 1.0);\n"
"}\n";

// Simple line shader for wireframes/grid
static const char* line_vertex_src =
"#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"uniform mat4 uMVP;\n"
"void main() {\n"
"    gl_Position = uMVP * vec4(aPos, 1.0);\n"
"}\n";

static const char* line_fragment_src =
"#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec3 uColor;\n"
"void main() {\n"
"    FragColor = vec4(uColor, 1.0);\n"
"}\n";

typedef struct {
    float pos[3];
    float normal[3];
    float texcoord[2];
} Vertex;

typedef struct {
    float pos[3];
} LineVertex;

struct OzGLModern {
    // Shaders
    GLuint main_program;
    GLuint line_program;
    
    // Uniforms
    GLint u_model, u_view, u_projection;
    GLint u_color, u_ambient_light, u_use_texture;
    GLint u_mvp_line, u_color_line;
    
    // Buffers for dynamic geometry
    GLuint grid_vao, grid_vbo;
    GLuint line_vao, line_vbo;
    GLuint geometry_vao, geometry_vbo, geometry_ebo;
    
    // Current state
    float view_matrix[16];
    float projection_matrix[16];
    OzVec3 ambient_light;
    
    // Checker texture
    GLuint checker_texture;
    
    // Geometry cache for efficient rendering
    Vertex* vertex_buffer;
    GLuint* index_buffer;
    size_t vertex_capacity;
    size_t index_capacity;
    size_t vertex_count;
    size_t index_count;
};

// Forward declarations
static void draw_box_wireframe(OzGLModern* renderer, const OzBrushBox* box, const OzVec3* color);
static void draw_cylinder_wireframe(OzGLModern* renderer, const OzBrushCylinder* cyl, const OzVec3* color);

static GLuint compile_shader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        OZ_ERROR("Shader compilation failed: %s", info_log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char* vertex_src, const char* fragment_src) {
    GLuint vertex = compile_shader(vertex_src, GL_VERTEX_SHADER);
    GLuint fragment = compile_shader(fragment_src, GL_FRAGMENT_SHADER);
    
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        OZ_ERROR("Program linking failed: %s", info_log);
        glDeleteProgram(program);
        program = 0;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

static void matrix_identity(float* m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void matrix_perspective(float* m, float fov, float aspect, float near, float far) {
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fov * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
}

static void matrix_look_at(float* m, const OzCamera* cam) {
    matrix_identity(m);
    
    // Apply camera rotations (inverse for view matrix)
    float cp = cosf(-cam->pitch), sp = sinf(-cam->pitch);
    float cy = cosf(-cam->yaw), sy = sinf(-cam->yaw);
    
    // View matrix combines rotation and translation
    m[0] = cy; m[1] = 0; m[2] = sy;
    m[4] = sp*sy; m[5] = cp; m[6] = -sp*cy;
    m[8] = -cp*sy; m[9] = sp; m[10] = cp*cy;
    
    // Apply translation
    m[12] = -(m[0]*cam->position.x + m[4]*cam->position.y + m[8]*cam->position.z);
    m[13] = -(m[1]*cam->position.x + m[5]*cam->position.y + m[9]*cam->position.z);
    m[14] = -(m[2]*cam->position.x + m[6]*cam->position.y + m[10]*cam->position.z);
}

static void matrix_multiply(const float* a, const float* b, float* result) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i*4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                result[i*4 + j] += a[i*4 + k] * b[k*4 + j];
            }
        }
    }
}

// Optimized vertex buffer management
static void ensure_vertex_capacity(OzGLModern* renderer, size_t vertices, size_t indices) {
    if (renderer->vertex_count + vertices > renderer->vertex_capacity) {
        renderer->vertex_capacity = (renderer->vertex_capacity + vertices) * 2;
        renderer->vertex_buffer = realloc(renderer->vertex_buffer, 
                                         renderer->vertex_capacity * sizeof(Vertex));
    }
    if (renderer->index_count + indices > renderer->index_capacity) {
        renderer->index_capacity = (renderer->index_capacity + indices) * 2;
        renderer->index_buffer = realloc(renderer->index_buffer,
                                        renderer->index_capacity * sizeof(GLuint));
    }
}

static void reset_geometry_buffers(OzGLModern* renderer) {
    renderer->vertex_count = 0;
    renderer->index_count = 0;
}

// Fast box generation - optimized for cache efficiency
static void generate_box_geometry(OzGLModern* renderer, const OzBrushBox* box) {
    const float cx = box->center.x, cy = box->center.y, cz = box->center.z;
    const float hx = box->half.x, hy = box->half.y, hz = box->half.z;
    
    ensure_vertex_capacity(renderer, 8, 36); // 8 vertices, 36 indices (6 faces * 6 indices per face)
    
    size_t base_vertex = renderer->vertex_count;
    Vertex* vertices = &renderer->vertex_buffer[base_vertex];
    
    // Generate 8 box vertices with normals and UVs
    const float positions[8][3] = {
        {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz},
        {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
        {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz},
        {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz}
    };
    
    for (int i = 0; i < 8; i++) {
        vertices[i].pos[0] = positions[i][0];
        vertices[i].pos[1] = positions[i][1];
        vertices[i].pos[2] = positions[i][2];
        vertices[i].normal[0] = 0; vertices[i].normal[1] = 0; vertices[i].normal[2] = 1; // Will be set per face
        vertices[i].texcoord[0] = 0; vertices[i].texcoord[1] = 0;
    }
    
    renderer->vertex_count += 8;
    
    // Generate indices for 6 faces (using quads as two triangles)
    GLuint* indices = &renderer->index_buffer[renderer->index_count];
    const int faces[6][4] = {
        {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}
    };
    
    int idx = 0;
    for (int f = 0; f < 6; f++) {
        // First triangle
        indices[idx++] = (GLuint)(base_vertex + faces[f][0]);
        indices[idx++] = (GLuint)(base_vertex + faces[f][1]);
        indices[idx++] = (GLuint)(base_vertex + faces[f][2]);
        // Second triangle
        indices[idx++] = (GLuint)(base_vertex + faces[f][0]);
        indices[idx++] = (GLuint)(base_vertex + faces[f][2]);
        indices[idx++] = (GLuint)(base_vertex + faces[f][3]);
    }
    
    renderer->index_count += 36;
}

// Fast cylinder generation with configurable subdivision
static void generate_cylinder_geometry(OzGLModern* renderer, const OzBrushCylinder* cyl) {
    int segments = cyl->segments > 3 ? cyl->segments : 16;
    float hz = cyl->height * 0.5f;
    
    // Calculate required capacity: 2 center vertices + 2*segments rim vertices
    size_t vertex_count = 2 + 2 * segments;
    size_t index_count = segments * 12; // sides (2 triangles per segment) + caps (1 triangle per segment * 2 caps)
    
    ensure_vertex_capacity(renderer, vertex_count, index_count);
    
    size_t base_vertex = renderer->vertex_count;
    Vertex* vertices = &renderer->vertex_buffer[base_vertex];
    
    // Center vertices for caps
    vertices[0] = (Vertex){{cyl->center.x, cyl->center.y, cyl->center.z - hz}, {0, 0, -1}, {0.5f, 0.5f}};
    vertices[1] = (Vertex){{cyl->center.x, cyl->center.y, cyl->center.z + hz}, {0, 0, 1}, {0.5f, 0.5f}};
    
    // Generate rim vertices
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / (float)segments * 2.0f * (float)M_PI;
        float x = cyl->center.x + cosf(angle) * cyl->radius_x;
        float y = cyl->center.y + sinf(angle) * cyl->radius_y;
        float nx = cosf(angle), ny = sinf(angle);
        float u = (float)i / (float)segments;
        
        // Bottom rim
        vertices[2 + i] = (Vertex){{x, y, cyl->center.z - hz}, {nx, ny, 0}, {u, 0}};
        // Top rim
        vertices[2 + segments + i] = (Vertex){{x, y, cyl->center.z + hz}, {nx, ny, 0}, {u, 1}};
    }
    
    renderer->vertex_count += vertex_count;
    
    // Generate indices
    GLuint* indices = &renderer->index_buffer[renderer->index_count];
    int idx = 0;
    
    // Bottom cap
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        indices[idx++] = (GLuint)(base_vertex + 0);
        indices[idx++] = (GLuint)(base_vertex + 2 + next);
        indices[idx++] = (GLuint)(base_vertex + 2 + i);
    }
    
    // Top cap
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        indices[idx++] = (GLuint)(base_vertex + 1);
        indices[idx++] = (GLuint)(base_vertex + 2 + segments + i);
        indices[idx++] = (GLuint)(base_vertex + 2 + segments + next);
    }
    
    // Sides
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        int bottom_i = 2 + i, bottom_next = 2 + next;
        int top_i = 2 + segments + i, top_next = 2 + segments + next;
        
        // First triangle
        indices[idx++] = (GLuint)(base_vertex + bottom_i);
        indices[idx++] = (GLuint)(base_vertex + top_i);
        indices[idx++] = (GLuint)(base_vertex + bottom_next);
        // Second triangle
        indices[idx++] = (GLuint)(base_vertex + bottom_next);
        indices[idx++] = (GLuint)(base_vertex + top_i);
        indices[idx++] = (GLuint)(base_vertex + top_next);
    }
    
    renderer->index_count += idx;
}

static void flush_geometry(OzGLModern* renderer) {
    if (renderer->vertex_count == 0) return;
    
    glBindVertexArray(renderer->geometry_vao);
    
    // Upload vertices
    glBindBuffer(GL_ARRAY_BUFFER, renderer->geometry_vbo);
    glBufferData(GL_ARRAY_BUFFER, renderer->vertex_count * sizeof(Vertex), 
                 renderer->vertex_buffer, GL_DYNAMIC_DRAW);
    
    // Upload indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->geometry_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, renderer->index_count * sizeof(GLuint),
                 renderer->index_buffer, GL_DYNAMIC_DRAW);
    
    // Set up vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));
    glEnableVertexAttribArray(2);
    
    // Use main shader
    glUseProgram(renderer->main_program);
    
    // Set matrices
    float identity[16];
    matrix_identity(identity);
    glUniformMatrix4fv(renderer->u_model, 1, GL_FALSE, identity);
    glUniformMatrix4fv(renderer->u_view, 1, GL_FALSE, renderer->view_matrix);
    glUniformMatrix4fv(renderer->u_projection, 1, GL_FALSE, renderer->projection_matrix);
    
    // Set lighting
    glUniform3f(renderer->u_ambient_light, renderer->ambient_light.x, 
                renderer->ambient_light.y, renderer->ambient_light.z);
    
    // Bind texture and render
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->checker_texture);
    glUniform1i(renderer->u_use_texture, 1);
    glUniform3f(renderer->u_color, 1.0f, 1.0f, 1.0f);
    
    glDrawElements(GL_TRIANGLES, (GLsizei)renderer->index_count, GL_UNSIGNED_INT, 0);
}

static void create_checker_texture(OzGLModern* renderer) {
    const int size = 64;
    unsigned char* pixels = malloc(size * size * 3);
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int checker = ((x / 8) ^ (y / 8)) & 1;
            unsigned char color = checker ? 200 : 120;
            int idx = (y * size + x) * 3;
            pixels[idx] = pixels[idx + 1] = pixels[idx + 2] = color;
        }
    }
    
    glGenTextures(1, &renderer->checker_texture);
    glBindTexture(GL_TEXTURE_2D, renderer->checker_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    free(pixels);
}

bool oz_gl_modern_is_available(void) {
    const char* version_str = (const char*)glGetString(GL_VERSION);
    if (!version_str) return false;
    
    // Check for OpenGL 3.2+ core profile
    int major = 0, minor = 0;
    if (sscanf(version_str, "%d.%d", &major, &minor) == 2) {
        return (major > 3) || (major == 3 && minor >= 2);
    }
    return false;
}

OzGLModern* oz_gl_modern_init(void) {
    if (!oz_gl_modern_is_available()) {
        OZ_ERROR("OpenGL 3.2+ core profile not available");
        return NULL;
    }
    
#ifndef __APPLE__
    // Initialize GLEW for extension loading
    if (glewInit() != GLEW_OK) {
        OZ_ERROR("Failed to initialize GLEW");
        return NULL;
    }
#endif
    
    OzGLModern* renderer = calloc(1, sizeof(OzGLModern));
    if (!renderer) return NULL;
    
    // Create shader programs
    renderer->main_program = create_program(vertex_shader_src, fragment_shader_src);
    renderer->line_program = create_program(line_vertex_src, line_fragment_src);
    
    if (!renderer->main_program || !renderer->line_program) {
        oz_gl_modern_destroy(renderer);
        return NULL;
    }
    
    // Get uniform locations
    renderer->u_model = glGetUniformLocation(renderer->main_program, "uModel");
    renderer->u_view = glGetUniformLocation(renderer->main_program, "uView");
    renderer->u_projection = glGetUniformLocation(renderer->main_program, "uProjection");
    renderer->u_color = glGetUniformLocation(renderer->main_program, "uColor");
    renderer->u_ambient_light = glGetUniformLocation(renderer->main_program, "uAmbientLight");
    renderer->u_use_texture = glGetUniformLocation(renderer->main_program, "uUseTexture");
    
    renderer->u_mvp_line = glGetUniformLocation(renderer->line_program, "uMVP");
    renderer->u_color_line = glGetUniformLocation(renderer->line_program, "uColor");
    
    // Create VAOs and VBOs
    glGenVertexArrays(1, &renderer->grid_vao);
    glGenBuffers(1, &renderer->grid_vbo);
    
    glGenVertexArrays(1, &renderer->line_vao);
    glGenBuffers(1, &renderer->line_vbo);
    
    glGenVertexArrays(1, &renderer->geometry_vao);
    glGenBuffers(1, &renderer->geometry_vbo);
    glGenBuffers(1, &renderer->geometry_ebo);
    
    // Initialize geometry buffers
    renderer->vertex_capacity = 1024;
    renderer->index_capacity = 4096;
    renderer->vertex_buffer = malloc(renderer->vertex_capacity * sizeof(Vertex));
    renderer->index_buffer = malloc(renderer->index_capacity * sizeof(GLuint));
    renderer->vertex_count = 0;
    renderer->index_count = 0;
    
    // Initialize matrices
    matrix_identity(renderer->view_matrix);
    matrix_identity(renderer->projection_matrix);
    
    // Default lighting
    renderer->ambient_light = (OzVec3){0.7f, 0.7f, 0.75f};
    
    // Create checker texture
    create_checker_texture(renderer);
    
    OZ_INFO("Modern GL renderer initialized");
    return renderer;
}

void oz_gl_modern_destroy(OzGLModern* renderer) {
    if (!renderer) return;
    
    if (renderer->main_program) glDeleteProgram(renderer->main_program);
    if (renderer->line_program) glDeleteProgram(renderer->line_program);
    if (renderer->grid_vao) glDeleteVertexArrays(1, &renderer->grid_vao);
    if (renderer->grid_vbo) glDeleteBuffers(1, &renderer->grid_vbo);
    if (renderer->line_vao) glDeleteVertexArrays(1, &renderer->line_vao);
    if (renderer->line_vbo) glDeleteBuffers(1, &renderer->line_vbo);
    if (renderer->geometry_vao) glDeleteVertexArrays(1, &renderer->geometry_vao);
    if (renderer->geometry_vbo) glDeleteBuffers(1, &renderer->geometry_vbo);
    if (renderer->geometry_ebo) glDeleteBuffers(1, &renderer->geometry_ebo);
    if (renderer->checker_texture) glDeleteTextures(1, &renderer->checker_texture);
    
    free(renderer->vertex_buffer);
    free(renderer->index_buffer);
    free(renderer);
}

void oz_gl_modern_set_viewport(OzGLModern* renderer, int width, int height) {
    if (!renderer) return;
    
    glViewport(0, 0, width, height);
    
    float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    matrix_perspective(renderer->projection_matrix, 60.0f * (float)M_PI / 180.0f, aspect, 0.1f, 100.0f);
}

void oz_gl_modern_apply_camera(OzGLModern* renderer, const OzCamera* camera) {
    if (!renderer || !camera) return;
    
    matrix_look_at(renderer->view_matrix, camera);
}

void oz_gl_modern_set_ambient_light(OzGLModern* renderer, float r, float g, float b) {
    if (!renderer) return;
    renderer->ambient_light = (OzVec3){r, g, b};
}

void oz_gl_modern_clear_lights(OzGLModern* renderer) {
    // TODO: Implement point lights array
    (void)renderer;
}

void oz_gl_modern_add_point_light(OzGLModern* renderer, const OzVec3* pos, const OzVec3* color, float intensity, float range) {
    // TODO: Implement point lights array
    (void)renderer; (void)pos; (void)color; (void)intensity; (void)range;
}

void oz_gl_modern_clear(OzGLModern* renderer, float r, float g, float b, float a) {
    (void)renderer;
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void oz_gl_modern_draw_grid(OzGLModern* renderer, float extent, float step, const OzVec3* color) {
    if (!renderer) return;
    
    // Generate grid lines
    int num_lines = (int)(2 * extent / step) + 1;
    LineVertex* vertices = malloc(num_lines * 4 * sizeof(LineVertex));
    int vertex_count = 0;
    
    for (float v = -extent; v <= extent + 0.001f; v += step) {
        // Horizontal line
        vertices[vertex_count++] = (LineVertex){{-extent, v, 0.0f}};
        vertices[vertex_count++] = (LineVertex){{extent, v, 0.0f}};
        // Vertical line
        vertices[vertex_count++] = (LineVertex){{v, -extent, 0.0f}};
        vertices[vertex_count++] = (LineVertex){{v, extent, 0.0f}};
    }
    
    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(LineVertex), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    glUseProgram(renderer->line_program);
    
    // Calculate MVP matrix
    float mvp[16];
    memcpy(mvp, renderer->projection_matrix, 16 * sizeof(float));
    // Matrix multiply: mvp = projection * view (simplified for identity model matrix)
    // For now, just pass view matrix since model is identity
    
    glUniformMatrix4fv(renderer->u_mvp_line, 1, GL_FALSE, renderer->view_matrix);
    glUniform3f(renderer->u_color_line, color->x, color->y, color->z);
    
    glDrawArrays(GL_LINES, 0, vertex_count);
    
    free(vertices);
}

void oz_gl_modern_draw_axes(OzGLModern* renderer, float length) {
    if (!renderer) return;
    
    LineVertex vertices[] = {
        // X axis (red)
        {{0, 0, 0}}, {{length, 0, 0}},
        // Y axis (green)  
        {{0, 0, 0}}, {{0, length, 0}},
        // Z axis (blue)
        {{0, 0, 0}}, {{0, 0, length}}
    };
    
    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    glUseProgram(renderer->line_program);
    glUniformMatrix4fv(renderer->u_mvp_line, 1, GL_FALSE, renderer->view_matrix);
    
    // Draw each axis with different colors
    glUniform3f(renderer->u_color_line, 1.0f, 0.2f, 0.2f); // Red X
    glDrawArrays(GL_LINES, 0, 2);
    glUniform3f(renderer->u_color_line, 0.2f, 1.0f, 0.2f); // Green Y
    glDrawArrays(GL_LINES, 2, 2);
    glUniform3f(renderer->u_color_line, 0.3f, 0.6f, 1.0f); // Blue Z
    glDrawArrays(GL_LINES, 4, 2);
}

void oz_gl_modern_draw_map_filled(OzGLModern* renderer, const OzMap* map) {
    if (!renderer || !map) return;
    
    reset_geometry_buffers(renderer);
    
    // Generate geometry for all brushes
    for (size_t i = 0; i < map->count; ++i) {
        const OzBrush* brush = &map->brushes[i];
        if (brush->type == OZ_BRUSH_BOX) {
            generate_box_geometry(renderer, &brush->as.box);
        } else if (brush->type == OZ_BRUSH_CYLINDER) {
            generate_cylinder_geometry(renderer, &brush->as.cyl);
        }
        // TODO: Add sphere, pyramid, plane generation
    }
    
    // Flush all geometry in one draw call
    flush_geometry(renderer);
}

void oz_gl_modern_draw_map_wireframe(OzGLModern* renderer, const OzMap* map, int selected_index) {
    if (!renderer || !map) return;
    
    // Generate wireframe lines for all brushes
    for (size_t i = 0; i < map->count; ++i) {
        const OzBrush* brush = &map->brushes[i];
        OzVec3 color = ((int)i == selected_index) ? 
                      (OzVec3){1.0f, 0.2f, 0.2f} : (OzVec3){0.05f, 0.05f, 0.05f};
        
        if (brush->type == OZ_BRUSH_BOX) {
            draw_box_wireframe(renderer, &brush->as.box, &color);
        } else if (brush->type == OZ_BRUSH_CYLINDER) {
            draw_cylinder_wireframe(renderer, &brush->as.cyl, &color);
        }
    }
}

static void draw_box_wireframe(OzGLModern* renderer, const OzBrushBox* box, const OzVec3* color) {
    const float cx = box->center.x, cy = box->center.y, cz = box->center.z;
    const float hx = box->half.x, hy = box->half.y, hz = box->half.z;
    
    LineVertex vertices[24]; // 12 edges * 2 vertices each
    int idx = 0;
    
    // Define box vertices
    float v[8][3] = {
        {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz},
        {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
        {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz},
        {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz}
    };
    
    // Define edges
    int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, // bottom face
        {4,5}, {5,6}, {6,7}, {7,4}, // top face
        {0,4}, {1,5}, {2,6}, {3,7}  // vertical edges
    };
    
    for (int e = 0; e < 12; e++) {
        vertices[idx++] = (LineVertex){{v[edges[e][0]][0], v[edges[e][0]][1], v[edges[e][0]][2]}};
        vertices[idx++] = (LineVertex){{v[edges[e][1]][0], v[edges[e][1]][1], v[edges[e][1]][2]}};
    }
    
    // Upload and draw
    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    glUseProgram(renderer->line_program);
    
    // Calculate MVP matrix
    float mvp[16];
    matrix_multiply(renderer->projection_matrix, renderer->view_matrix, mvp);
    glUniformMatrix4fv(renderer->u_mvp_line, 1, GL_FALSE, mvp);
    glUniform3f(renderer->u_color_line, color->x, color->y, color->z);
    
    glDrawArrays(GL_LINES, 0, 24);
}

static void draw_cylinder_wireframe(OzGLModern* renderer, const OzBrushCylinder* cyl, const OzVec3* color) {
    int segments = cyl->segments > 3 ? cyl->segments : 16;
    float hz = cyl->height * 0.5f;
    
    // Generate wireframe lines: rim circles + vertical lines
    LineVertex* vertices = malloc((segments * 4 + segments * 2) * sizeof(LineVertex));
    int idx = 0;
    
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / (float)segments * 2.0f * (float)M_PI;
        float next_angle = (float)((i + 1) % segments) / (float)segments * 2.0f * (float)M_PI;
        
        float x1 = cyl->center.x + cosf(angle) * cyl->radius_x;
        float y1 = cyl->center.y + sinf(angle) * cyl->radius_y;
        float x2 = cyl->center.x + cosf(next_angle) * cyl->radius_x;
        float y2 = cyl->center.y + sinf(next_angle) * cyl->radius_y;
        
        // Bottom rim
        vertices[idx++] = (LineVertex){{x1, y1, cyl->center.z - hz}};
        vertices[idx++] = (LineVertex){{x2, y2, cyl->center.z - hz}};
        
        // Top rim
        vertices[idx++] = (LineVertex){{x1, y1, cyl->center.z + hz}};
        vertices[idx++] = (LineVertex){{x2, y2, cyl->center.z + hz}};
        
        // Vertical edge (every 4th segment to reduce clutter)
        if (i % 4 == 0) {
            vertices[idx++] = (LineVertex){{x1, y1, cyl->center.z - hz}};
            vertices[idx++] = (LineVertex){{x1, y1, cyl->center.z + hz}};
        }
    }
    
    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, idx * sizeof(LineVertex), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    glUseProgram(renderer->line_program);
    
    float mvp[16];
    matrix_multiply(renderer->projection_matrix, renderer->view_matrix, mvp);
    glUniformMatrix4fv(renderer->u_mvp_line, 1, GL_FALSE, mvp);
    glUniform3f(renderer->u_color_line, color->x, color->y, color->z);
    
    glDrawArrays(GL_LINES, 0, idx);
    
    free(vertices);
}

void oz_gl_modern_draw_gizmo(OzGLModern* renderer, const OzVec3* center, float scale, int highlight_axis) {
    if (!renderer || !center) return;
    
    LineVertex vertices[6] = {
        // X axis
        {{center->x, center->y, center->z}}, {{center->x + scale, center->y, center->z}},
        // Y axis
        {{center->x, center->y, center->z}}, {{center->x, center->y + scale, center->z}},
        // Z axis
        {{center->x, center->y, center->z}}, {{center->x, center->y, center->z + scale}}
    };
    
    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    glUseProgram(renderer->line_program);
    
    float mvp[16];
    matrix_multiply(renderer->projection_matrix, renderer->view_matrix, mvp);
    glUniformMatrix4fv(renderer->u_mvp_line, 1, GL_FALSE, mvp);
    
    glLineWidth(highlight_axis >= 0 ? 3.0f : 2.0f);
    
    // Draw each axis with appropriate color/highlighting
    OzVec3 colors[3] = {{1.0f, 0.2f, 0.2f}, {0.2f, 1.0f, 0.2f}, {0.3f, 0.6f, 1.0f}};
    for (int axis = 0; axis < 3; axis++) {
        OzVec3 color = colors[axis];
        if (highlight_axis == axis) {
            color.x *= 1.5f; color.y *= 1.5f; color.z *= 1.5f;
        }
        glUniform3f(renderer->u_color_line, color.x, color.y, color.z);
        glDrawArrays(GL_LINES, axis * 2, 2);
    }
    
    glLineWidth(1.0f);
}

void oz_gl_modern_draw_zone_circle(OzGLModern* renderer, const OzVec3* center, float radius, const OzVec3* color) {
    if (!renderer || !center) return;
    
    const int segments = 48;
    LineVertex vertices[segments * 2];
    
    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i / (float)segments * 2.0f * (float)M_PI;
        float angle2 = (float)((i + 1) % segments) / (float)segments * 2.0f * (float)M_PI;
        
        vertices[i*2] = (LineVertex){{
            center->x + cosf(angle1) * radius,
            center->y + sinf(angle1) * radius,
            center->z
        }};
        vertices[i*2+1] = (LineVertex){{
            center->x + cosf(angle2) * radius,
            center->y + sinf(angle2) * radius,
            center->z
        }};
    }
    
    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    glUseProgram(renderer->line_program);
    
    float mvp[16];
    matrix_multiply(renderer->projection_matrix, renderer->view_matrix, mvp);
    glUniformMatrix4fv(renderer->u_mvp_line, 1, GL_FALSE, mvp);
    glUniform3f(renderer->u_color_line, color->x, color->y, color->z);
    
    glDrawArrays(GL_LINES, 0, segments * 2);
}

void oz_gl_modern_draw_pickup_cube(OzGLModern* renderer, const OzVec3* center, float size, const OzVec3* color) {
    if (!renderer || !center) return;
    
    // Create a temporary box for the pickup cube
    OzBrushBox cube = {
        .center = *center,
        .half = {size, size, size},
        .rotation_z = 0
    };
    
    reset_geometry_buffers(renderer);
    generate_box_geometry(renderer, &cube);
    
    // Render with pickup color
    glUseProgram(renderer->main_program);
    
    float identity[16];
    matrix_identity(identity);
    glUniformMatrix4fv(renderer->u_model, 1, GL_FALSE, identity);
    glUniformMatrix4fv(renderer->u_view, 1, GL_FALSE, renderer->view_matrix);
    glUniformMatrix4fv(renderer->u_projection, 1, GL_FALSE, renderer->projection_matrix);
    
    glUniform3f(renderer->u_ambient_light, renderer->ambient_light.x, 
                renderer->ambient_light.y, renderer->ambient_light.z);
    glUniform1i(renderer->u_use_texture, 0);
    glUniform3f(renderer->u_color, color->x, color->y, color->z);
    
    glBindVertexArray(renderer->geometry_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->geometry_vbo);
    glBufferData(GL_ARRAY_BUFFER, renderer->vertex_count * sizeof(Vertex), 
                 renderer->vertex_buffer, GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->geometry_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, renderer->index_count * sizeof(GLuint),
                 renderer->index_buffer, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));
    glEnableVertexAttribArray(2);
    
    glDrawElements(GL_TRIANGLES, (GLsizei)renderer->index_count, GL_UNSIGNED_INT, 0);
}

void oz_gl_modern_draw_player_start(OzGLModern* renderer, const OzVec3* pos, float yaw, const OzVec3* color) {
    if (!renderer || !pos) return;
    
    // Draw arrow pointing in yaw direction
    float fx = cosf(yaw), fy = sinf(yaw);
    float len = 0.6f;
    float arrow_width = 0.2f;
    
    LineVertex vertices[6] = {
        // Main arrow line
        {{pos->x, pos->y, pos->z}}, 
        {{pos->x + fx * len, pos->y + fy * len, pos->z}},
        
        // Arrow head lines
        {{pos->x + fx * len, pos->y + fy * len, pos->z}},
        {{pos->x + fx * (len - 0.2f) - fy * arrow_width, pos->y + fy * (len - 0.2f) + fx * arrow_width, pos->z}},
        
        {{pos->x + fx * len, pos->y + fy * len, pos->z}},
        {{pos->x + fx * (len - 0.2f) + fy * arrow_width, pos->y + fy * (len - 0.2f) - fx * arrow_width, pos->z}}
    };
    
    glBindVertexArray(renderer->line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    glUseProgram(renderer->line_program);
    
    float mvp[16];
    matrix_multiply(renderer->projection_matrix, renderer->view_matrix, mvp);
    glUniformMatrix4fv(renderer->u_mvp_line, 1, GL_FALSE, mvp);
    glUniform3f(renderer->u_color_line, color->x, color->y, color->z);
    
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 6);
    glLineWidth(1.0f);
    
    // Draw small cube at base
    OzVec3 cube_color = {1.0f, 0.3f, 0.6f};
    oz_gl_modern_draw_pickup_cube(renderer, pos, 0.06f, &cube_color);
}
