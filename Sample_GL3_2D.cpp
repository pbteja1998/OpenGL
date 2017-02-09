#pragma GCC diagnostic ignored "-Wwrite-strings"
#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <algorithm>

#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "signal.h"
#include "sys/wait.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define NUMBER_OF_BRICKS 10
#define WIDTH 800
#define HEIGHT 800
#define MAX_X 4.0f
#define MAX_Y 4.0f
#define MIN_SPEED 1
#define MAX_SPEED 30
#define MIRROR_LENGTH 0.8
#define BULLET_SPEED 0.2
#define NUMBER_OF_MIRRORS 4
#define SCORE_WIDTH 0.1

using namespace std;

struct COLOR {
    float r;
    float g;
    float b;
};
typedef struct COLOR COLOR;

COLOR silver = {192/255.0, 192/255.0, 192/255.0};
COLOR red = {255.0/255.0,51.0/255.0,51.0/255.0};
COLOR blue = {0,0,1};
COLOR grey = {168.0/255.0,168.0/255.0,168.0/255.0};
COLOR white = {255/255.0,255/255.0,255/255.0};
COLOR gold = {218.0/255.0,165.0/255.0,32.0/255.0};
COLOR black = {30/255.0,30/255.0,21/255.0};

struct VAO {
    GLuint VertexArrayID;
    GLuint VertexBuffer;
    GLuint ColorBuffer;

    GLenum PrimitiveMode;
    GLenum FillMode;
    int NumVertices;
};
typedef struct VAO VAO;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID;
} Matrices;

GLuint programID;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description){
    fprintf(stderr, "Error: %s\n", description);
}

pid_t pid_clock,pid_bullet,pid_bullet_hit;

void quit(GLFWwindow *window){
  //kill(pid_clock,SIGKILL);
  kill(pid_bullet,SIGKILL);
  kill(pid_bullet_hit,SIGKILL);
    glfwDestroyWindow(window);
    glfwTerminate();//    exit(EXIT_SUCCESS);
}

