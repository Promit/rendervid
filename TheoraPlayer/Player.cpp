#include <cstdio>
#include <cstring>
#include <cassert>
#include <chrono>
#include <thread>
#include "TheoraPlayer.h"

#ifndef GLEW_STATIC
//GLEW
#define GLEW_STATIC
#include <GL/glew.h>
#endif

// GLFW
#include <GLFW/glfw3.h>
#include "shader.h"

static long long baseticks = 0;


static GLFWwindow* window;
static int setup() {
	// initialize glfw library
	glfwInit();
	// set hints before create window - first param is desired option, second is desired value of option
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	// create window --> (width, height, windowName, null for windowed mode, null to not share resources)
	window = glfwCreateWindow(800, 600, "TheoraPlayer", nullptr, nullptr);
	if(window == nullptr)
	{
		printf("Failed to create GLFW window\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	// initialize glew
	glewExperimental = GL_TRUE;
	if(glewInit() != GLEW_OK)
	{
		printf("Failed to initialize GLEW\n");
		return -1;
	}
	//set size of rendering window
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	// set viewport (x coordinate of bottom left corner, y bottom left coordinate, width, height)
	glViewport(0, 0, width, height);
	return 0;
}

static GLuint texture;
static void gen_texture(const THEORAPLAYER_VideoFrame *video) {
	glEnable(GL_TEXTURE_2D);
	//generate texture
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	unsigned char* rgb = video->pixels;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Load and generate the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, video->width, video->height, 0, GL_BGR, GL_UNSIGNED_BYTE, rgb);
	glBindTexture(GL_TEXTURE_2D, 0);
}

static long long getTime() {
	using namespace std::chrono;
	milliseconds ms = duration_cast< milliseconds >(
		system_clock::now().time_since_epoch()
		);
	return ms.count();
}

static GLuint setupBindings(Shader ourShader) {
	GLfloat vertices[] = {
		// Positions          // Colors           // Texture Coords
		1.f,  1.f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f,   // Top Right
		1.f, -1.f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,   // Bottom Right
		-1.f, -1.f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   // Bottom Left
		-1.f,  1.f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 0.0f    // Top Left 
	};
	GLuint indices[] = {  // Note that we start from 0!
		0, 1, 3, // First Triangle
		1, 2, 3  // Second Triangle
	};

	// enable attributes
	glEnableVertexAttribArray(0);

	GLuint VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
	// 1. Bind Vertex Array Object
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	// 2. Copy our vertices array in a buffer for OpenGL to use
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	// 3. Then set our vertex attributes pointers
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	//Color attributes
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	// TextCoord atributes
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);
	//4. Unbind the VAO
	glBindVertexArray(0);
	return VAO;
}

static void game_loop(Shader ourShader, const THEORAPLAYER_VideoFrame *video, GLuint VAO) {
	glfwPollEvents(); // checks if any events are triggered and calls the corresponding functions
					  // Rendering commands
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // sets the color used for glClear
	glClear(GL_COLOR_BUFFER_BIT); // clears the entire buffer (can be color, depth, and/or stencil)
								  // Draw traingle
								  //glUseProgram(ourShader);
	if(video)
		gen_texture(video);

	ourShader.Use();
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	glfwSwapBuffers(window);
}

static void playfile(const char *fname)
{
	using namespace std::chrono_literals;

	TheoraPlayer player;
	//const THEORAPLAYER_AudioPacket *audio = NULL;
	int quit = 0;
	if(setup() != 0) {
		return;
	}
	Shader ourShader("texture.vs", "texture.frag");
	GLuint VAO = setupBindings(ourShader);

	printf("Trying file '%s' ...\n", fname);

	auto result = player.OpenDecode(fname, THEORAPLAYER_VIDFMT_BGR);
	if(!result)
	{
		printf("Failed to open decoding '%s'!\n", fname);
		return;
	} // if

	result = player.Prepare();
	if(!result)
	{
		printf("Failed to parse input file.\n");
		return;
	}

	THEORAPLAYER_VideoFrame* video = new THEORAPLAYER_VideoFrame();
	player.GetVideoFrame(video);
	const unsigned int framems = (video->fps == 0.0) ? 0 : ((GLuint)(1000.0 / video->fps));
	if(!window)
	{
		std::cout << "ERROR" << std::endl;
	}

	baseticks = getTime();
	while(!quit && player.IsDecoding())
	{
		const long long now = getTime() - baseticks;

		// Play video frames when it's time.
		if (video->playms <= now)
		{
			if (framems && ((now - video->playms) >= framems))
			{
				// Skip frames to catch up
				while (player.GetVideoFrame(video) >= 0)
				{
					if ((now - video->playms) < framems)
						break;
				} // while
			} // if

			game_loop(ourShader, video, VAO);
			player.GetVideoFrame(video);
		}
		else
		{
			std::this_thread::sleep_for(1ms);
		}
	}
} // playfile

int main(int argc, char **argv)
{
	int i;
	for(i = 1; i < argc; i++)
		playfile(argv[i]);

	printf("done all files!\n");

	return 0;
} // main
