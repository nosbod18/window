#include "wnd/wnd.h"

#define GLAD_GL_IMPLEMENTATION
#include "glad.h"

static struct {
    float x, y;
    float r, g, b;
} const vertices[] = {
    { -0.5f, -0.5f, 1.0f, 0.0f, 0.0f },
    {  0.5f, -0.5f, 0.0f, 1.0f, 0.0f },
    {  0.0f,  0.5f, 0.0f, 0.0f, 1.0f }
};

static char const *vsSource =
    "#version 330\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec3 aColor;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPosition, 0.0, 1.0);\n"
    "    vColor = vec4(aColor, 1.0);\n"
    "}";

static char const *fsSource =
    "#version 330\n"
    "in vec4 vColor;\n"
    "out vec4 fColor;\n"
    "void main() {\n"
    "    fColor = vColor;\n"
    "}";

int main(void) {
    WndWindow *window = WndCreateWindow(&(WndWindowDesc){0});
    WndMakeCurrent(window);
    gladLoadGL(WndGetProcAddress);

    unsigned vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    unsigned vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof *vertices, (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof *vertices, (void *)(2 * sizeof(float)));

    unsigned vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, 0);
    glCompileShader(vs);

    unsigned fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, 0);
    glCompileShader(fs);

    unsigned program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    while (!WndGetWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(vao);
        glUseProgram(program);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        WndSwapBuffers(window);
        WndPollEvents();
    }

    glDeleteProgram(program);
    glDeleteShader(fs);
    glDeleteShader(vs);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    WndDeleteWindow(window);
}
