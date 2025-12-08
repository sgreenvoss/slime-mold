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
#include <mutex>
#include <queue>
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


vector<string>world_options = { "red_smaller", "red_green_smaller", 
                                "six_seven_rose_circles", "mostly_blue", 
                                "purple_haze", "scary_subway", "scary_subway_v2"};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
// green_city was 60000

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
    int circle; // stores 1-3 for channel preference in 10s place, then 0-1 for if it should spawn in circle
    float padding[3];
};

class CommandQueue {
private:
    std::queue<std::string> q;
    std::mutex m;

public:
    // Non-blocking way to check for a command
    bool try_pop(std::string& value) {
        std::lock_guard<std::mutex> lock(m);
        if (q.empty()) {
            return false;
        }
        value = q.front();
        q.pop();
        return true;
    }

    // Blocking way to push a command
    void push(const std::string& value) {
        std::lock_guard<std::mutex> lock(m);
        q.push(value);
    }
};
CommandQueue command_queue;
bool running = true;
mt19937 generator(random_device{}());
uniform_real_distribution<> angle_dist(0.0, 2.0 * M_PI);
uniform_real_distribution<> radius_sq_dist(0.0, 1.0);



glm::vec2 generateRandomPointInUnitCircle() {

    // Uniform distribution for angle (0 to 2*PI)
    
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

void inputListener() {
    cout << "enter an integer corresponding to one of the simulations. " << endl;
    string input;
    int i = 0;
    for (string s : world_options) {
        cout << i++ << ": " << s << endl;
    }
    while (running) {
        cout << ">>" << flush;
        if (getline(cin, input)) {
            if (input.empty()) continue;
            // stupid err checking here
            try {
                int in = stoi(input);
                if (in >= 0 && in < i) {
                    command_queue.push(input);
                }
                else {
                    command_queue.push("stop");
                }
            }
            catch (const exception& e) {
                continue;
            }

        }
        else {
            command_queue.push("stop");
            break;
        }

    }
    cout << "ok bye" << endl;
}

class World {
public:
    int width, height;
    int numSlimes;
    int numSpecies;
    float decay;
    json world_config;
    json conf;
    unsigned int texture, backup_texture;
    ComputeShader basic, slimeSim;


    World(json &_conf, int simID) : 
        conf(_conf),
        world_config(_conf.at("world_configs").at(world_options[simID]))
    {
        width = world_config["width"];
        height = world_config["height"];
        numSlimes = world_config["slimes"];
        numSpecies = world_config["species_ct"];
        decay = world_config["decay"];

    }

    void reconfig(int simID) {
        world_config = conf.at("world_configs").at(world_options[simID]);
        cout << world_config << endl;
        width = world_config["width"];
        height = world_config["height"];
        numSlimes = world_config["slimes"];
        cout << "numslimes in reconfig is " << numSlimes << endl;
        numSpecies = world_config["species_ct"];
        decay = world_config["decay"];
    }

    void SetUpGPU(vector<ParticleSettings>& settings, vector<smallPart>& slimes) {
        glGenTextures(1, &texture); 
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glClearTexImage(texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

        glGenTextures(1, &backup_texture); 
        glBindTexture(GL_TEXTURE_2D, backup_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glClearTexImage(backup_texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindImageTexture(3, backup_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

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

        glUseProgram(basic.ID);
        basic.setFloat("decay", decay);
        glUseProgram(slimeSim.ID);
        slimeSim.setUInt("numSlimes", numSlimes);
        slimeSim.setInt("numSpecies", numSpecies);
    }

    void InitGL() {
        cout << "numslimes in initgl is " << numSlimes << endl;
        basic = ComputeShader("slime.glsl", glm::uvec2(width, height), numSlimes);
        slimeSim = ComputeShader("slime_sim.glsl", glm::uvec2(width, height), numSlimes);
    }

    void SetUpSim(vector<ParticleSettings> &settings, vector<smallPart> &slimes) {
        glm::vec2 screen_center = glm::vec2((float)width / 2.0f, (float)height / 2.0f);
        uniform_int_distribution<int> distx(0, width - 1);
        uniform_int_distribution<int> disty(0, height - 1);

        int slimed = 0;
        int idx = 0;

        for (auto& kvp : world_config.at("slime_types").items()) {
            // cout << kvp.key() << " " << kvp.value() << endl;
            //float slime_ratio = kvp.value();
            cout << round(numSlimes * kvp.value()) << endl;
            int s_count = (int)ceil(numSlimes * (float)kvp.value());
            cout << "s_count: " << s_count << endl;
            json slime_conf = conf.at("slime_configs").at(kvp.key());

            float r = slime_conf.at("color")[0];
            float g = slime_conf.at("color")[1];
            float b = slime_conf.at("color")[2];

            glm::vec4 col = glm::vec4(r, g, b, 1.0);

            settings.push_back({ slime_conf["move_dist"],
                slime_conf["sensor_dist"],
                slime_conf["sensor_rotation"],
                slime_conf["rotation_angle"],
                col,
                slime_conf["circle"]
                }
            );

            int start = slimed;
            int end = start + s_count;
            cout << "species id: " << idx << endl;
            for (int i = start; i < end; i++) {
                slimed++;

                glm::ivec2 pos;

                if (slime_conf["circle"] % 2 == 0) {
                    float initial_radius = 0.9 * std::min((float)width, (float)height) / 2.0f;
                    glm::vec2 p_i = screen_center + (generateRandomPointInUnitCircle() * initial_radius);
                

                    // p_i += glm::vec2((float)width / 2, (float)height / 2);
                    int x = round(p_i.x);
                    int y = round(p_i.y);

                    x = max(0, min(x, width - 1));
                    y = max(0, min(y, height - 1));

                    pos = glm::ivec2(x, y);
                }
                else {
                    pos = glm::ivec2(distx(generator), disty(generator));
                }
                
                
                slimes.push_back(smallPart{ pos, (float)angle_dist(generator), idx });
            }
            cout << "number of slimes: " << slimes.size() << endl;
            idx++;
        }
        cout << "settings length: " << settings.size() << endl;
        
    }

    unsigned int simStep(bool swapped) {
        float currentFrameTime = (float)glfwGetTime();

        unsigned int currentTex = swapped ? backup_texture : texture;
        unsigned int oldTex = swapped ? texture : backup_texture;

        glUseProgram(basic.ID);
        glBindImageTexture(0, currentTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glBindImageTexture(3, oldTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glUniform1f(glGetUniformLocation(basic.ID, "time"), currentFrameTime);

        glDispatchCompute(width, height, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        // 2. Slime Simulation Compute Shader (slimeSim.ID)
        glUseProgram(slimeSim.ID);
        glBindImageTexture(0, oldTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glUniform1f(glGetUniformLocation(slimeSim.ID, "time"), currentFrameTime);

        int groups = (numSlimes + 255) / 256;
        glDispatchCompute(groups, 1, 1);
        // Barrier needed for subsequent rendering (texture fetch) and SSBO access
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        return currentTex;

    }
};

void _print_shader_info_log(GLuint shader_index) {
  int max_length = 2048;
  int actual_length = 0;
  char shader_log[2048];
  glGetShaderInfoLog(shader_index, max_length, &actual_length, shader_log);
  printf("shader info log for GL index %u:\n%s\n", shader_index, shader_log);
}


int main() 
{
    int sim_id = 0;

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
    World w(config, sim_id);

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
    w.InitGL();

    // -----------------------------------simple shaders-----------------------------------
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

    // end simple shader zone ------------------------------------------------------------


    vector<ParticleSettings> settings;
    vector<smallPart> slimes;
    uniform_real_distribution<> angleDist((float)0.0, (float)2.0 * 3.141593);
    mt19937 gen(random_device{}());

    w.SetUpSim(settings, slimes);
    w.SetUpGPU(settings, slimes);
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

    glUseProgram(shaderProgramID);
    glUniform1i(glGetUniformLocation(shaderProgramID, "TrailMap"), 0);

    bool swapped = false;

    // start the input listener
    cout << "simulation started, sim id is " << sim_id << endl;
    thread inputThread(inputListener);
    int t = 5;
    if (w.numSlimes > 1000000) {
        t = 0;
    }

    while (!glfwWindowShouldClose(window)) 
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        unsigned int tex = w.simStep(swapped);
        glUseProgram(shaderProgramID);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
        swapped = !swapped;

        this_thread::sleep_for(chrono::milliseconds(t));

        string command;
        if (command_queue.try_pop(command)) {
            cout << "heard: " << command << endl;
            try {
                sim_id = stoi(command);
                settings.clear();
                slimes.clear();
                w.reconfig(sim_id);
                w.InitGL();
                w.SetUpSim(settings, slimes);
                w.SetUpGPU(settings, slimes);
                int t = 5;
                if (w.numSlimes > 1000000) {
                    t = 0;
                }
            }
            catch (const exception& e) {
                continue;
            }
        }
        
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