/* Generate VAO, VBOs and return VAO handle */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode=GL_FILL){
    struct VAO* vao = new struct VAO;
    vao->PrimitiveMode = primitive_mode;
    vao->NumVertices = numVertices;
    vao->FillMode = fill_mode;

    // Create Vertex Array Object
    // Should be done after CreateWindow and before any other GL calls
    glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
    glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
    glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

    glBindVertexArray (vao->VertexArrayID); // Bind the VAO
    glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
    glVertexAttribPointer(
                          0,                  // attribute 0. Vertices
                          3,                  // size (x,y,z)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
    glVertexAttribPointer(
                          1,                  // attribute 1. Color
                          3,                  // size (r,g,b)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, GLenum fill_mode=GL_FILL){
    GLfloat* color_buffer_data = new GLfloat [3*numVertices];
    for (int i=0; i<numVertices; i++) {
        color_buffer_data [3*i] = red;
        color_buffer_data [3*i + 1] = green;
        color_buffer_data [3*i + 2] = blue;
    }

    return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}

/* Render the VBOs handled by VAO */
void draw3DObject (struct VAO* vao){
    // Change the Fill Mode for this object
    glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

    // Bind the VAO to use
    glBindVertexArray (vao->VertexArrayID);

    // Enable Vertex Attribute 0 - 3d Vertices
    glEnableVertexAttribArray(0);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

    // Enable Vertex Attribute 1 - Color
    glEnableVertexAttribArray(1);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

    // Draw the geometry !
    glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

/**************************
 * Customizable functions *
 **************************/
float camera_rotation_angle = 0;
float cursor_x = 0, cursor_y = 0; // cursor OpenGL coordinates
float shoot_state = 0;
float bullet_direction = 0;   // Angle bullet makes with +ve x-axis
float center_leftb_x;         //x-coor of center of circle of dragging
float center_leftb_y;         //y-coor of center of circle of dragging
float center_rightb_x;        //x-coor of center of circle of dragging
float center_rightb_y;        //y-coor of center of circle of dragging
float center_cannon_x;        //x-coor of center of circle of dragging
float center_cannon_y;        //y-coor of center of circle of dragging
float basketr = 0.8;          //radius of circle of dragging
float cannonr = 1.2;          //radius of circle of dragging
float selected_object = 0;    //Object that is selected while dragging(1 => LeftBasket, 2 => RightBasket, 3 => Cannon)
float speed = 4;              //Speed of the falling bricks
float zoom = 1;
float pan = 0;
int score = 0;
float left_digit_x = 3;
float right_digit_x = -0.4+left_digit_x;
float digit_x = -0.8+left_digit_x;
float digit_y = 3;
float obstacle_x = 0.1;
float obstacle_y = 0;
int reflect_state = 0;

int left_segment_visibility[] = {0, 0, 0, 0, 0, 0, 0};
int right_segment_visibility[] = {0, 0, 0, 0, 0, 0, 0};
int segment_visibility[] = {0, 0, 0, 0, 0, 0, 0};

double mouse_x,mouse_y;       //mouse coordinates

int left_mouse_button_clicked = 0;
int right_mouse_button_clicked = 0;
GLFWwindow* window;


//converts mouse coordinates to OpenGL coordinates
float get_ogl_x(float x){
  return (2*MAX_X*x)/WIDTH-MAX_X;
}

float get_ogl_y(float y){
  return -(2*MAX_Y*y)/HEIGHT+MAX_Y;
}

class Object {
  public:
    float x ;
    float y ;
    float z ;
    float rotation_angle ;
    glm::vec3 RotateVector;
    float centre_x;
    float centre_y;
    COLOR color;

  public:
    Object(float X = 0, float Y = 0, float Z = 0, float R = 0, glm::vec3 Vector = glm::vec3 (0,0,1)){
      x = X;
      y = Y;
      z = Z;
      rotation_angle = R;
      RotateVector = Vector;
    }

    void draw(VAO* object ){
      // Eye - Location of camera. Don't change unless you are sure!!
      glm::vec3 eye ( 5*cos(camera_rotation_angle*M_PI/180.0f), 0, 5*sin(camera_rotation_angle*M_PI/180.0f) );
      // Target - Where is the camera looking at.  Don't change unless you are sure!!
      glm::vec3 target (0, 0, 0);
      // Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
      glm::vec3 up (0, 1, 0);

      // Compute Camera matrix (view)
      // Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
      //  Don't change unless you are sure!!
      Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

      // Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
      //  Don't change unless you are sure!!
      glm::mat4 VP = Matrices.projection * Matrices.view;

      // Send our transformation to the currently bound shader, in the "MVP" uniform
      // For each model you render, since the MVP will be different (at least the M part)
      //  Don't change unless you are sure!!
      glm::mat4 MVP;	// MVP = Projection * View * Model

      Matrices.model = glm::mat4(1.0f);
      glm::mat4 Translate = glm::translate (glm::vec3(x, y, z));        // glTranslatef
      glm::mat4 Rotate = glm::rotate ((float)(rotation_angle * (M_PI/180.0f)), RotateVector);
      // glm::mat4 rotate_cannon = glm::rotate((float)(cannon_rotation_angle*(M_PI/180.0f)), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
      Matrices.model *= (Translate*Rotate);
      MVP = VP * Matrices.model;
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(object);
    }

    void translate(float X = 0, float Y = 0, float Z = 0){
      x+=X;
      y+=Y;
      z+=Z;
    }

    void rotate(float R){
      rotation_angle += R;
    }

    void setRotationAngle(float r){
      rotation_angle = r;
    }

    void setCoor(float X = 0,float Y = 0, float Z = 0 ){
      x = X;
      y = Y;
      z = Z;
    }

    void drag(int flag){
      double new_mouse_x,new_mouse_y;
      glfwGetCursorPos(window, &new_mouse_x, &new_mouse_y);
      if(flag == 1 || flag == 2){
        setCoor(get_ogl_x(new_mouse_x),y);
      }
      else
        setCoor(x,get_ogl_y(new_mouse_y));
    }
};

/***** Basket Declarations ******/
VAO *left_basket_top, *right_basket_top,*left_basket_bottom;
Object LeftBasketTop(-1.75,-3.1, 0 , -80, glm::vec3(1,0,0));
Object LeftBasketBottom(-1.75,-3.9, 0 , -80,glm::vec3(1,0,0));
Object LeftBasketBody(-1.75,-3.5,0,45,glm::vec3(0,0,1));

VAO *right_basket_bottom, *left_basket_body, *right_basket_body;
Object RightBasketTop(1.75, -3.1, 0 , -80,glm::vec3(1,0,0));
Object RightBasketBottom(1.75, -3.9, 0 , -80,glm::vec3(1,0,0));
Object RightBasketBody(1.75,-3.5,0,45,glm::vec3(0,0,1));


/*****  Cannon Declarations ******/
VAO *cannon_body, *cannon_boundary, *cannon_pipe;
Object CannonPipe(-3.5, 0, 0);
Object CannonBody(-3.5, 0, 0);

/***** Bricks Declarations *****/
VAO *brick[NUMBER_OF_BRICKS];
Object Bricks[NUMBER_OF_BRICKS];
VAO *bullet;

/******Mirror Declarations ***********/
VAO *mirror[NUMBER_OF_MIRRORS];
Object Mirror[NUMBER_OF_MIRRORS];

Object Bullet(-3.5,0,0);

VAO *left_digit_segment[7];

VAO *right_digit_segment[7];

VAO *digit_segment[7];

//checking collision between bullet and brick
int check_collision(){
  for(int i = 0; i < NUMBER_OF_BRICKS ; i++ ){
    float distance = sqrt(pow(Bullet.x - Bricks[i].x,2) - pow(Bullet.y - Bricks[i].y,2));
    if(distance <= 0.1)
      return i;
  }
  return -1;
}

//Fetches the cannon angle which we will used to get bullet direction when
// space is pressed
float get_cannon_direction(){
  return CannonPipe.rotation_angle;
}

void  execute(char **argv,int background,pid_t pid){
  int status;

  if ((pid = fork()) < 0) {
    perror("ERROR: forking child process failed");
    exit(1);
  }

  if (pid == 0) {
    // child process
    if(background) {
      fclose(stdin); // close child's stdin
      fopen("/dev/null", "r"); // open a new stdin that is always empty

      if(execvp(*argv,argv) == -1){
        fprintf (stderr, "unknown command: %s\n", argv[0]);
        exit(1);
      }
    }
    else {
      if(execvp(*argv,argv)==-1){
      fprintf (stderr, "unknown command: %s\n", argv[0]);
      exit(1);
      }
    }

  } else {
    //parent process
    if (background) {

    }
    else {
      waitpid(pid, &status, 0);
      kill(pid,SIGKILL);
     // printf("%d Killed\n",pid);
    }
  }
}



void play_sound(char *mp3, pid_t pid){
  char *a[100] = {NULL};
  char *command = "mpg123";
  char *arg = "-vC";
  char *arg1 = mp3;
  a[0] = command;
  a[1] = arg;
  a[2] = arg1;
  execute(a,1,pid);
}

float shoot_bullet(){
  if( Bullet.x >= 4.0 || Bullet.x <= -4.0 || Bullet.y >= 4.0 || Bullet.y <= -4.0 )
  {
    //return the bullet to initial state i.e., to the centre of the cannon
    shoot_state = 0;
    Bullet.setCoor(CannonPipe.x, CannonPipe.y);
  }
  float x = Bullet.x;
  float y = Bullet.y;

  float cos_theta = cos(bullet_direction * (M_PI/180.0f));
  float sin_theta = sin(bullet_direction * (M_PI/180.0f));

  Bullet.translate(BULLET_SPEED*cos_theta, BULLET_SPEED*sin_theta);
}

void reshapeWindow (GLFWwindow* window, int width, int height){
    int fbwidth=width, fbheight=height;
    /* With Retina display on Mac OS X, GLFW's FramebufferSize
     is different from WindowSize */
    glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	GLfloat fov = 90.0f;

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrix as perspective
	/* glMatrixMode (GL_PROJECTION);
	   glLoadIdentity ();
	   gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); */
	// Store the projection matrix in a variable for future use
    // Perspective projection for 3D views
    // Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1f, 500.0f);

    // Ortho projection for 2D views
    Matrices.projection = glm::ortho((-MAX_X + pan )/zoom, (MAX_X+pan)/zoom, -MAX_Y/zoom, MAX_Y/zoom, 0.1f, 500.0f);
}

void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods){
     // Function is called first on GLFW_PRESS.

    if (action == GLFW_RELEASE) {
        switch (key) {
            case GLFW_KEY_LEFT_CONTROL:

                LeftBasketTop.translate(-0.5);
                LeftBasketBottom.translate(-0.5);
                LeftBasketBody.translate(-0.5);
                break;

            case GLFW_KEY_LEFT_ALT:

                LeftBasketTop.translate(0.5);
                LeftBasketBottom.translate(0.5);
                LeftBasketBody.translate(0.5);
                break;

            case GLFW_KEY_RIGHT_ALT :

                RightBasketTop.translate(-0.5);
                RightBasketBottom.translate(-0.5);
                RightBasketBody.translate(-0.5);
                break;

            case GLFW_KEY_RIGHT_CONTROL :

                RightBasketTop.translate(0.5);
                RightBasketBottom.translate(0.5);
                RightBasketBody.translate(0.5);
                break;

            case GLFW_KEY_SPACE:
                if(shoot_state == 0){
                  play_sound("bullet.mp3",pid_bullet);
                  shoot_state = 1;
                  bullet_direction = get_cannon_direction();
                }
                break;

            case GLFW_KEY_UP:
                if(zoom<2)
                  zoom+=0.1;
                reshapeWindow(window, WIDTH,HEIGHT);
                break;

            case GLFW_KEY_DOWN:
                if(zoom>1)
                  zoom-=0.1;
                if(zoom==1)
                  pan = 0;
                reshapeWindow(window, WIDTH,HEIGHT);
                break;

            case GLFW_KEY_LEFT:
                if(zoom > 1)
                pan-=0.5;
                reshapeWindow(window, WIDTH,HEIGHT);
                break;

            case GLFW_KEY_RIGHT:
                if(zoom > 1)
                pan+=0.5;
                reshapeWindow(window, WIDTH,HEIGHT);
                break;

            default:
                break;
        }
    }
    else if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                quit(window);
                break;
            default:
                break;
        }
    }
}

