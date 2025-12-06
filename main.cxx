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
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

#include <GL/glew.h>    // include GLEW and new version of GL on Windows
#include <GLFW/glfw3.h> // GLFW helper library
#include <glm/vec3.hpp>   // glm::vec3
#include <glm/vec2.hpp>
//#include <glm/vec4.hpp>   // glm::vec4
//#include <glm/mat4x4.hpp> // glm::mat4
//#include <glm/gtx/string_cast.hpp>
//#include <glm/gtc/type_ptr.hpp>
//#include <glm/gtc/matrix_transform.hpp>  // glm::translate, glm::rotate, glm::scale

const unsigned int REF_PER_FRAME = 1000;
string world_type = "mostly_blue";

class SlimeManager;

// for whatever reason, clamp is not working, so we will settle for this
template <typename T>
constexpr const T& my_clamp(const T& val, const T& low, const T& high) {
    return std::max(low, std::min(val, high));
}

class coord {
public:
    float x;
    float y;
    int cx, cy;

    coord(float tx, float ty, int width, int height)
        : x(tx), y(ty)
    {
        cx = my_clamp((int)floor(x), 0, width - 1);
        cy = my_clamp((int)floor(y), 0, height - 1);
    }
};


float scrToWorld(float x, int max) {
    float slope = (float)(max - 1) / 2.;
    return slope * (x + 1);
}

float worldToScr(float x, int max) {
    float slope = 2. / (float)(max - 1);
    return (slope * x) - 1;
}


class Particle {
public:
    float x, y; // screen space!
    float mx, my; // world(ish) space!
    float dir; 
    //float speed;
    uniform_int_distribution<> r_int;
    uniform_int_distribution<> r_neg;
    mt19937 gen;
    glm::vec3 color;

    float type; // this is float because im using the z coord - doesn't need to be

    int moveDist;
    int sensDist;
    float sensAngle;
    float rotAngle;

    Particle() : gen(random_device{}()),
                 r_int(1, 10),
                 r_neg(-1,2)
    {
        type = (float) r_neg(gen);
        // cout << type << endl;
        int t = (int)type;
        moveDist = (t ? 1 : 2);
        sensDist = (t ? 9 : 3);
        sensAngle = (t ? 44 : 22.5)  * (3.141593/180);
        //speed = (t ? 2.0 : 1.0);
    }

    Particle(int _moveDist, int _sensDist, float _sensAngle, float _rotAngle, glm::vec3 _color) : gen(random_device{}()),
        r_int(1, 10),
        r_neg(-1, 2)
    {
        moveDist = _moveDist;
        sensDist = _sensDist;
        sensAngle = _sensAngle * (3.141593 / 180);
        rotAngle = _rotAngle * (3.141593 / 180);
        //speed = _speed;
        color = _color;
    }

    void turn(float u, float r, float l) {
        if (u > r and u > l) return; // no change

        if (r == l) {
            // turn randomly (based on coin flip
            if (r_int(gen) % 2 == 0)
                dir += sensAngle;  // turn right (?)
            else dir -= sensAngle; // turn left(?)
        }
        else if (l > r) {
            dir -= sensAngle; // left
        }
        else dir += sensAngle; // right
    }
};


class SlimeManager {
public:
    int slimeCount;
    int width, height;
    vector<Particle> slimes;
    GLuint vao;
    GLuint v_vbo;
    GLuint c_vbo;
    uniform_real_distribution<> distrib;
    uniform_real_distribution<> angleDist;
    uniform_int_distribution<> type_gen;
    mt19937 gen;

