// #define GLEW_STATIC
// #include "GL/glew.h"
// #include "GLFW/glfw3.h"

// #include "Engone/PlatformLayer.h"

// void TestWindow(){
// // #define GL_CHECK glErrCode = glGetError() if glErrCode { log("Error",glErrCode,":",glewGetErrorString(glErrCode)); }
// #define GL_CHECK
//     int yes = glfwInit();
//     if (!yes) {
//         // log("glfwInit failed");
//         return;
//     }
//     // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//     // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//     // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//     auto title = "Yes sir";
//     auto width = 800;
//     auto height = 600;
//     auto window = glfwCreateWindow(width, height, title, nullptr, nullptr);
//     // window = glfwCreateWindow(0, 0, null, null, null);
//     // window = null
//     // if !window {
//         // log("window failed");
//         // glfwTerminate();
//         // return;
//     // }

//     glfwMakeContextCurrent(window);
//     // glfwMakeContextCurrent(null);
//     // faulty(window)
//     // log("oki")

//     // There seems to be a strange bug in the compiler here.
//     // It crashes strangely when uncommenting certain lines.
//     // perhaps a bug in the if statement?
//     // Strip away as much code as possible
//     // Check the instructions see if you can find a bug

//     // Yeah I can't find it. I have no idea what's wrong.
//     // Try doing the same in C++.

//     int err = glewInit();
//     // msg = glewGetErrorString(err);
//     if (err) {
//         // msg = 0
//         // faulty(err);
//         // msg = glewGetErrorString(err);
//         // log("glew failed: ",msg)
//         // glfwTerminate();
//         return;
//     }

//     // GL_CHECK
//     glViewport(0, 0, width, height);
//     // GL_CHECK
//     // log("oi")

//     // log("glew version:", glewGetString(GLEW_VERSION));

//     // VBO: u32;
//     u32 VBO;
//     // ptr: u32* = Allocate(sizeof(u32));
//     // *ptr = 0;
//     // __glewGenBuffers(1,&VBO);
//     // __glewGenBuffers(1,ptr);
//     glGenBuffers(1, &VBO);

//     int a = 0;
//     int dir = 1;
//     while (!glfwWindowShouldClose(window)) {
//         if (a > 400)
//             dir = -1;
//         if (a < 0)
//             dir = 1;
//         a += dir;
//         glClearColor(a / 400.0f,0.3,0.5,1);
        
//         glClear(GL_COLOR_BUFFER_BIT);

//         glfwSwapBuffers(window);

//         glfwPollEvents();
//         engone::Sleep(0.001);
//     }

//     glfwTerminate();
// }