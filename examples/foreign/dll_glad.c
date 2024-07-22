/*
    NOTE: Make sure glad.dll exists at the root of the project
    gcc -Ilibs/glad/include -Ilibs/glfw-3.3.9/include examples/foreign/dll_glad.c -c -o wa.o
    gcc wa.o glad.dll libs/glfw-3.3.9/lib-mingw-w64/glfw3.lib -lopengl32 -lgdi32 -o wa.exe
*/

#define GLAD_GLAPI_EXPORT
#include "glad/glad.h"
#include "GLFW/glfw3.h"

int main() {
    glfwInit();

    GLFWwindow* win = glfwCreateWindow(800,600,"title",0,0);

    glfwMakeContextCurrent(win);

    gladLoadGL();
    glViewport(0,0,0,0);

    return 0;
}