    SlimeManager(json &config, string current_world) :
        slimeCount(config["world_configs"][current_world]["slimes"]),
        width(config["world_configs"][current_world]["width"]),
        height(config["world_configs"][current_world]["height"]),
        gen(random_device{}()),
        distrib(-1.0, 1.0),
        type_gen(-1.0, 2.0),
        angleDist(0.0, 2.0 * 3.141593)
    {
        json world = config.at("world_configs").at(current_world);

        int slimed = 0;
        for (auto& kvp : world.at("slime_types").items()) {
            float slime_ratio = kvp.value();

            int s_count = (int)(slime_ratio * (float)slimeCount);
            json slime_conf = config.at("slime_configs").at(kvp.key());

            float r = slime_conf.at("color")[0];
            float g = slime_conf.at("color")[1];
            float b = slime_conf.at("color")[2];

            glm::vec3 col = glm::vec3(r,g,b);

            int start = slimed;
            int end = start + s_count;
            for (int i = start; i < end; i++) {
                slimes.emplace_back(slime_conf["move_dist"],
                    slime_conf["sensor_dist"],
                    slime_conf["sensor_rotation"],
                    slime_conf["rotation_angle"],
                    col);

                float randx = distrib(gen);
                float randy = distrib(gen);
                slimes[i].x = randx;
                slimes[i].mx = scrToWorld(randx, width);
                slimes[i].y = randy;
                slimes[i].my = scrToWorld(randy, height);
                slimes[i].dir = angleDist(gen);
                slimed++;
            }
        }
        cout << "slimes initialized." << endl;
    }

    SlimeManager() {

    }

    void assign_angle(int i) {
        slimes[i].dir = angleDist(gen);
        //cout << " in here: " << slimes[i].dir << endl;
    }


    void slimesToGPU() {
        int numind = slimeCount * 2;
        int numCol = slimeCount * 3;

        std::vector<GLfloat> vertices(numind);
        std::vector<GLfloat> color(numCol);

        for (int i = 0; i < slimeCount; i++) {
            vertices[i * 2] = slimes[i].x;
            vertices[i * 2 + 1] = slimes[i].y;
            color[i * 3] = slimes[i].color.x;
            color[i * 3 + 1] = slimes[i].color.y;
            color[i * 3 + 2] = slimes[i].color.z;
           // vertices[i * 3 + 2] = slimes[i].type; // this is actually the z but im not using the z
        }

        // --- VAO ---
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        // --- Position VBO ---
        glGenBuffers(1, &v_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
            vertices.data(), GL_DYNAMIC_DRAW);

        // Bind VBO to binding index 0
        glBindVertexBuffer(0, v_vbo, 0, 2 * sizeof(float));

        // Attribute 0 describes positions
        glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexAttribBinding(0, 0);
        glEnableVertexAttribArray(0);

        // --- Color VBO ---
        glGenBuffers(1, &c_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
        glBufferData(GL_ARRAY_BUFFER, color.size() * sizeof(float),
            color.data(), GL_DYNAMIC_DRAW);

        // Bind color VBO to binding index 1
        glBindVertexBuffer(1, c_vbo, 0, 3 * sizeof(float));

        // Attribute 1 describes colors
        glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexAttribBinding(1, 1);
        glEnableVertexAttribArray(1);

        // Cleanup
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

    }


};


class World {
public:
    int width, height;
    float dwidth, dheight;
    SlimeManager sm;
    vector<vector<int>> data;
    vector<vector<float>> trails;
    json world_config;
    string current_world;

    World(const int w, const int h, SlimeManager &s) : data(w, vector<int>(h, 0)), 
                                                       trails(w, vector<float>(h, 0)),
                                                       sm(s){
        width = w; height = h;

        sm.slimesToGPU();
    }

    World(json conf, string world_name) : data(conf["width"], vector<int>(conf["height"], 0)),
                                          trails(conf["width"], vector<float>(conf["height"], 0)) {
        width = conf["width"]; height = conf["height"];
        world_config = conf;
        current_world = world_name;
       // cout << "running here" << endl;

    }

    void AddSlimeManager(json config) {
        sm = SlimeManager(config, current_world);
        sm.slimesToGPU();
    }

    float scrToWorldx(float x) {
        float slope = (float)(width-1) / 2.;
        return slope * (x + 1);
    }

    float scrToWorldy(float y) {
        float slope = (float)(height-1) / 2.;
        return slope * (y + 1);
    }

    float worldToScrx(int x) {
        float slope = 2. / (float)(width-1);
        return (slope * x) - 1;
    }
    float worldToScry(int y) {
        float slope = 2. / (float)(height-1);
        return (slope * y) - 1;
    }

