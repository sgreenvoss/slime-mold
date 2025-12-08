#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS
#define GLM_ENABLE_EXPERIMENTAL

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <array>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>
#include <omp.h>

#include <fstream>
#include <sstream>
#include "json.hpp"
#include "compute-helper.h"

using json = nlohmann::json;
using namespace std;

#include <glm/vec3.hpp>   // glm::vec3
#include <glm/vec2.hpp>


const unsigned int REF_PER_FRAME = 1000;
//const string world_type = "ventricles";
const string current_world = "six_seven_rose_circles";
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
// green_city was 60000

glm::vec2 generateRandomPointInUnitCircle(std::mt19937 generator) {

    // Uniform distribution for angle (0 to 2*PI)
    std::uniform_real_distribution<> angle_dist(0.0, 2.0 * M_PI);
    // Uniform distribution for radius squared (0 to 1) to ensure uniform point distribution
    std::uniform_real_distribution<> radius_sq_dist(0.0, 1.0);

    // Generate random angle and radius
    double angle = angle_dist(generator);
    double radius = std::sqrt(radius_sq_dist(generator)); // Take square root for uniform distribution

    // Convert polar coordinates to Cartesian coordinates
    glm::vec2 p;
    p.x = radius * std::cos(angle);
    p.y = radius * std::sin(angle);

    return p;
}


