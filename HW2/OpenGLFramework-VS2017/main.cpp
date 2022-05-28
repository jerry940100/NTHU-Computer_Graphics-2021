#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

#define PI 3.1415926
#define deg2rad(x) ((x)*((3.1415926f)/(180.0f)))

using namespace std;

// Default window size
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 800;

bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

// for cursor/mouse function
int current_x, current_y;
int diff_x, diff_y;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	LightEdit = 3,
	ShininessEdit = 4,
};

struct Uniform
{
	GLint iLocMVP;
};
Uniform uniform;

vector<string> filenames; // .obj filename list

struct PhongMaterial
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;
};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;

int cur_idx = 0; // represent which model should be rendered now

// Assignment 02 - Shader attributes for uniform variables
GLint iLocmv;
//Light reflection coeficient 
GLint iLocKa;
GLint iLocKd;
GLint iLocKs;
//Shininess
GLint iLocShininess;
float shininess = 64.0f;
//diffuse light intensity
GLint iLocLd1;
GLint iLocLd2;
GLint iLocLd3;
Vector3 Ld1 = Vector3(1, 1, 1);
Vector3 Ld2 = Vector3(1, 1, 1);
Vector3 Ld3 = Vector3(1, 1, 1);
//light position(direction light, point/position light, spot light )
GLint iLocLp1;
GLint iLocLp2;
GLint iLocLp3;
Vector3 Lp1 = Vector3(1.0f, 1.0f, 1.0f);
Vector3 Lp2 = Vector3(0.0f, 2.0f, 1.0f);
Vector3 Lp3 = Vector3(0.0f, 0.0f, 2.0f);
//Perpixel light or Pervertex light
GLuint iLocperpix;
int perpix;
//lightidx(direction light, point/position light, spot light)
GLuint iLocLightIdx;
int light_idx = 0;
GLuint iLocspotcutoff;
float spotcutoff = 30;
GLuint iLocview;

void updatelight() {
	glUniform1f(iLocShininess, shininess);
	glUniform3f(iLocLd1, Ld1.x, Ld1.y, Ld1.z);
	glUniform3f(iLocLd2, Ld2.x, Ld2.y, Ld2.z);
	glUniform3f(iLocLd3, Ld3.x, Ld3.y, Ld3.z);
	glUniform3f(iLocLp1, Lp1.x, Lp1.y, Lp1.z);
	glUniform3f(iLocLp2, Lp2.x, Lp2.y, Lp2.z);
	glUniform3f(iLocLp3, Lp3.x, Lp3.y, Lp3.z);
	glUniform1i(iLocLightIdx, light_idx);
	glUniform1f(iLocspotcutoff, spotcutoff);
}

void set_basic_variables(GLuint p) {
	iLocmv = glGetUniformLocation(p, "mv");
	iLocLightIdx = glGetUniformLocation(p, "lightIdx");
	iLocKa = glGetUniformLocation(p, "Ka");
	iLocKd = glGetUniformLocation(p, "Kd");
	iLocKs = glGetUniformLocation(p, "Ks");
	iLocShininess = glGetUniformLocation(p, "shininess");
	iLocLd1 = glGetUniformLocation(p, "Ld1");
	iLocLd2 = glGetUniformLocation(p, "Ld2");
	iLocLd3 = glGetUniformLocation(p, "Ld3");
	iLocLp1 = glGetUniformLocation(p, "lightPos1");
	iLocLp2 = glGetUniformLocation(p, "lightPos2");
	iLocLp3 = glGetUniformLocation(p, "lightPos3");
	iLocspotcutoff = glGetUniformLocation(p, "spotCutoff");
	iLocview = glGetUniformLocation(p, "view_matrix");
	iLocperpix = glGetUniformLocation(p, "perpix");
}


static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;
	mat = Matrix4(
		1.0f, 0.0f, 0.0f, vec[0],
		0.0f, 1.0f, 0.0f, vec[1],
		0.0f, 0.0f, 1.0f, vec[2],
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		vec[0], 0.0f, 0.0f, 0.0f,
		0.0f, vec[1], 0.0f, 0.0f,
		0.0f, 0.0f, vec[2], 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}


// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cos(val), -sin(val), 0.0f,
		0.0f, sin(val), cos(val), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), 0.0f, sin(val), 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sin(val), 0.0f, cos(val), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f

	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;
	// same as 2d
	mat = Matrix4(
		cos(val), -sin(val), 0.0f, 0.0f,
		sin(val), cos(val), 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	// view_matrix[...] = ...
	/*
		reference https://www.geertarien.com/blog/2017/07/30/breakdown-of-the-lookAt-function-in-OpenGL/
	*/

	// view_matrix[...] = ...
	GLfloat x_dot_position = 0, y_dot_position = 0, z_dot_position = 0;
	GLfloat *up_vector = new GLfloat[3];
	GLfloat *zaxis = new GLfloat[3];
	GLfloat *xaxis = new GLfloat[3];
	GLfloat *yaxis = new GLfloat[3];
	zaxis[0] = main_camera.center[0] - main_camera.position[0];
	zaxis[1] = main_camera.center[1] - main_camera.position[1];
	zaxis[2] = main_camera.center[2] - main_camera.position[2];
	Normalize(zaxis);
	up_vector[0] = main_camera.up_vector[0];
	up_vector[1] = main_camera.up_vector[1];
	up_vector[2] = main_camera.up_vector[2];
	Cross(zaxis, up_vector, xaxis);
	Normalize(xaxis);
	Cross(xaxis, zaxis, yaxis);

	//negate the zaxis
	for (int i = 0; i < 3; i++)
	{
		zaxis[i] = -zaxis[i];
	}
	for (int i = 0; i < 3; i++)
	{
		x_dot_position += -(xaxis[i] * main_camera.position[i]);
		y_dot_position += -(yaxis[i] * main_camera.position[i]);
		z_dot_position += -(zaxis[i] * main_camera.position[i]);
	}

	view_matrix = Matrix4(xaxis[0], xaxis[1], xaxis[2], x_dot_position,
		yaxis[0], yaxis[1], yaxis[2], y_dot_position,
		zaxis[0], zaxis[1], zaxis[2], z_dot_position,
		0.0f, 0.0f, 0.0f, 1.0f);
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	// GLfloat f = ...
	// project_matrix [...] = ...
	// project_matrix [...] = ...
	GLfloat f = cos(deg2rad(proj.fovy) / 2) / sin(deg2rad(proj.fovy) / 2);
	//cout << f<<endl;
	project_matrix = Matrix4(
		f / proj.aspect, 0.0f, 0.0f, 0.0f,
		0.0f, f, 0.0f, 0.0f,
		0.0f, 0.0f, (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip), (2.0 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip),
		0.0f, 0.0f, -1.0f, 0.0f
	);
}

void setGLMatrix(GLfloat* glm, Matrix4& m) {
	glm[0] = m[0];  glm[4] = m[1];   glm[8] = m[2];    glm[12] = m[3];
	glm[1] = m[4];  glm[5] = m[5];   glm[9] = m[6];    glm[13] = m[7];
	glm[2] = m[8];  glm[6] = m[9];   glm[10] = m[10];   glm[14] = m[11];
	glm[3] = m[12];  glm[7] = m[13];  glm[11] = m[14];   glm[15] = m[15];
}

// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio
	proj.aspect = (float)(width/2) / (float)height;
	setPerspective();
	WINDOW_WIDTH = width;
	WINDOW_HEIGHT = height;
}