    void serial_tick() {    

        // motor stage
        for (int i = 0; i < sm.slimeCount; i++) {
            Particle& slime = sm.slimes[i];
            // start by sensing
            // look at first sens
            
            coord up(slime.mx + cos(slime.dir) * slime.sensDist,
                     slime.my + sin(slime.dir) * slime.sensDist,
                     width, height);

            float up_trail = trails[up.cx][up.cy];

            // calculate right hand sens
            coord right(slime.mx + cos(slime.dir + slime.sensAngle) * slime.sensDist,
                        slime.my + sin(slime.dir + slime.sensAngle) * slime.sensDist,
                        width, height);

            float right_trail = trails[right.cx][right.cy];

            // calculate left hand sens
            coord left(slime.mx + cos(slime.dir - slime.sensAngle) * slime.sensDist,
                          slime.my + sin(slime.dir - slime.sensAngle) * slime.sensDist,
                          width, height);
            float left_trail = trails[left.cx][left.cy];

            slime.turn(up_trail, right_trail, left_trail);

           /* cout << "======" << endl;
            cout << "my dir is " << slime.dir << endl;
            cout << slime.mx << " " << slime.my << endl;
            cout << "want to sense " << up.x << " " << up.y << endl;
            cout << "want to sense " << right.x << " " << right.y << endl;
            cout << "want to sense " << left.x << " " << left.y << endl;*/

            float dirx = cos(slime.dir);
            float diry = sin(slime.dir);
            float newPosx = slime.mx + (dirx * slime.moveDist);
            float newPosy = slime.my + (diry * slime.moveDist);

            if (newPosx < 0 || newPosx >= width) {
                newPosx = max((float)0.0, min(newPosx, (float)width - 1));
                //slime.dir += 3.14 * rand();
                sm.assign_angle(i);
            } if (newPosy < 0 || newPosy >= height) {
                newPosy = max((float)0., min(newPosy, (float)height - 1));
                sm.assign_angle(i);
            }

            slime.mx = newPosx;
            slime.x = worldToScrx(newPosx);
            slime.my = newPosy;
            slime.y = worldToScry(newPosy);

            int cx = my_clamp((int)slime.mx, 0, width - 1);
            int cy = my_clamp((int)slime.my, 0, height - 1);
            trails[cx][cy] += 1.0;
        }

        // sensory stage
        for (int x = 0; x < width; x++)
            for (int y = 0; y < height; y++)
                trails[x][y] *= 0.95f;

        glBindBuffer(GL_ARRAY_BUFFER, sm.v_vbo);

        // stores pos of each slime
        int numind = sm.slimeCount * 2;
        std::vector<GLfloat> vertices(numind);

        for (int i = 0; i < sm.slimeCount; i++) {
            vertices[i * 2] = sm.slimes[i].x;
            vertices[i * 2 + 1] = sm.slimes[i].y;
        }

        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        //this_thread::sleep_for(chrono::milliseconds(5));
    }
};

class RenderManager;

const char *GetVertexShader();
const char *GetFragmentShader();


void _print_shader_info_log(GLuint shader_index) {
  int max_length = 2048;
  int actual_length = 0;
  char shader_log[2048];
  glGetShaderInfoLog(shader_index, max_length, &actual_length, shader_log);
  printf("shader info log for GL index %u:\n%s\n", shader_index, shader_log);
}

class RenderManager
{
public:
                 RenderManager();
   void          SetColor(double r, double g, double b);
   void          SetColor(std::vector<double>&);
   //void          Render(ShapeType, glm::mat4 model);
   GLFWwindow   *GetWindow() { return window; };


  private:
   glm::vec3 color;
   //GLuint sphereVAO;
   //GLuint sphereNumPrimitives;
   //GLuint cylinderVAO;
   //GLuint cylinderNumPrimitives;
   //GLuint mvploc;
   //GLuint colorloc;
   //GLuint camloc;
   //GLuint ldirloc;
   /*glm::mat4 projection;
   glm::mat4 view;*/
   GLuint shaderProgram;
   GLFWwindow *window;

