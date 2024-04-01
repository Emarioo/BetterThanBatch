/*
    Random file used for compiling random things
*/

#include "glad/glad.h"
#include "libs/glfw-3.3.9/include/GLFW/glfw3.h"

int main(void) {
    int ok = glfwInit();
    if(!ok)
        return ok;
    // ok = gladLoadGL();

    return ok;
}