string LoadShaderFile(const char* filePath) {
    ifstream file(filePath, ios::in | ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to open shader file: " << filePath << endl;
        exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    string src = buffer.str();
    return src;
}


class World {
public:
    int width, height;
    json world_config;


    World(json &conf) {
        world_config = conf;
        // assuming world congfigs rather than worlds here
        width = conf["world_configs"][current_world]["width"];
        height = conf["world_configs"][current_world]["height"];
       // cout << "running here" << endl;

    }

};

void _print_shader_info_log(GLuint shader_index) {
  int max_length = 2048;
  int actual_length = 0;
  char shader_log[2048];
  glGetShaderInfoLog(shader_index, max_length, &actual_length, shader_log);
  printf("shader info log for GL index %u:\n%s\n", shader_index, shader_log);
}


struct smallPart {
    glm::ivec2 pos;
    float heading;
    int species_id;
};

struct ParticleSettings {
    int move_dist;
    int sensor_dist;
    float rotation_angle;
    float sensor_rotation;
    glm::vec4 color;
    int pref;
};

int main() 
{
    // TODO fix
    int numSlimes = 670000;

    if (!glfwInit()) {
        fprintf(stderr, "ERROR: could not start GLFW3\n");
        exit(EXIT_FAILURE);
    }

    glm::ivec2 windowDims = glm::ivec2(1400, 1400);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    std::ifstream f("configs.json");
    json config = json::parse(f);
    World w(config);

    GLFWwindow* window = glfwCreateWindow(windowDims.x, windowDims.y, "sliiime", NULL, NULL);
    if (!window) {
        fprintf(stderr, "ERROR: could not open window with GLFW3\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glewExperimental = GL_TRUE;
    glewInit();


    // -----------------------------------shaders-----------------------------------
    const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString(GL_VERSION);   // version as a string
    printf("Renderer: %s\n", renderer);
    printf("OpenGL version supported %s\n", version);

    // tell GL to only draw onto a pixel if the shape is closer to the viewer
    glEnable(GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"

    string v_src = LoadShaderFile("vert.glsl");
    string f_src = LoadShaderFile("frag.glsl");

    unsigned int v_id, f_id;
    v_id = glCreateShader(GL_VERTEX_SHADER);
    const char* src = v_src.c_str();
    glShaderSource(v_id, 1, &src, NULL);
    glCompileShader(v_id);
    int params = -1;
    glGetShaderiv(v_id, GL_COMPILE_STATUS, &params);
    if (GL_TRUE != params) {
        fprintf(stderr, "ERROR: GL shader index %i did not compile\n", v_id);
        _print_shader_info_log(v_id);
        exit(EXIT_FAILURE);
    }

    src = f_src.c_str();
    f_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_id, 1, &src, NULL);
    glCompileShader(f_id);
    glGetShaderiv(f_id, GL_COMPILE_STATUS, &params);
    if (GL_TRUE != params) {
        fprintf(stderr, "ERROR: GL shader index %i did not compile\n", f_id);
        _print_shader_info_log(f_id);
        exit(EXIT_FAILURE);
    }

    GLuint shaderProgramID = glCreateProgram();
    glAttachShader(shaderProgramID, v_id);
    glAttachShader(shaderProgramID, f_id);
    glLinkProgram(shaderProgramID);
    GLint success;
    glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(shaderProgramID, 1024, NULL, infoLog);
        fprintf(stderr, "ERROR: Shader Program Link Failed:\n%s\n", infoLog);
        exit(EXIT_FAILURE);
    }
    glUseProgram(shaderProgramID);
    glDeleteShader(v_id); glDeleteShader(f_id);

    // end shader zone ------------------------------------------------------------

    vector<ParticleSettings> settings;
    vector<smallPart> slimes;
    uniform_int_distribution<> distribx(0, w.width - 1);
    uniform_int_distribution<> distriby(0, w.height - 1);
    uniform_real_distribution<> angleDist((float)0.0, (float)2.0 * 3.141593);
    mt19937 gen(random_device{}());

    int slimed = 0;
    int idx = 0;
    // TODO world_now should be held by the world
    float screen_radius = std::min((float)w.width, (float)w.height) / 2.0f;
    glm::vec2 screen_center = glm::vec2((float)w.width / 2.0f, (float)w.height / 2.0f);
    cout << screen_center.x << " " << screen_center.y << endl;

    for (auto& kvp : config.at("world_configs").at(current_world).at("slime_types").items()) {
        float slime_ratio = kvp.value();

        int s_count = (int)(slime_ratio * (float) numSlimes);
        json slime_conf = config.at("slime_configs").at(kvp.key());

        float r = slime_conf.at("color")[0];
        float g = slime_conf.at("color")[1];
        float b = slime_conf.at("color")[2];
        glm::vec4 col = glm::vec4(r, g, b, 1);

        settings.push_back({ slime_conf["move_dist"],
            slime_conf["sensor_dist"],
            slime_conf["sensor_rotation"],
            slime_conf["rotation_angle"],
            col,
            slime_conf["pref"]
            }
        );

        int start = slimed;
        int end = start + s_count;
        for (int i = start; i < end; i++) {
            glm::vec2 p_i = screen_center + (generateRandomPointInUnitCircle(gen) * (float)w.height * 0.5f);
            
           // p_i += glm::vec2((float)width / 2, (float)height / 2);
            glm::ivec2 pos = glm::ivec2(round(p_i.x), round(p_i.y));
            //glm::ivec2 pos = glm::ivec2(distribx(gen), distriby(gen));
            //cout << pos.x << " " << pos.y << endl;
            slimes.push_back(smallPart{ pos, (float)angleDist(gen), idx });
        }
        cout << "number of slimes: " << slimes.size() << endl;
        idx++;
    }
    //numSlimes = slimes.size();

    float vertices[] = {
         1.0f,  1.0f, 1.0f, 1.0f,   // top right
         1.0f, -1.0f, 1.0f, 0.0f,   // bottom right
        -1.0f, -1.0f, 0.0f, 0.0f,   // bottom left
        -1.0f,  1.0f, 0.0f, 1.0f    // top left 
    };

    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    ComputeShader basic = ComputeShader("slime.glsl", glm::uvec2(w.width, w.height), numSlimes);
    ComputeShader slimeSim = ComputeShader("slime_sim.glsl", glm::uvec2(w.width, w.height), numSlimes);

    // set up the simulation! move this to a new function girl

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w.width, w.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glClearTexImage(texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

    glDisable(GL_DEPTH_TEST);

    GLuint settingsSSBO = 0;
    GLuint partSSBO = 0;

    glGenBuffers(1, &settingsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, settingsSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, settings.size() * sizeof(ParticleSettings), settings.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, settingsSSBO);

    glGenBuffers(1, &partSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, partSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, slimes.size() * sizeof(smallPart), slimes.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, partSSBO);
    
    // this holds stuff for the diffusion step
    unsigned int backup_texture;
    glGenTextures(1, &backup_texture);
    glBindTexture(GL_TEXTURE_2D, backup_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w.width, w.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glClearTexImage(backup_texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindImageTexture(3, backup_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, backup_texture);

    glBindImageTexture(3, backup_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);


    glUseProgram(slimeSim.ID);
    slimeSim.setUInt("numSlimes", numSlimes);
    slimeSim.setInt("numSpecies", 1); // TODO: modify when more species
    glUseProgram(shaderProgramID);
    glUniform1i(glGetUniformLocation(shaderProgramID, "TrailMap"), 0);

    unsigned int currentTex = texture;
    unsigned int oldTex = backup_texture;
    bool swapped = false;
    while (!glfwWindowShouldClose(window)) 
    {
        currentTex = swapped ? backup_texture : texture;
        oldTex = swapped ? texture : backup_texture;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float currentFrameTime = (float)glfwGetTime();

        // 1. Decay/Diffusion Compute Shader (basic.ID)
        glUseProgram(basic.ID);
        glBindImageTexture(0, currentTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(3, oldTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glUniform1f(glGetUniformLocation(basic.ID, "time"), currentFrameTime);
        glDispatchCompute(w.width, w.height, 1);
        // Barrier needed for sampling (TrailMap) and subsequent image access (slimeSim)
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        // 2. Slime Simulation Compute Shader (slimeSim.ID)
        glUseProgram(slimeSim.ID);
        glBindImageTexture(0, oldTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glUniform1f(glGetUniformLocation(slimeSim.ID, "time"), currentFrameTime);

        int groups = (numSlimes + 255) / 256;
        glDispatchCompute(groups, 1, 1);
        // Barrier needed for subsequent rendering (texture fetch) and SSBO access
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        // 3. Rendering Pass
        glUseProgram(shaderProgramID);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTex);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // update other events like input handling
        // put the stuff we've been drawing onto the display
        glfwSwapBuffers(window);
        glfwPollEvents();
        swapped = !swapped;
        this_thread::sleep_for(chrono::milliseconds(5));

        
    }

    // close GL context and any other GLFW resources
    glfwTerminate();
    return 0;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