   void SetUpWindowAndShaders();
  // void MakeModelView(glm::mat4 &);
};

RenderManager::RenderManager()
{
  SetUpWindowAndShaders();
}


void
RenderManager::SetUpWindowAndShaders()
{
  
  // get version info
  const GLubyte *renderer = glGetString(GL_RENDERER); // get renderer string
  const GLubyte *version = glGetString(GL_VERSION);   // version as a string
  printf("Renderer: %s\n", renderer);
  printf("OpenGL version supported %s\n", version);

  // tell GL to only draw onto a pixel if the shape is closer to the viewer
  glEnable(GL_DEPTH_TEST); // enable depth-testing
  glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"

  const char* vertex_shader = GetVertexShader();
  const char* fragment_shader = GetFragmentShader();

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &vertex_shader, NULL);
  glCompileShader(vs);
  int params = -1;
  glGetShaderiv(vs, GL_COMPILE_STATUS, &params);
  if (GL_TRUE != params) {
    fprintf(stderr, "ERROR: GL shader index %i did not compile\n", vs);
    _print_shader_info_log(vs);
    exit(EXIT_FAILURE);
  }

  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &fragment_shader, NULL);
  glCompileShader(fs);
  glGetShaderiv(fs, GL_COMPILE_STATUS, &params);
  if (GL_TRUE != params) {
    fprintf(stderr, "ERROR: GL shader index %i did not compile\n", fs);
    _print_shader_info_log(fs);
    exit(EXIT_FAILURE);
  }

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, fs);
  glAttachShader(shaderProgram, vs);
  glLinkProgram(shaderProgram);
  glUseProgram(shaderProgram);
}

void RenderManager::SetColor(double r, double g, double b)
{
   color[0] = r;
   color[1] = g;
   color[2] = b;
}

void RenderManager::SetColor(std::vector<double>& cols)
{
    color[0] = cols[0];
    color[1] = cols[1];
    color[2] = cols[2];
}

//void SetUpVBOs(std::vector<float> &coords, std::vector<float> &colors,
//               GLuint &points_vbo, GLuint &colors_vbo, GLuint &index_vbo)
//{
//  int numIndices = coords.size()/3;
//  std::vector<GLuint> indices(numIndices);
//  for(int i = 0; i < numIndices; i++)
//    indices[i] = i;
//
//  points_vbo = 0;
//  glGenBuffers(1, &points_vbo);
//  glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
//  glBufferData(GL_ARRAY_BUFFER, coords.size() * sizeof(float), coords.data(), GL_STATIC_DRAW);
//
//  colors_vbo = 0;
//  glGenBuffers(1, &colors_vbo);
//  glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
//  glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);
//
//  index_vbo = 0;    // Index buffer object
//  glGenBuffers(1, &index_vbo);
//  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_vbo);
//  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
//}


World SetUpWorld(json config) {
    json world_configuration = config["world_configs"][world_type];
    return World(world_configuration, world_type);
}

int main() 
{
    std::ifstream f("configs.json");
    json config = json::parse(f);

    World w = SetUpWorld(config);

    if (!glfwInit()) {
        fprintf(stderr, "ERROR: could not start GLFW3\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(w.width, w.height, "sliiime", NULL, NULL);
    if (!window) {
        fprintf(stderr, "ERROR: could not open window with GLFW3\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit();

    w.AddSlimeManager(config);
    RenderManager rm = RenderManager();


    while (!glfwWindowShouldClose(window)) 
    {
        // wipe the drawing surface clear
        /*  cout << "slowdown" << endl;
            this_thread::sleep_for(chrono::milliseconds(500));*/
        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(w.sm.vao);

        w.serial_tick();
        glDrawArrays(GL_POINTS, 0, w.sm.slimeCount);

        // update other events like input handling
        glfwPollEvents();
        // put the stuff we've been drawing onto the display
        glfwSwapBuffers(window);
    }

    // close GL context and any other GLFW resources
    glfwTerminate();
    return 0;
}


const char* GetVertexShader() {
    static char vertShader[4096];
    strcpy(vertShader,
        "#version 400\n"
        "layout (location = 0) in vec2 pos;\n"
        "layout (location = 1) in vec3 color;\n"
        "out vec3 f_color;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);\n"
        "   f_color = color;\n"
        "}\n"
    );
    return vertShader;
}

const char *GetFragmentShader()
{
   static char fragmentShader[4096];
   strcpy(fragmentShader, 
           "#version 400\n"
           "in vec3 f_color;\n"
           "out vec4 frag_color;\n"
           "void main() {\n"
           "    frag_color = vec4(f_color.x, f_color.y, f_color.z, 1.0);\n"
           "}\n"
         );
   return fragmentShader;
}