void keyboardChar (GLFWwindow* window, unsigned int key){
	switch (key) {
    case 'a':
    case 'A':
        if(CannonPipe.rotation_angle <= 85)
        CannonPipe.rotate(10);
        break;

    case 'd':
    case 'D':
        if(CannonPipe.rotation_angle >= -85)
        CannonPipe.rotate(-10);
        break;

    case 'S':
    case 's':

      CannonBody.translate(0, 0.5);
      CannonPipe.translate(0, 0.5);
      ////CannonBoundary.translate(0, 0.5);
      break;
    case 'F':
    case 'f':

      CannonBody.translate(0, -0.5);
      ////CannonBoundary.translate(0, -0.5);
      CannonPipe.translate(0, -0.5);
      break;
    case 'Q':
		case 'q':
            quit(window);
            break;

    case 'n':
    case 'N':
        if(speed < MAX_SPEED)
        speed++;
        break;

    case 'm':
    case 'M':
        if(speed > MIN_SPEED)
        speed--;
        break;

		default:
			break;
	}
}

//checking if within circle of dragging ans selecting corresponding object
void select_object(){
  float distance;

  distance = sqrt(pow( center_leftb_x - cursor_x, 2 ) - pow( center_leftb_y - cursor_y, 2));
  if(distance <= basketr){
    selected_object = 1;
    return;
  }

  distance = sqrt(pow( center_rightb_x - cursor_x , 2 ) - pow( center_rightb_y - cursor_y , 2));
  if(distance <= basketr){
    selected_object = 2;
    return;
  }
  distance = sqrt(pow( center_cannon_x - cursor_x , 2 ) - pow( center_cannon_y - cursor_y, 2));
  if(distance <= cannonr){
    selected_object = 3;
    return ;
  }
  selected_object = 0;
}

void handle_left_mouse_button_click(){

  left_mouse_button_clicked = 1;
  glfwGetCursorPos(window, &mouse_x, &mouse_y);
  cursor_x = get_ogl_x(mouse_x);
  cursor_y = get_ogl_y(mouse_y);
  select_object();

  //Moving the cannon to the direction of the cursor
  float slope;
  float theta;
  slope = (cursor_y - CannonPipe.y)/(cursor_x - CannonPipe.x);
  theta = atan(slope) * (180.0f/M_PI);
  CannonPipe.setRotationAngle(theta);

  if(shoot_state == 0){
    play_sound("bullet.mp3",pid_bullet);
    shoot_state = 1;
    bullet_direction = get_cannon_direction();
  }
}

void handle_left_mouse_button_release(){
  left_mouse_button_clicked = 0;
  glfwGetCursorPos(window, &mouse_x, &mouse_y);
  cursor_x = get_ogl_x(mouse_x);
  cursor_y = get_ogl_y(mouse_y);
}

void handle_right_mouse_button_click(){
    right_mouse_button_clicked = 1;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
}

void handle_right_mouse_button_release(){
    right_mouse_button_clicked = 0;
}

//executed on mouseButton click or release
void mouseButton (GLFWwindow* window, int button, int action, int mods){
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            if(action == GLFW_PRESS)
              handle_left_mouse_button_click();
            else if(action == GLFW_RELEASE)
              handle_left_mouse_button_release();
            break;

        case GLFW_MOUSE_BUTTON_RIGHT:
            if(action == GLFW_PRESS)
              handle_right_mouse_button_click();
            else if(action == GLFW_RELEASE)
              handle_right_mouse_button_release();
            break;

        default:
            break;
    }
}

//executed on mouseScroll up or down
void mouseScroll(GLFWwindow* window, double xoffset, double yoffset){
  if(yoffset == 1){      //if mouse is scrolled up
    if(zoom<2)
      zoom+=0.1;
    reshapeWindow(window, WIDTH,HEIGHT);
  }
  if(yoffset == -1){     //if mouse is scrolled down
    if(zoom>1)
      zoom-=0.1;
    if(zoom == 1)
      pan = 0;
    reshapeWindow(window, WIDTH,HEIGHT);
  }
}