// Render function for display rendering
void RenderScene(void) {	
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP= project_matrix * view_matrix*T*R*S;
	GLfloat mvp[16];

	Matrix4 MV = view_matrix * T * R * S;
	GLfloat mv[16];

	// [TODO] multiply all the matrix
	// row-major ---> column-major
	setGLMatrix(mvp, MVP);
	setGLMatrix(mv, MV);

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(uniform.iLocMVP, 1, GL_FALSE, mvp);

	//use uniform to send light to vertex shader
	updatelight();

	//use uniform to send mv to vertex shader
	glUniformMatrix4fv(iLocmv, 1, GL_FALSE, mv);
	glUniformMatrix4fv(iLocview, 1, GL_FALSE, view_matrix.getTranspose());

	glViewport(0, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT);
	//Pervertex lighting
	for (int i = 0; i < models[cur_idx].shapes.size(); i++) 
	{
		// set glViewport and draw twice ... 
		perpix = 0;
		glUniform1i(iLocperpix, perpix);
		glUniform3fv(iLocKa, 1, &(models[cur_idx].shapes[i].material.Ka[0]));
		glUniform3fv(iLocKd, 1, &(models[cur_idx].shapes[i].material.Kd[0]));
		glUniform3fv(iLocKs, 1, &(models[cur_idx].shapes[i].material.Ks[0]));
		//cout << "Ka:"<<models[cur_idx].shapes[i].material.Ka<<"\n";
		//cout << "Kd:" << models[cur_idx].shapes[i].material.Kd << "\n";
		//cout << "Ks:" << models[cur_idx].shapes[i].material.Ks << "\n";
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}
	
	glViewport(WINDOW_WIDTH / 2, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT);
	//Perpixel lighting
	for (int i = 0; i < models[cur_idx].shapes.size(); i++)
	{
		// set glViewport and draw twice ... 
		perpix = 1;
		glUniform1i(iLocperpix, perpix);
		//glUniform3fv(iLocKa, 1, &(models[cur_idx].shapes[i].material.Ka[0]));
		//glUniform3fv(iLocKd, 1, &(models[cur_idx].shapes[i].material.Kd[0]));
		//glUniform3fv(iLocKs, 1, &(models[cur_idx].shapes[i].material.Ks[0]));
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}
	
}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if (action == GLFW_PRESS) {
		if (key== GLFW_KEY_ESCAPE)
		{
			exit(0);
		}
		if (key == GLFW_KEY_Z)
		{
			if (cur_idx == 0)
				cur_idx += 4;
			else {
				cur_idx--;
			}
		}
		if (key == GLFW_KEY_X)
		{
			if (cur_idx == 4)
				cur_idx -= 4;
			else {
				cur_idx++;
			}
		}
		if (key == GLFW_KEY_T) {
			cur_trans_mode = GeoTranslation;
		}
		if (key == GLFW_KEY_R) {
			cur_trans_mode = GeoRotation;
		}
		if (key == GLFW_KEY_S) {
			cur_trans_mode = GeoScaling;
		}
		if (key == GLFW_KEY_L) {
			light_idx += 1;
			if (light_idx>2)light_idx -= 3;
			if (light_idx == 0) cout << "Light Mode: " << "Directional Light" << endl;
			if (light_idx == 1) cout << "Light Mode: " << "Positional Light" << endl;
			if (light_idx == 2) cout << "Light Mode: " << "Spot Light" << endl;
		}
		if (key == GLFW_KEY_K) {
			cur_trans_mode = LightEdit;
		}
		if (key == GLFW_KEY_J) {
			cur_trans_mode = ShininessEdit;
		}
		if (key== GLFW_KEY_I)
		{
			cout << "Matrix Value : " << endl;
			cout << "Viewing Matrix :" << endl << view_matrix << endl;
			cout << "Projection Matrix :" << endl << project_matrix << endl;
			cout << "Translation Matrix :" << endl << translate(models[cur_idx].position) << endl;
			cout << "Rotation Matrix :" << endl << rotate(models[cur_idx].rotation) << endl;
			cout << "Scaling Matrix :" << endl << scaling(models[cur_idx].scale) << endl;
			cout << "Light Mode: " << light_idx << endl;
			cout << "Shininess: " << shininess << endl;
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be nagetive
	if (yoffset < 0)
	{
		switch (cur_trans_mode) {
		case GeoTranslation:
			models[cur_idx].position.z -= 0.1;
			break;
		case GeoRotation:
			//每次轉3弧度
			models[cur_idx].rotation.z -= (PI / 180.0) * 3;
			break;
		case GeoScaling:
			models[cur_idx].scale.z -= 0.01;
			break;
		case LightEdit:
			if (light_idx==0)
			{
				Ld1 -= Vector3(0.05f, 0.05f, 0.05f);
			}
			else if (light_idx==1)
			{
				Ld2-= Vector3(0.05f, 0.05f, 0.05f);
			}
			else
			{
				if (spotcutoff <= 180 and spotcutoff >=0)
				{
					spotcutoff -= 5;
					//cout << spotcutoff << endl;
				}
			}
			break;
		case ShininessEdit:
			shininess -= 1;
			//cout << "shininess:" << shininess << endl;
			break;
		default:
			break;
		}

	}
	else
	{
		switch (cur_trans_mode) {
		case GeoTranslation:
			models[cur_idx].position.z += 0.1;
			break;
		case GeoRotation:
			//每次轉3弧度
			models[cur_idx].rotation.z += (PI / 180.0) * 3;
			break;
		case GeoScaling:
			models[cur_idx].scale.z += 0.01;
			break;
		case LightEdit:
			if (light_idx == 0)
			{
				Ld1 += Vector3(0.05f, 0.05f, 0.05f);
			}
			else if (light_idx == 1)
			{
				Ld2 += Vector3(0.05f, 0.05f, 0.05f);
			}
			else
			{
				if (spotcutoff<=180 and spotcutoff >=0)
				{
					spotcutoff += 5;
					//cout << spotcutoff << endl;
				}
			
			}
			break;
		case ShininessEdit:
			shininess += 1;
			//cout << "shininess:" << shininess << endl;
			break;
		default:
			break;
		}
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	double xpos, ypos;

	if (action == GLFW_PRESS) {
		switch (button) {
		case GLFW_MOUSE_BUTTON_RIGHT:
			//此api的原點為upper-left corner
			glfwGetCursorPos(window, &xpos, &ypos);
			current_x = xpos;
			current_y = ypos;
			break;

		case GLFW_MOUSE_BUTTON_LEFT:
			glfwGetCursorPos(window, &xpos, &ypos);
			current_x = xpos;
			current_y = ypos;
			//cout << "current_x:" << current_x << endl << "current_y" << current_y << endl;
			break;

		default:
			break;
		}

	}
		
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	diff_x = xpos - current_x;
	diff_y = ypos - current_y;
	current_x = xpos;
	current_y = ypos;

	int left_mouse_click = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	int right_mouse_click = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

	if (left_mouse_click == GLFW_PRESS || right_mouse_click == GLFW_PRESS)
	{
		switch (cur_trans_mode) {
		case GeoTranslation:
			models[cur_idx].position.x += diff_x * 0.0025;
			models[cur_idx].position.y -= diff_y * 0.0025;
			break;

		case GeoScaling:
			models[cur_idx].scale.x += diff_x * 0.025;
			models[cur_idx].scale.y += diff_y * 0.025;
			break;

		case GeoRotation:
			models[cur_idx].rotation.x += PI / 180.0*diff_y*(0.125);
			models[cur_idx].rotation.y += PI / 180.0*diff_x*(0.125);
			break;
		case LightEdit:
			Lp1[0]+= diff_x * 0.0025;
			Lp1[1] -= diff_y * 0.0025;
			Lp2[0] += diff_x * 0.0025;
			Lp2[1] -= diff_y * 0.0025;
			Lp3[0] += diff_x * 0.0025;
			Lp3[1] -= diff_y * 0.0025;
			break;
		default:
			break;

		}
	}
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p,f);
	glAttachShader(p,v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	//combine variable to shader uniform
	uniform.iLocMVP = glGetUniformLocation(p, "mvp");
	set_basic_variables(p);

	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, vector<GLfloat>& normals, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0) {
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals;

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", int(shapes.size()), int(materials.size()));
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	// [TODO] Setup some parameters if you need
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH/2 / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{ "../NormalModels/bunny5KN.obj", "../NormalModels/dragon10KN.obj", "../NormalModels/lucy25KN.obj", "../NormalModels/teapot4KN.obj", "../NormalModels/dolphinN.obj"};
	// [TODO] Load five model at here
	for (int i = 0; i < 5; i++)
	{
		LoadModels(model_list[i]);
	}
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
    // initial glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

    
    // create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "110065521 HW2", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    
    // load OpenGL function pointer
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
	// register glfw callback functions
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
    while (!glfwWindowShouldClose(window))
    {
        // render
        RenderScene();
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}
