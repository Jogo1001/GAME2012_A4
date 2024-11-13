#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Mesh.h"

// TODO -- Texture.h & Texture.cpp during lab, show result next lexture
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>

constexpr int SCREEN_WIDTH = 1600;
constexpr int SCREEN_HEIGHT = 800;
constexpr float SCREEN_ASPECT = SCREEN_WIDTH / (float)SCREEN_HEIGHT;

void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void error_callback(int error, const char* description);

GLuint CreateShader(GLint type, const char* path);
GLuint CreateProgram(GLuint vs, GLuint fs);

std::array<int, GLFW_KEY_LAST> gKeysCurr{}, gKeysPrev{};
bool IsKeyDown(int key);
bool IsKeyUp(int key);
bool IsKeyPressed(int key);

void Print(Matrix m);

enum Projection : int
{
    ORTHO,  // Orthographic, 2D
    PERSP   // Perspective,  3D
};

struct Pixel
{
    stbi_uc r = 255;
    stbi_uc g = 255;
    stbi_uc b = 255;
    stbi_uc a = 255;
};

int main(void)
{
    glfwSetErrorCallback(error_callback);
    assert(glfwInit() == GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
#ifdef NDEBUG
#else
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Graphics 1", NULL, NULL);
    glfwMakeContextCurrent(window);
    assert(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress));
    glfwSetKeyCallback(window, key_callback);

#ifdef NDEBUG
#else
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glDebugOutput, nullptr);
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Vertex shaders:
    GLuint vs = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/default.vert");
    GLuint vsPoints = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/points.vert");
    GLuint vsLines = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/lines.vert");
    GLuint vsVertexPositionColor = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/vertex_color.vert");
    GLuint vsColorBufferColor = CreateShader(GL_VERTEX_SHADER, "./assets/shaders/buffer_color.vert");
    
    // Fragment shaders:
    GLuint fsLines = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/lines.frag");
    GLuint fsUniformColor = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/uniform_color.frag");
    GLuint fsVertexColor = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/vertex_color.frag");
    GLuint fsTcoords = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/tcoord_color.frag");
    GLuint fsNormals = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/normal_color.frag");
    GLuint fsTexture = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/texture_color.frag");
    GLuint fsTextureMix = CreateShader(GL_FRAGMENT_SHADER, "./assets/shaders/texture_color_mix.frag");
    
    // Shader programs:
    GLuint shaderUniformColor = CreateProgram(vs, fsUniformColor);
    GLuint shaderVertexPositionColor = CreateProgram(vsVertexPositionColor, fsVertexColor);
    GLuint shaderVertexBufferColor = CreateProgram(vsColorBufferColor, fsVertexColor);
    GLuint shaderPoints = CreateProgram(vsPoints, fsVertexColor);
    GLuint shaderLines = CreateProgram(vsLines, fsLines);
    GLuint shaderTcoords = CreateProgram(vs, fsTcoords);
    GLuint shaderNormals = CreateProgram(vs, fsNormals);
    GLuint shaderTexture = CreateProgram(vs, fsTexture);
    GLuint shaderTextureMix = CreateProgram(vs, fsTextureMix);

    // Our obj file defines tcoords as 0 = bottom, 1 = top, but OpenGL defines as 0 = top 1 = bottom.
    // Flipping our image vertically is the best way to solve this as it ensures a "one-stop" solution (rather than an in-shader solution).
    stbi_set_flip_vertically_on_load(true);

    // Note that generating an image ourselves is intuitive in OpenGL's space since it matches 2D array coordinates!
    int texGradientWidth = 256;
    int texGradientHeight = 256;
    Pixel* pixelsGradient = (Pixel*)malloc(texGradientWidth * texGradientHeight * sizeof(Pixel));
    for (int y = 0; y < texGradientHeight; y++)
    {
        for (int x = 0; x < texGradientWidth; x++)
        {
            float u = x / (float)texGradientWidth;
            float v = y / (float)texGradientHeight;

            Pixel pixel;
            pixel.r = u * 255.0f;
            pixel.g = v * 255.0f;
            pixel.b = 255;
            pixel.a = 255;

            pixelsGradient[y * texGradientWidth + x] = pixel;
        }
    }

    // Car
    int CarHeadWidth = 0;
    int CarHeadHeight = 0;
    int CarHeadChannels = 0;
    stbi_uc* pixelsHead = stbi_load("./assets/textures/ct4_grey.png", &CarHeadWidth, &CarHeadHeight, &CarHeadChannels, 0);


    GLuint texHead = GL_NONE;
    glGenTextures(1, &texHead);
    glBindTexture(GL_TEXTURE_2D, texHead);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CarHeadWidth, CarHeadHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsHead);
    stbi_image_free(pixelsHead);
    pixelsHead = nullptr;

   // Trex
    int HeadHeadWidth = 0;
    int HeadHeadHeight = 0;
    int HeadHeadChannels = 0;
    stbi_uc* HeadHead = stbi_load("./assets/textures/head.png", &HeadHeadWidth, &HeadHeadHeight, &HeadHeadChannels, 0);

    GLuint HeadsHead = GL_NONE;
    glGenTextures(1, &HeadsHead);
    glBindTexture(GL_TEXTURE_2D, HeadsHead);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HeadHeadWidth, HeadHeadHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, HeadHead);
    stbi_image_free(HeadHead);
    HeadHead = nullptr;


    //////////////



    GLuint texGradient = GL_NONE;
    glGenTextures(1, &texGradient);
    glBindTexture(GL_TEXTURE_2D, texGradient);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texGradientWidth, texGradientHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsGradient);
    free(pixelsGradient);
    pixelsGradient = nullptr;

    int object = 0;
    printf("Object %i\n", object + 1);

    Projection projection = PERSP;
    Vector3 camPos{ 0.0f, 0.0f, 5.0f };
    float fov = 75.0f * DEG2RAD;
    float left = -1.0f;
    float right = 1.0f;
    float top = 1.0f;
    float bottom = -1.0f;
    float near = 1.0f; // 1.0 for testing purposes. Usually 0.1f or 0.01f
    float far = 10.0f;

    // Whether we render the imgui demo widgets
    bool imguiDemo = false;
    bool texToggle = false;
    bool camToggle = false;


    Mesh shapeMesh, CarMesh, TrexMesh,shoesMesh, HeadMesh, GunMesh;
    CreateMesh(&shapeMesh, CUBE);
    CreateMesh(&CarMesh, "assets/meshes/ct4.obj");
    CreateMesh(&TrexMesh, "assets/meshes/dynotex.obj");
    CreateMesh(&shoesMesh, "assets/meshes/shoes.obj");
    CreateMesh(&HeadMesh, "assets/meshes/head.obj");
    CreateMesh(&GunMesh, "assets/meshes/Gun.obj");
  


    float objectPitch = 0.0f;
    float objectYaw = 0.0f;
    Vector3 objectPosition = V3_ZERO;
    float objectSpeed = 10.0f;

    // Render looks weird cause this isn't enabled, but its causing unexpected problems which I'll fix soon!
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    float timePrev = glfwGetTime();
    float timeCurr = glfwGetTime();
    float dt = 0.0f;

    double pmx = 0.0, pmy = 0.0, mx = 0.0, my = 0.0;
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        float time = glfwGetTime();
        timePrev = time;

        pmx = mx; pmy = my;
        glfwGetCursorPos(window, &mx, &my);
        Vector2 mouseDelta = { mx - pmx, my - pmy };

        // Change object when space is pressed
        if (IsKeyPressed(GLFW_KEY_TAB))
        {
            ++object %= 5;
            printf("Object %i\n", object + 1);
        }

        if (IsKeyPressed(GLFW_KEY_I))
            imguiDemo = !imguiDemo;

        if (IsKeyPressed(GLFW_KEY_T))
            texToggle = !texToggle;

        
            camToggle = !camToggle;
            if (camToggle)
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else
            {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        

        Matrix objectRotation = ToMatrix(FromEuler(objectPitch * DEG2RAD, objectYaw * DEG2RAD, 0.0f));
        Matrix objectTranslation = Translate(objectPosition);

        Vector3 objectRight = { objectRotation.m0, objectRotation.m1, objectRotation.m2 };
        Vector3 objectUp = { objectRotation.m4, objectRotation.m5, objectRotation.m6 };
        Vector3 objectForward = { objectRotation.m8, objectRotation.m9, objectRotation.m10 };
        Matrix objectMatrix = objectRotation * objectTranslation;

        float objectDelta = objectSpeed * dt;
        float mouseScale = 1.0f;
        if (!camToggle)
        {
            objectDelta = 0.0f;
            mouseScale = 0.0f;
        }
        objectPitch += mouseDelta.y * mouseScale;
        objectYaw += mouseDelta.x * mouseScale;
        if (IsKeyDown(GLFW_KEY_W))
        {
            objectPosition += objectForward * objectDelta;
;       }
        if (IsKeyDown(GLFW_KEY_S))
        {
            objectPosition -= objectForward * objectDelta;
        }
        if (IsKeyDown(GLFW_KEY_A))
        {
            objectPosition -= objectRight * objectDelta;
        }
        if (IsKeyDown(GLFW_KEY_D))
        {
            objectPosition += objectRight * objectDelta;
        }
        if (IsKeyDown(GLFW_KEY_LEFT_SHIFT))
        {
            objectPosition -= objectUp * objectDelta;
        }
        if (IsKeyDown(GLFW_KEY_SPACE))
        {
            objectPosition += objectUp * objectDelta;
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // view-matrix is the *inverse* of camera matrix
        // ex: when we move the camera left, objects move right
        // ex: when we move the camera up, object move down
        // when we rotate the camera left, objects should move right
        // when we rotate the camera up, object should move down
        Matrix world = MatrixIdentity();
        Matrix view = LookAt(camPos, camPos - V3_FORWARD, V3_UP);
        Matrix proj = projection == ORTHO ? Ortho(left, right, bottom, top, near, far) : Perspective(fov, SCREEN_ASPECT, near, far);
        Matrix mvp = MatrixIdentity();
        GLint u_mvp = GL_NONE;
        GLint u_tex = GL_NONE;
        GLint u_textureSlots[2];
        GLuint shaderProgram = GL_NONE;
        GLuint texture = texToggle ? texGradient : texHead;
        GLuint Head = texToggle ? texGradient : HeadsHead;
        GLint u_t = GL_NONE;

        Matrix rotationX = RotateX(100.0f * time * DEG2RAD);
        Matrix rotationY = RotateY(100.0f * time * DEG2RAD);

        Matrix negrotationY = RotateY(-100.0f * time * DEG2RAD);

        

        switch (object + 1)
        {
        // Left side: object with texture applied to it
        // Right side: the coordinates our object uses to sample its texture
        case 1:

            shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            //view = view * rotationY;
            //view = rotationY * view;
            world = objectMatrix;//rotationY * Translate(-2.5f, 0.0f, 0.0f);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            DrawMesh(CarMesh);


            shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            //view = view * rotationY;
            //view = rotationY * view;
            world = rotationY * Translate(-2.5f, 0.0f, 0.0f);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            DrawMesh(TrexMesh);


            shaderProgram = shaderTcoords;
            glUseProgram(shaderProgram);
            //view = view * rotationY;
            //view = rotationY * view;
            world = negrotationY * Translate(2.5f, 0.0f, 0.0f);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            DrawMesh(shoesMesh);



            shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            //view = view * rotationY;
            //view = rotationY * view;
            world = Translate(2.5f, -3.0f, 0.0f);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, Head);
            DrawMesh(HeadMesh);


            shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            //view = view * rotationY;
            //view = rotationY * view;
            world = Translate(-2.5f, -3.0f, 0.0f);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            DrawMesh(GunMesh);








 /*           shaderProgram = shaderTcoords;
            glUseProgram(shaderProgram);
            world = rotationX * Translate(2.5f, 0.0f, 0.0f);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            DrawMesh(objMesh);*/
            break;


        // Interpolating (lerping) between 2 textures:
        case 2:
    /*        shaderProgram = shaderTextureMix;
            mvp = world * view * proj;

            glUseProgram(shaderProgram);
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_textureSlots[0] = glGetUniformLocation(shaderProgram, "u_tex0");
            u_textureSlots[1] = glGetUniformLocation(shaderProgram, "u_tex1");
            u_t = glGetUniformLocation(shaderProgram, "u_t");

            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1f(u_t, cosf(time) * 0.5f + 0.5f);

            glUniform1i(u_textureSlots[0], 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texGradient);

            glUniform1i(u_textureSlots[1], 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texHead);

            DrawMesh(TrexMesh);*/
            break;

        // Texture coordinates of our object
        case 3:
   /*         shaderProgram = shaderTcoords;
            glUseProgram(shaderProgram);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            DrawMesh(TrexMesh);*/
            break;

        // Normals of our object
        case 4:
      /*      shaderProgram = shaderNormals;
            glUseProgram(shaderProgram);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            DrawMesh(TrexMesh);*/
            break;

        // Applies a texture to our object
        case 5:
        /*    shaderProgram = shaderTexture;
            glUseProgram(shaderProgram);
            mvp = world * view * proj;
            u_mvp = glGetUniformLocation(shaderProgram, "u_mvp");
            u_tex = glGetUniformLocation(shaderProgram, "u_tex");
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, ToFloat16(mvp).v);
            glUniform1i(u_tex, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            DrawMesh(TrexMesh);*/
            break;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (imguiDemo)
            ImGui::ShowDemoWindow();
        else
        {
            ImGui::SliderFloat3("Camera Position", &camPos.x, -10.0f, 10.0f);

            ImGui::RadioButton("Orthographic", (int*)&projection, 0); ImGui::SameLine();
            ImGui::RadioButton("Perspective", (int*)&projection, 1);

            ImGui::SliderFloat("Near", &near, -10.0f, 10.0f);
            ImGui::SliderFloat("Far", &far, -10.0f, 10.0f);
            if (projection == ORTHO)
            {
                ImGui::SliderFloat("Left", &left, -1.0f, -10.0f);
                ImGui::SliderFloat("Right", &right, 1.0f, 10.0f);
                ImGui::SliderFloat("Top", &top, 1.0f, 10.0f);
                ImGui::SliderFloat("Bottom", &bottom, -1.0f, -10.0f);
            }
            else if (projection == PERSP)
            {
                ImGui::SliderAngle("FoV", &fov, 10.0f, 90.0f);
            }
        }
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        timeCurr = glfwGetTime();
        dt = timeCurr - timePrev;

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll and process events */
        memcpy(gKeysPrev.data(), gKeysCurr.data(), GLFW_KEY_LAST * sizeof(int));
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Disable repeat events so keys are either up or down
    if (action == GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    gKeysCurr[key] = action;

    // Key logging
    //const char* name = glfwGetKeyName(key, scancode);
    //if (action == GLFW_PRESS)
    //    printf("%s\n", name);
}

void error_callback(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}

// Compile a shader
GLuint CreateShader(GLint type, const char* path)
{
    GLuint shader = GL_NONE;
    try
    {
        // Load text file
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(path);

        // Interpret the file as a giant string
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();

        // Verify shader type matches shader file extension
        const char* ext = strrchr(path, '.');
        switch (type)
        {
        case GL_VERTEX_SHADER:
            assert(strcmp(ext, ".vert") == 0);
            break;

        case GL_FRAGMENT_SHADER:
            assert(strcmp(ext, ".frag") == 0);
            break;
        default:
            assert(false, "Invalid shader type");
            break;
        }

        // Compile text as a shader
        std::string str = stream.str();
        const char* src = str.c_str();
        shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);

        // Check for compilation errors
        GLint success;
        GLchar infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cout << "Shader failed to compile: \n" << infoLog << std::endl;
        }
    }
    catch (std::ifstream::failure& e)
    {
        std::cout << "Shader (" << path << ") not found: " << e.what() << std::endl;
        assert(false);
    }

    return shader;
}

// Combine two compiled shaders into a program that can run on the GPU
GLuint CreateProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        program = GL_NONE;
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    return program;
}

// Graphics debug callback
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam)
{
    // ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (source)
    {
    case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
    } std::cout << std::endl;

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
    } std::cout << std::endl;

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    } std::cout << std::endl;
    std::cout << std::endl;
}

bool IsKeyDown(int key)
{
    return gKeysCurr[key] == GLFW_PRESS;
}

bool IsKeyUp(int key)
{
    return gKeysCurr[key] == GLFW_RELEASE;
}

bool IsKeyPressed(int key)
{
    return gKeysPrev[key] == GLFW_PRESS && gKeysCurr[key] == GLFW_RELEASE;
}

void Print(Matrix m)
{
    printf("%f %f %f %f\n", m.m0, m.m4, m.m8, m.m12);
    printf("%f %f %f %f\n", m.m1, m.m5, m.m9, m.m13);
    printf("%f %f %f %f\n", m.m2, m.m6, m.m10, m.m14);
    printf("%f %f %f %f\n\n", m.m3, m.m7, m.m11, m.m15);
}