//creates a regular polygon with distance to any vertex from geometric center as radius
//creates a regular polygon by creating triangles with one of the vertex at geometric center
//rotation axes at geometric center
VAO* createRegularPolygon(float radius,int sides,COLOR color = silver, int fill = 1){
  float theta = 0, angle_to_change = 360.0/sides;

  GLfloat vertex_buffer_data[sides*9];
  GLfloat color_buffer_data[sides*9];

  float center_x = 0;
  float center_y = 0;
  float center_z = 0;

  float initial_x = center_x + radius,initial_y = center_y,initial_z = center_z;
  int number_of_vertices = 0, k = 0;

  while(theta < 360){
    color_buffer_data[k] = color.r;
    vertex_buffer_data[k++] = 0;
    color_buffer_data[k] = color.g;
    vertex_buffer_data[k++] = 0;
    color_buffer_data[k] = color.b;
    vertex_buffer_data[k++] = 0;

    color_buffer_data[k] = color.r;
    vertex_buffer_data[k++] = initial_x * cos(theta * M_PI/180.0f) - initial_y * sin(theta * M_PI/180.0f);
    color_buffer_data[k] = color.g;
    vertex_buffer_data[k++] = initial_y * cos(theta * M_PI/180.0f) + initial_x * sin(theta * M_PI/180.0f);
    color_buffer_data[k] = color.b;
    vertex_buffer_data[k++] = 0;

    theta += angle_to_change;
    color_buffer_data[k] = color.r;
    vertex_buffer_data[k++] = initial_x * cos(theta * M_PI/180.0f) - initial_y * sin(theta * M_PI/180.0f);
    color_buffer_data[k] = color.g;
    vertex_buffer_data[k++] = initial_y * cos(theta * M_PI/180.0f) + initial_x * sin(theta * M_PI/180.0f);
    color_buffer_data[k] = color.b;
    vertex_buffer_data[k++] = 0;

    number_of_vertices += 3;
  }
  if(fill)
    return create3DObject(GL_TRIANGLES, number_of_vertices, vertex_buffer_data, color_buffer_data, GL_FILL);

  return create3DObject(GL_TRIANGLES, number_of_vertices, vertex_buffer_data, color_buffer_data, GL_LINE);
}

