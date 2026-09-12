#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader_m.h"
#include "camera.h"
#include "Sphere_light.h"
#include "Sphere.h"

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdlib>
using namespace std;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);
unsigned int loadCubemap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 4.0f, 14.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// lighting
glm::vec3 containerPos(5.0f, 0.0f, 0.0f);

// Skala waktu simulasi: 1 tahun = 60 detik, 1 hari = 15 detik (4 hari/tahun)
float yearLength = 60.0f;
float dayLength = 15.0f;
float orbitRadius = 5.0f;
float axialTilt = 23.4f;

// Kontrol simulasi (debug/demo): SPACE=pause, R=bekukan rotasi, O=bekukan orbit, UP/DOWN=kecepatan
bool paused = false;
bool freezeRotation = false;
bool freezeOrbit = false;
float timeScale = 1.0f;
float orbitTime = 0.0f;
float spinTime = 0.0f;
int debugMode = 0; // 0=penuh, 1=diffuse saja, 2=emission saja, 3=peta diff

void savePPM(const char* path, int width, int height)
{
    std::vector<unsigned char> px(width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << width << " " << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        f.write(reinterpret_cast<char*>(px.data() + y * width * 3), width * 3);
}

int main(int argc, char** argv)
{
    // CLI debug: --capture <frame> <file.ppm> [--debug <0-3>]
    int captureAt = -1;
    std::string capturePath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--capture" && i + 2 < argc) { captureAt = std::atoi(argv[++i]); capturePath = argv[++i]; }
        else if (a == "--debug" && i + 1 < argc) { debugMode = std::atoi(argv[++i]); }
        else if (a == "--orbit-time" && i + 1 < argc) { orbitTime = (float)std::atof(argv[++i]); freezeOrbit = true; }
        else if (a == "--spin-time" && i + 1 < argc) { spinTime = (float)std::atof(argv[++i]); freezeRotation = true; }
    }
    int frameIndex = 0;
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader zprogram
    // ------------------------------------
    Shader objectShader("shaders/object.vs", "shaders/object_emission.fs");
    Shader sunShader("shaders/light_source.vs", "shaders/light_source.fs");
    Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");

    // set up vertex data (and buffer(s)) and configure vertex attributes
    Sphere_light Sun(3.0f, 36 * 5, 18 * 5);
    Sphere Earth(1.0f, 36, 18);

    // load textures (we now use a utility function to keep the code more organized)
    // -----------------------------------------------------------------------------
    unsigned int earth_sm = loadTexture("resources/planets/2k_earth_specular_map.jpg");
    unsigned int earth_em = loadTexture("resources/planets/2k_earth_nightmap.jpg");
    unsigned int earth_tex = loadTexture("resources/planets/2k_earth_daymap.jpg");
    unsigned int sun_tex = loadTexture("resources/planets/2k_sun.jpg");

    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    // skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    std::vector<std::string> faces{
        "resources/skybox/starfield/starfield_rt.tga",
        "resources/skybox/starfield/starfield_lf.tga",
        "resources/skybox/starfield/starfield_up.tga",
        "resources/skybox/starfield/starfield_dn.tga",
        "resources/skybox/starfield/starfield_ft.tga",
        "resources/skybox/starfield/starfield_bk.tga",
    };

    unsigned int cubemapTexture = loadCubemap(faces);

    // shader configuration
    // --------------------
    objectShader.use();
    objectShader.setInt("material.diffuse", 0);
    objectShader.setInt("material.specular", 1);
    objectShader.setInt("material.emission", 2);

    sunShader.use();
    sunShader.setInt("texture1", 0);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // render loop
    // -----------
    std::cout << "Kontrol: WASD+mouse=kamera, SPACE=pause, R=bekukan rotasi, O=bekukan orbit, UP/DOWN=kecepatan, 1-4=mode cahaya" << std::endl;
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ------ EARTH ------
        // Waktu simulasi (terpengaruh pause / timeScale / freeze)
        if (!paused) {
            if (!freezeOrbit) orbitTime += deltaTime * timeScale;
            if (!freezeRotation) spinTime += deltaTime * timeScale;
        }

        // Revolusi: 1 orbit penuh per yearLength detik (prograde, berlawanan jarum jam dari kutub utara)
        float orbitAngle = 2.0f * 3.14159265f * orbitTime / yearLength;
        containerPos.x = orbitRadius * cos(orbitAngle);
        containerPos.z = -orbitRadius * sin(orbitAngle);
        containerPos.y = 0.0f;


        // be sure to activate shader when setting uniforms/drawing objects
        objectShader.use();
        objectShader.setInt("debugMode", debugMode);
        objectShader.setVec3("light.position", glm::vec3 (0.0f, 0.0f, 0.0f));
        objectShader.setVec3("viewPos", camera.Position);

        // light properties
        objectShader.setVec3("light.ambient", 0.1f, 0.1f, 0.1f);
        objectShader.setVec3("light.diffuse", 0.7f, 0.7f, 0.7f);
        objectShader.setVec3("light.specular", 0.5f, 0.5f, 0.5f);

        // material properties
        objectShader.setFloat("material.shininess", 64.0f);

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        objectShader.setMat4("projection", projection);
        objectShader.setMat4("view", view);

        glm::mat4 model_earth = glm::mat4(1.0f);
        model_earth = glm::translate(model_earth, containerPos);
        model_earth = glm::rotate(model_earth, glm::radians(axialTilt), glm::vec3(0.0f, 0.0f, 1.0f)); // Kemiringan sumbu 23.4 derajat
        model_earth = glm::rotate(model_earth, 2.0f * 3.14159265f * spinTime / dayLength, glm::vec3(0.0f, 1.0f, 0.0f)); // Rotasi harian prograde
        objectShader.setMat4("model", model_earth);

        // bind diffuse map
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, earth_tex);
        // bind specular map
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, earth_sm);
        // bind emission map
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, earth_em);

        // render the earth
        Earth.Draw();
        // ------ EARTH ------
        

        // ------ SUN -------
        sunShader.use();
        sunShader.setMat4("projection", projection);
        sunShader.setMat4("view", view);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sun_tex);
        
        glm::mat4 model_sun = glm::mat4(1.0f); // Matahari diam di pusat sebagai sumber cahaya
        sunShader.setMat4("model", model_sun);

        Sun.Draw();
        // ------ SUN -------


        // draw skybox as last
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default   

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        // capture framebuffer ke PPM lalu keluar (untuk analisis/laporan)
        if (captureAt > 0 && ++frameIndex >= captureAt) {
            savePPM(capturePath.c_str(), SCR_WIDTH, SCR_HEIGHT);
            std::cout << "[CAPTURE] frame " << frameIndex << " -> " << capturePath << std::endl;
            glfwSetWindowShouldClose(window, true);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    // Kontrol simulasi (edge-triggered: sekali tekan = sekali toggle)
    static bool spaceHeld = false, rHeld = false, oHeld = false;
    static bool numHeld[4] = { false, false, false, false };
    bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool r = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool o = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (space && !spaceHeld) { paused = !paused; std::cout << (paused ? "[SIM] paused" : "[SIM] running") << std::endl; }
    if (r && !rHeld) { freezeRotation = !freezeRotation; std::cout << "[SIM] rotasi " << (freezeRotation ? "BEKU" : "jalan") << std::endl; }
    if (o && !oHeld) { freezeOrbit = !freezeOrbit; std::cout << "[SIM] orbit " << (freezeOrbit ? "BEKU" : "jalan") << std::endl; }
    spaceHeld = space; rHeld = r; oHeld = o;

    // Tombol 1-4: mode tampilan cahaya (0=penuh, 1=diffuse, 2=emission, 3=peta diff)
    const int numKeys[4] = { GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4 };
    for (int i = 0; i < 4; ++i) {
        bool k = glfwGetKey(window, numKeys[i]) == GLFW_PRESS;
        if (k && !numHeld[i]) { debugMode = i; std::cout << "[SIM] debugMode=" << i << std::endl; }
        numHeld[i] = k;
    }

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        timeScale += deltaTime * 2.0f;
        if (timeScale > 4.0f) timeScale = 4.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        timeScale -= deltaTime * 2.0f;
        if (timeScale < 0.1f) timeScale = 0.1f;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    // Minta 3 channel (RGB): tekstur grayscale 1-channel ikut mengembang,
    // supaya tidak ter-upload sebagai GL_RED yang terbaca sebagai (R,0,0) alias merah
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 3);
    nrComponents = 3;
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front) 
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}