/*
  7-Segment LED Display

  **0**
  1   3
  **2**
  4   6
  **5**

*/
VAO* Segment0(float l, COLOR color = red){
  GLfloat vertex_buffer_data[] = {
    -l/2,             l,              0,
    l/2+SCORE_WIDTH,  l,              0,
    l/2+SCORE_WIDTH,  l-SCORE_WIDTH,  0,
    l/2+SCORE_WIDTH,  l-SCORE_WIDTH,  0,
    -l/2,             l,              0,
    -l/2,             l-SCORE_WIDTH,  0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
  };

  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

VAO* Segment1(float l, COLOR color = red){
  GLfloat vertex_buffer_data[] = {
    -l/2,               l, 0,
    -l/2+SCORE_WIDTH,   l, 0,
    -l/2+SCORE_WIDTH,   0, 0,
    -l/2+SCORE_WIDTH,   0, 0,
    -l/2,               l, 0,
    -l/2,               0, 0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };
  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

VAO* Segment2(float l, COLOR color = red){
  GLfloat vertex_buffer_data[] = {
    -l/2,               0,          0,
     l/2+SCORE_WIDTH,   0,          0,
     l/2+SCORE_WIDTH, -SCORE_WIDTH, 0,
     l/2+SCORE_WIDTH, -SCORE_WIDTH, 0,
    -l/2,             -SCORE_WIDTH, 0,
    -l/2,               0,          0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };
  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

VAO* Segment3(float l, COLOR color = red){
  GLfloat vertex_buffer_data[] = {
    l/2,             l,   0,
    l/2+SCORE_WIDTH, l,   0,
    l/2+SCORE_WIDTH, 0,   0,
    l/2+SCORE_WIDTH, 0,   0,
    l/2,             0,   0,
    l/2,             l,   0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };
  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

VAO* Segment4(float l, COLOR color = red){
  GLfloat vertex_buffer_data[] = {
    -l/2,             0,   0,
    -l/2+SCORE_WIDTH, 0,   0,
    -l/2+SCORE_WIDTH,-l,   0,
    -l/2+SCORE_WIDTH,-l,   0,
    -l/2,            -l,   0,
    -l/2,             0,   0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };
  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

VAO* Segment5(float l, COLOR color = red){
  GLfloat vertex_buffer_data[] = {
    -l/2,             -l,             0,
     l/2+SCORE_WIDTH, -l,             0,
     l/2+SCORE_WIDTH, -l-SCORE_WIDTH, 0,
     l/2+SCORE_WIDTH, -l-SCORE_WIDTH, 0,
    -l/2,             -l,             0,
    -l/2,             -l-SCORE_WIDTH, 0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };
  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

VAO* Segment6(float l, COLOR color = red){
  GLfloat vertex_buffer_data[] = {
    l/2,             0,              0,
    l/2+SCORE_WIDTH, 0,              0,
    l/2+SCORE_WIDTH, -l-SCORE_WIDTH, 0,
    l/2+SCORE_WIDTH, -l-SCORE_WIDTH, 0,
    l/2,             -l-SCORE_WIDTH, 0,
    l/2,              0,             0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };
  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

VAO* createSegment(int i,float l,COLOR color=red){
  switch(i){
    case 0:
      return Segment0(l,color);
    case 1:
      return Segment1(l,color);
    case 2:
      return Segment2(l,color);
    case 3:
      return Segment3(l,color);
    case 4:
      return Segment4(l,color);
    case 5:
      return Segment5(l,color);
    case 6:
      return Segment6(l,color);
  }
}

//creates a rectangle of length l and breadth b with rotation axis at one of the ends
VAO* createRectangle(float l, float b,COLOR color = silver){
  GLfloat vertex_buffer_data[] = {
    0,  b/2, 0,
    0, -b/2, 0,
    l, -b/2, 0,
    0,  b/2, 0,
    l,  b/2, 0,
    l, -b/2, 0
  };
  GLfloat color_buffer_data[] = {
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b,
    color.r, color.g, color.b
  };

  return create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

//checking collision between two bricks(ath brick with any other brick)
int check_collision_bricks(int a,int x, int y){
  for(int i = 0; i < NUMBER_OF_BRICKS ; i++){
    if(i==a)
      continue;
    float distance = sqrt(pow(x - Bricks[i].x,2) - pow(y - Bricks[i].y,2));
    if(distance <= 0.6)
        return 1;
  }
  return 0;
}

//recreating the brick with random color and at random position
//without colliding with any other existing brick
void recreateBrick(int i){
  int color = (rand()%(3-(1) + 1) + 1);

  if(color == 1){
      brick[i] = createRegularPolygon(0.15, 360, black);
      Bricks[i].color = black;
  }
  else if(color == 2){
      brick[i] = createRegularPolygon(0.15, 360, red);
      Bricks[i].color = red;
  }
  else if(color == 3){
      brick[i] = createRegularPolygon(0.15, 360, blue);
      Bricks[i].color = blue;
  }

  //random number in the range(-25 to 35) milli units
  int randNum = rand()%(35-(-30) + 1) + (-25);
  float x = randNum/10;
  float y = MAX_Y;

  while(check_collision_bricks(i, x, y)){
    randNum = rand()%(35-(-25) + 1) + (-25);
    x = randNum/10;
  }

  Bricks[i].setCoor(x,y);
}

//checking collision between block and mirror[i]
int check_collision_mirror(float x, float y, int i){

  float x1 = Mirror[i].x;
  float y1 = Mirror[i].y;
  float x2 = Mirror[i].x + MIRROR_LENGTH * cos(Mirror[i].rotation_angle * (M_PI/180.0f));
  float y2 = Mirror[i].y + MIRROR_LENGTH * sin(Mirror[i].rotation_angle * (M_PI/180.0f));

  float min_x = min(x1,x2);
  float max_x = max(x1,x2);
  float min_y = min(y1,y2);
  float max_y = max(y1,y2);

  //checking if bullet is not on the mirror which is a line segment
  if( x <= min_x || x >= max_x || y <= min_y || y >= max_y)
    return 0;

  //checking if bullet satisfies the equation of the mirror which is also a line(y2-y1)==m(x2-x1) )
  if(abs(y - Mirror[i].y - tan(Mirror[i].rotation_angle * (M_PI/180.0f)) * ( x - Mirror[i].x )) <= 0.2)
      return 1;

  return 0;
}

void draw_segment(VAO *object, int flag){
  // Eye - Location of camera. Don't change unless you are sure!!
  glm::vec3 eye ( 5*cos(camera_rotation_angle*M_PI/180.0f), 0, 5*sin(camera_rotation_angle*M_PI/180.0f) );
  // Target - Where is the camera looking at.  Don't change unless you are sure!!
  glm::vec3 target (0, 0, 0);
  // Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
  glm::vec3 up (0, 1, 0);

  // Compute Camera matrix (view)
  // Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
  //  Don't change unless you are sure!!
  Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

  // Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
  //  Don't change unless you are sure!!
  glm::mat4 VP = Matrices.projection * Matrices.view;

  // Send our transformation to the currently bound shader, in the "MVP" uniform
  // For each model you render, since the MVP will be different (at least the M part)
  //  Don't change unless you are sure!!
  glm::mat4 MVP;	// MVP = Projection * View * Model
  Matrices.model = glm::mat4(1.0f);
  if(flag == 0){
    glm::mat4 Translate = glm::translate (glm::vec3(left_digit_x, digit_y, 0));        // glTranslatef
    Matrices.model *= Translate;
  }
  else if(flag == 1){
    glm::mat4 Translate = glm::translate (glm::vec3(right_digit_x, digit_y, 0));        // glTranslatef
    Matrices.model *= Translate;
  }
  else{
    glm::mat4 Translate = glm::translate (glm::vec3(digit_x, digit_y, 0));        // glTranslatef
    Matrices.model *= Translate;
  }
  MVP = VP * Matrices.model;
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  draw3DObject(object);
}

void draw_score(){

  switch(score % 10){
    case 0:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 1;
      left_segment_visibility[2] = 0;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 1;
      left_segment_visibility[5] = 1;
      left_segment_visibility[6] = 1;
      break;
    case 1:
      left_segment_visibility[0] = 0;
      left_segment_visibility[1] = 0;
      left_segment_visibility[2] = 0;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 0;
      left_segment_visibility[5] = 0;
      left_segment_visibility[6] = 1;
      break;
    case 2:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 0;
      left_segment_visibility[2] = 1;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 1;
      left_segment_visibility[5] = 1;
      left_segment_visibility[6] = 0;
      break;
    case 3:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 0;
      left_segment_visibility[2] = 1;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 0;
      left_segment_visibility[5] = 1;
      left_segment_visibility[6] = 1;
      break;
    case 4:
      left_segment_visibility[0] = 0;
      left_segment_visibility[1] = 1;
      left_segment_visibility[2] = 1;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 0;
      left_segment_visibility[5] = 0;
      left_segment_visibility[6] = 1;
      break;
    case 5:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 1;
      left_segment_visibility[2] = 1;
      left_segment_visibility[3] = 0;
      left_segment_visibility[4] = 0;
      left_segment_visibility[5] = 1;
      left_segment_visibility[6] = 1;
      break;
    case 6:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 1;
      left_segment_visibility[2] = 1;
      left_segment_visibility[3] = 0;
      left_segment_visibility[4] = 1;
      left_segment_visibility[5] = 1;
      left_segment_visibility[6] = 1;
      break;
    case 7:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 0;
      left_segment_visibility[2] = 0;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 0;
      left_segment_visibility[5] = 0;
      left_segment_visibility[6] = 1;
      break;
    case 8:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 1;
      left_segment_visibility[2] = 1;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 1;
      left_segment_visibility[5] = 1;
      left_segment_visibility[6] = 1;
      break;
    case 9:
      left_segment_visibility[0] = 1;
      left_segment_visibility[1] = 1;
      left_segment_visibility[2] = 1;
      left_segment_visibility[3] = 1;
      left_segment_visibility[4] = 0;
      left_segment_visibility[5] = 1;
      left_segment_visibility[6] = 1;
      break;
  }

  if(score >= 10)
    switch((score/10) % 10){
      case 0:
          right_segment_visibility[0] = 1;
          right_segment_visibility[1] = 1;
          right_segment_visibility[2] = 0;
          right_segment_visibility[3] = 1;
          right_segment_visibility[4] = 1;
          right_segment_visibility[5] = 1;
          right_segment_visibility[6] = 1;
          break;
      case 1:
        right_segment_visibility[0] = 0;
        right_segment_visibility[1] = 0;
        right_segment_visibility[2] = 0;
        right_segment_visibility[3] = 1;
        right_segment_visibility[4] = 0;
        right_segment_visibility[5] = 0;
        right_segment_visibility[6] = 1;
        break;
      case 2:
        right_segment_visibility[0] = 1;
        right_segment_visibility[1] = 0;
        right_segment_visibility[2] = 1;
        right_segment_visibility[3] = 1;
        right_segment_visibility[4] = 1;
        right_segment_visibility[5] = 1;
        right_segment_visibility[6] = 0;
        break;
      case 3:
        right_segment_visibility[0] = 1;
        right_segment_visibility[1] = 0;
        right_segment_visibility[2] = 1;
        right_segment_visibility[3] = 1;
        right_segment_visibility[4] = 0;
        right_segment_visibility[5] = 1;
        right_segment_visibility[6] = 1;
        break;
      case 4:
        right_segment_visibility[0] = 0;
        right_segment_visibility[1] = 1;
        right_segment_visibility[2] = 1;
        right_segment_visibility[3] = 1;
        right_segment_visibility[4] = 0;
        right_segment_visibility[5] = 0;
        right_segment_visibility[6] = 1;
        break;
      case 5:
        right_segment_visibility[0] = 1;
        right_segment_visibility[1] = 1;
        right_segment_visibility[2] = 1;
        right_segment_visibility[3] = 0;
        right_segment_visibility[4] = 0;
        right_segment_visibility[5] = 1;
        right_segment_visibility[6] = 1;
        break;
      case 6:
        right_segment_visibility[0] = 1;
        right_segment_visibility[1] = 1;
        right_segment_visibility[2] = 1;
        right_segment_visibility[3] = 0;
        right_segment_visibility[4] = 1;
        right_segment_visibility[5] = 1;
        right_segment_visibility[6] = 1;
        break;
      case 7:
        right_segment_visibility[0] = 1;
        right_segment_visibility[1] = 0;
        right_segment_visibility[2] = 0;
        right_segment_visibility[3] = 1;
        right_segment_visibility[4] = 0;
        right_segment_visibility[5] = 0;
        right_segment_visibility[6] = 1;
        break;
      case 8:
        right_segment_visibility[0] = 1;
        right_segment_visibility[1] = 1;
        right_segment_visibility[2] = 1;
        right_segment_visibility[3] = 1;
        right_segment_visibility[4] = 1;
        right_segment_visibility[5] = 1;
        right_segment_visibility[6] = 1;
        break;
      case 9:
        right_segment_visibility[0] = 1;
        right_segment_visibility[1] = 1;
        right_segment_visibility[2] = 1;
        right_segment_visibility[3] = 1;
        right_segment_visibility[4] = 0;
        right_segment_visibility[5] = 1;
        right_segment_visibility[6] = 1;
        break;
      }

  if(score >= 100)
    switch((score/100) % 10){
    case 0:
        segment_visibility[0] = 1;
        segment_visibility[1] = 1;
        segment_visibility[2] = 0;
        segment_visibility[3] = 1;
        segment_visibility[4] = 1;
        segment_visibility[5] = 1;
        segment_visibility[6] = 1;
        break;
    case 1:
      segment_visibility[0] = 0;
      segment_visibility[1] = 0;
      segment_visibility[2] = 0;
      segment_visibility[3] = 1;
      segment_visibility[4] = 0;
      segment_visibility[5] = 0;
      segment_visibility[6] = 1;
      break;
    case 2:
      segment_visibility[0] = 1;
      segment_visibility[1] = 0;
      segment_visibility[2] = 1;
      segment_visibility[3] = 1;
      segment_visibility[4] = 1;
      segment_visibility[5] = 1;
      segment_visibility[6] = 0;
      break;
    case 3:
      segment_visibility[0] = 1;
      segment_visibility[1] = 0;
      segment_visibility[2] = 1;
      segment_visibility[3] = 1;
      segment_visibility[4] = 0;
      segment_visibility[5] = 1;
      segment_visibility[6] = 1;
      break;
    case 4:
      segment_visibility[0] = 0;
      segment_visibility[1] = 1;
      segment_visibility[2] = 1;
      segment_visibility[3] = 1;
      segment_visibility[4] = 0;
      segment_visibility[5] = 0;
      segment_visibility[6] = 1;
      break;
    case 5:
      segment_visibility[0] = 1;
      segment_visibility[1] = 1;
      segment_visibility[2] = 1;
      segment_visibility[3] = 0;
      segment_visibility[4] = 0;
      segment_visibility[5] = 1;
      segment_visibility[6] = 1;
      break;
    case 6:
      segment_visibility[0] = 1;
      segment_visibility[1] = 1;
      segment_visibility[2] = 1;
      segment_visibility[3] = 0;
      segment_visibility[4] = 1;
      segment_visibility[5] = 1;
      segment_visibility[6] = 1;
      break;
    case 7:
      segment_visibility[0] = 1;
      segment_visibility[1] = 0;
      segment_visibility[2] = 0;
      segment_visibility[3] = 1;
      segment_visibility[4] = 0;
      segment_visibility[5] = 0;
      segment_visibility[6] = 1;
      break;
    case 8:
      segment_visibility[0] = 1;
      segment_visibility[1] = 1;
      segment_visibility[2] = 1;
      segment_visibility[3] = 1;
      segment_visibility[4] = 1;
      segment_visibility[5] = 1;
      segment_visibility[6] = 1;
      break;
    case 9:
      segment_visibility[0] = 1;
      segment_visibility[1] = 1;
      segment_visibility[2] = 1;
      segment_visibility[3] = 1;
      segment_visibility[4] = 0;
      segment_visibility[5] = 1;
      segment_visibility[6] = 1;
      break;
  }

  for(int i=0; i<= 6; i++)
  {
    if(left_segment_visibility[i]==1)
      draw_segment(left_digit_segment[i],0);
    if(right_segment_visibility[i]==1)
      draw_segment(right_digit_segment[i],1);
    if(segment_visibility[i]==1)
      draw_segment(digit_segment[i],2);
  }
}

//also checks if game is over by checking if black bricks falls in basket
int check_collision_left_basket(){
  float distance;
  for(int i=0;i<NUMBER_OF_BRICKS;i++){
    distance = sqrt(pow( center_leftb_x - Bricks[i].x, 2 ) + pow( center_leftb_y - Bricks[i].y, 2));
    if(distance <= basketr-0.2){
      if(Bricks[i].color.r == red.r && Bricks[i].color.g == red.g && Bricks[i].color.b == red.b )
        score += 10;
      else if(Bricks[i].color.r == black.r && Bricks[i].color.g == black.g && Bricks[i].color.b == black.b )
        return 1 ; //Game Over
      else
        score -= 1;
      recreateBrick(i);
      draw_score();
      play_sound("bullet_hit.mp3",pid_bullet_hit);
    }
  }
  return 0;  // not game over
}

//also checks if game is over by checking if black bricks falls in basket
int check_collision_right_basket(){
  float distance;
  for(int i=0;i<NUMBER_OF_BRICKS;i++){
    distance = sqrt(pow( center_rightb_x - Bricks[i].x, 2 ) + pow( center_rightb_y - Bricks[i].y, 2));
    if(distance <= basketr-0.2){
      if(Bricks[i].color.r == blue.r && Bricks[i].color.g == blue.g && Bricks[i].color.b == blue.b)
        score += 10;
        else if(Bricks[i].color.r == black.r && Bricks[i].color.g == black.g && Bricks[i].color.b == black.b )
          return 1 ; //Game Over
        else
          score -= 1;
      recreateBrick(i);
      draw_score();
      play_sound("bullet_hit.mp3",pid_bullet_hit);
    }
  }
  return 0; // not game over
}

void draw (){
  // clear the color and depth in the frame buffer
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // use the loaded shader program
  // Don't change unless you know what you are doing
  glUseProgram (programID);

  draw_score();

  int collided_brick = check_collision();  //collision b/w bullet and brick

  if(collided_brick != -1){
    shoot_state = 0;
    Bullet.setCoor(CannonPipe.x, CannonPipe.y);
    //recreate the brick
    recreateBrick(collided_brick);
    play_sound("bullet_hit.mp3",pid_bullet_hit) ;
    if(Bricks[collided_brick].color.r == black.r && Bricks[collided_brick].color.g == black.g && Bricks[collided_brick].color.b == black.b )
      score += 10;
    else
      score -= 1;
  }

  // float r = rand()%361;
  // Mirror[3].rotate(r);
  if(Mirror[3].x >= 2.0){
    obstacle_x = -0.1;
    obstacle_y = 0.005;
  }
  if(Mirror[3].x <= -2.0){
    obstacle_x = 0.005;
    obstacle_y = 0.1;
  }
  if(Mirror[3].y >= 2.0){
    obstacle_y = -1*obstacle_y;
    obstacle_x = 0.005;
  }
  if(Mirror[3].y <= -2.0){
    obstacle_y = 0.005;
    obstacle_x = 0.1;
  }
  Mirror[3].translate(obstacle_x,obstacle_y);
  //if theta_1 is angle of mirror and theta_2 is the angle of bullet(bullet_direction)
  //reflected bullet_direction should be 2*thetha_1-theta_2 with +ve X-axis
  for(int i = 0; i < NUMBER_OF_MIRRORS; i++ ){
    if(check_collision_mirror(Bullet.x,Bullet.y,i))
      bullet_direction = 2*Mirror[i].rotation_angle - Bullet.rotation_angle ;
    Mirror[i].draw(mirror[i]);
  }


  //Draw the bricks
  //translate all bricks and also recreate the brick if brick moves out of the world
  for(int i = 0; i < NUMBER_OF_BRICKS; i++){
    Bricks[i].draw(brick[i]);
    if(Bricks[i].y <= -4.0){
      //choosing random color and random position for the brick
      recreateBrick(i);
    }
    Bricks[i].translate(0,-0.01*speed);
  }

  //Draw all objects
  LeftBasketTop.draw(left_basket_top);
  LeftBasketBottom.draw(left_basket_bottom);
  LeftBasketBody.draw(left_basket_body);
  RightBasketTop.draw(right_basket_top);
  RightBasketBottom.draw(right_basket_bottom);
  RightBasketBody.draw(right_basket_body);
  CannonPipe.draw(cannon_pipe);
  CannonBody.draw(cannon_body);
  if(shoot_state){
    Bullet.draw(bullet);
    shoot_bullet();
  }

  //Updating the centers of circles of dragging based on present positions
  center_leftb_x = (LeftBasketTop.x + LeftBasketBottom.x + LeftBasketBody.x)/3;
  center_leftb_y = (LeftBasketTop.y + LeftBasketBottom.y + LeftBasketBody.y)/3;
  center_rightb_x = (RightBasketTop.x + RightBasketBottom.x + RightBasketBody.x)/3;
  center_rightb_y = (RightBasketTop.y + RightBasketBottom.y + RightBasketBody.y)/3;
  center_cannon_x = (CannonBody.x + CannonPipe.x)/2;
  center_cannon_y = (CannonBody.y + CannonPipe.y)/2;

  //panning
  if(right_mouse_button_clicked){
    double new_mouse_x,new_mouse_y;
    glfwGetCursorPos(window, &new_mouse_x, &new_mouse_y);
    pan = get_ogl_x(mouse_x) - get_ogl_x(new_mouse_x);
    reshapeWindow(window, WIDTH,HEIGHT);
  }

  if(left_mouse_button_clicked){

    if(selected_object == 1){
      LeftBasketTop.drag(selected_object);
      LeftBasketBottom.drag(selected_object);
      LeftBasketBody.drag(selected_object);
    }

    else if(selected_object == 2){
      RightBasketTop.drag(selected_object);
      RightBasketBottom.drag(selected_object);
      RightBasketBody.drag(selected_object);
    }

    else if(selected_object == 3){
      CannonBody.drag(selected_object);
      CannonPipe.drag(selected_object);
    }

  }

}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height){
    GLFWwindow* window; // window desciptor/handle

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {//        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Sample OpenGL 3.3 Application", NULL, NULL);

    if (!window) {
        glfwTerminate();//        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval( 1 );

    /* --- register callbacks with GLFW --- */

    /* Register function to handle window resizes */
    /* With Retina display on Mac OS X GLFW's FramebufferSize
     is different from WindowSize */
    glfwSetFramebufferSizeCallback(window, reshapeWindow);
    glfwSetWindowSizeCallback(window, reshapeWindow);

    /* Register function to handle window close */
    glfwSetWindowCloseCallback(window, quit);

    /* Register function to handle keyboard input */
    glfwSetKeyCallback(window, keyboard);      // general keyboard input
    glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

    /* Register function to handle mouse click */
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetMouseButtonCallback(window, mouseButton);  // mouse button clicks
    glfwSetScrollCallback(window, mouseScroll); // mouse scroll

    return window;
}


/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height){
    /* Objects should be created before any other gl function and shaders */
	// Create the models
  float basket_radius = 0.4;
  float basket_body_radius = basket_radius * sqrt(2);
	left_basket_top = createRegularPolygon(basket_radius, 360); // Generate the VAO, VBOs, vertices data & copy into the array buffer
  right_basket_top = createRegularPolygon(basket_radius, 360);
  left_basket_bottom = createRegularPolygon(basket_radius, 360, red);
  LeftBasketBody.color = red;
  right_basket_bottom = createRegularPolygon(basket_radius, 360, blue);
  left_basket_body = createRegularPolygon(basket_body_radius, 4, red);
  right_basket_body = createRegularPolygon(basket_body_radius,4, blue);
  RightBasketBody.color = blue;
  cannon_body = createRegularPolygon(0.4, 10, grey);
  cannon_boundary = createRegularPolygon(1.2, 8, white, 0);
  cannon_pipe = createRectangle(0.8, 0.2, gold);
  bullet = createRegularPolygon(0.1, 360, gold);

  for(int i=0;i<=6;i++){
    left_digit_segment[i] = createSegment(i, 0.2);
    right_digit_segment[i] = createSegment(i, 0.2);
    digit_segment[i] = createSegment(i, 0.2);
  }

  //creating all bricks and assigning random color to them
  for(int i = 0; i < NUMBER_OF_BRICKS; i++){
    int color = (rand()%(3-(1) + 1) + 1);
    if(color == 1){
        brick[i] = createRegularPolygon(0.15, 360, black);
        Bricks[i].color = black;
    }
    else if(color == 2){
        brick[i] = createRegularPolygon(0.15, 360, red);
        Bricks[i].color = red;
    }
    else if(color == 3){
        brick[i] = createRegularPolygon(0.15, 360, blue);
        Bricks[i].color = blue;
    }

    int randNum;
    float x,y;
    //choosing random position in the range(-25 to 35) milli units
    randNum = rand()%(35-(-25) + 1) + (-25);
    x = randNum/10;

    randNum = rand()%(35-(-25) + 1) + (-25);
    y = randNum/10;

    Bricks[i].translate(x,y);
    Bricks[i].rotate(45);
  }

  for(int i = 0; i < NUMBER_OF_MIRRORS; i++){
    mirror[i] = createRectangle(MIRROR_LENGTH,0.1,white);
  }

  Mirror[0].translate(-1,3);
  Mirror[0].rotate(150);

  Mirror[1].translate(3,2);
  Mirror[1].rotate(135);

  Mirror[2].translate(0,0);
  Mirror[2].rotate(70);

  Mirror[3].translate(1,1);
  Mirror[3].rotate(45);

	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");


	reshapeWindow (window, width, height);

    // Background color of the scene
	glClearColor (0.3f, 0.3f, 0.3f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);

  cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
  cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
  cout << "VERSION: " << glGetString(GL_VERSION) << endl;
  cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

int main (int argc, char** argv){

  window = initGLFW(WIDTH,HEIGHT);

	initGL (window, WIDTH, HEIGHT);

  double last_update_time = glfwGetTime(), current_time;

  while (!glfwWindowShouldClose(window)) {

      if(score>=1000){
        //kill(pid_clock,SIGKILL);
        kill(pid_bullet,SIGKILL);
        kill(pid_bullet_hit,SIGKILL);
        cout << "You win" << endl;
        cout << "Game Over" << endl;
        cout << "You Win" << endl;
        break;
      }

      if(check_collision_left_basket()){
        //kill(pid_clock,SIGKILL);
        kill(pid_bullet,SIGKILL);
        kill(pid_bullet_hit,SIGKILL);
        cout << "Black Ball is collected in the Basket" << endl;
        cout << "Your score is " << score << endl;
        cout << "Game Over" << endl;
        cout << "You Lose" << endl;
        break;
      }

      if(check_collision_right_basket()){
        //kill(pid_clock,SIGKILL);
        kill(pid_bullet,SIGKILL);
        kill(pid_bullet_hit,SIGKILL);
        cout << "Black Ball is collected in the Basket" << endl;
        cout << "Your score is " << score << endl;
        cout << "Game Over" << endl;
        cout << "You Lose" << endl;
        break;
      }

      // OpenGL Draw commands
      draw();

      // Swap Frame Buffer in double buffering
      glfwSwapBuffers(window);

      // Poll for Keyboard and mouse events
      glfwPollEvents();

      // Control based on time (Time based transformation like 5 degrees rotation every 0.5s)
      current_time = glfwGetTime(); // Time in seconds
      if ((current_time - last_update_time) >= 0.5) { // atleast 0.5s elapsed since last frame
          // do something every 0.5 seconds ..
          last_update_time = current_time;
      }
  }

  glfwTerminate();
  //    exit(EXIT_SUCCESS);
}
