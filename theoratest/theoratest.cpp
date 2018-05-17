/**
* TheoraPlay; multithreaded Ogg Theora/Ogg Vorbis decoding.
*
* Please see the file LICENSE.txt in the source's root directory.
*
*  This file written by Ryan C. Gordon.
*/

/*
* This is meant to be a dirt simple test case. If you want a big, robust
*  version that handles lots of strange variations, try sdltheoraplay.c
*/
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <chrono>
#include "theoraplay.h"

#ifndef GLEW_STATIC
//GLEW
#define GLEW_STATIC
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>
#include <iostream>
#endif
#include "shader.h"
//static GLuint baseticks = 0;
static long long baseticks = 0;
FILE _iob[] = { *stdin, *stdout, *stderr };

extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}


typedef struct AudioQueue
{
	const THEORAPLAY_AudioPacket *audio;
	int offset;
	struct AudioQueue *next;
} AudioQueue;

static volatile AudioQueue *audio_queue = NULL;
static volatile AudioQueue *audio_queue_tail = NULL;

//static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len)
static void audio_callback(void *userdata, unsigned __int8 *stream, int len)
{
	// !!! FIXME: this should refuse to play if item->playms is in the future.
	//const Uint32 now = SDL_GetTicks() - baseticks;
	_int16 *dst = (_int16 *)stream;

	while(audio_queue && (len > 0))
	{
		volatile AudioQueue *item = audio_queue;
		AudioQueue *next = item->next;
		const int channels = item->audio->channels;

		const float *src = item->audio->samples + (item->offset * channels);
		int cpy = (item->audio->frames - item->offset) * channels;
		int i;

		if(cpy > (len / sizeof(_int16)))
			cpy = len / sizeof(_int16);

		for(i = 0; i < cpy; i++)
		{
			const float val = *(src++);
			if(val < -1.0f)
				*(dst++) = -32768;
			else if(val > 1.0f)
				*(dst++) = 32767;
			else
				*(dst++) = (_int16)(val * 32767.0f);
		} // for

		item->offset += (cpy / channels);
		len -= cpy * sizeof(_int16);

		if(item->offset >= item->audio->frames)
		{
			THEORAPLAY_freeAudio(item->audio);
			free((void *)item);
			audio_queue = next;
		} // if
	} // while

	if(!audio_queue)
		audio_queue_tail = NULL;

	if(len > 0)
		memset(dst, '\0', len);
} // audio_callback


static void queue_audio(const THEORAPLAY_AudioPacket *audio)
{
	AudioQueue *item = (AudioQueue *)malloc(sizeof(AudioQueue));
	if(!item)
	{
		THEORAPLAY_freeAudio(audio);
		return;  // oh well.
	} // if

	item->audio = audio;
	item->offset = 0;
	item->next = NULL;

	//SDL_LockAudio();
	if(audio_queue_tail)
		audio_queue_tail->next = item;
	else
		audio_queue = item;
	audio_queue_tail = item;
	//SDL_UnlockAudio();
} // queue_audio

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
	window = glfwCreateWindow(800, 600, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	// initialize glew
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cout << "Failed to initialize GLEW" << std::endl;
		return -1;
	}
	//set size of rendering window
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	// set viewport (x coordinate of bottom left corner, y bottom left coordinate, width, height)
	glViewport(0, 0, width, height);
	return 0;
}

void YCbCrToRgb(const unsigned char y, const unsigned char cb, const unsigned char cr, unsigned char* rout, unsigned char* gout, unsigned char* bout)
{
	double r = y +(1.4065 * (cr - 128));
	double g = y -(0.3455 * (cb - 128)) - (0.7169 * (cr - 128));
	double b = y +(1.7790 * (cb - 128));
	//double r = cb;
	//double g = cb;
	//double b = cb;

	//To prevent colour distortions
	if (r < 0) r = 0;
	else if (r > 255) r = 255;
	if (g < 0) g = 0;
	else if (g > 255) g = 255;
	if (b < 0) b = 0;
	else if (b > 255) b = 255;

	*rout = r;
	*gout = g;
	*bout = b;
}

static unsigned char * convertToRGB(unsigned char *yuyv_image, int width, int height) {
	//same width and height of the image to be converted
	unsigned char* rgb_image = new unsigned char[width * height * 3];

	//Test/debug code that simply copies the YUV channels over to RGB
	/*memset(rgb_image, 0, width*height * 3);
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			int offset = y * width + x;
			rgb_image[offset * 3 + 0] = yuyv_image[offset];
			rgb_image[offset * 3 + 1] = yuyv_image[offset];
			rgb_image[offset * 3 + 2] = yuyv_image[offset];
		}
	}
	return rgb_image;*/

	int y;
	int cr;
	int cb;

	double r;
	double g;
	double b;
	
	unsigned char* yptr = yuyv_image;
	unsigned char* cbptr = yptr + width * height;
	unsigned char* crptr = cbptr + (width / 2) * (height / 2);
	for (int imgx = 0; imgx < width; ++imgx)
	{
		for (int imgy = 0; imgy < height; ++imgy)
		{
			int offset = imgy * width + imgx;
			int cbcroffset = (imgy / 2) * (width / 2) + (imgx / 2);
			int rgboffset = offset * 3;
			unsigned char y = yptr[offset];
			unsigned char cb = cbptr[cbcroffset];
			unsigned char cr = crptr[cbcroffset];
			YCbCrToRgb(y, cb, cr, rgb_image + rgboffset, rgb_image + rgboffset + 1, rgb_image + rgboffset + 2);
		}
	}
	return rgb_image;
}

static GLuint texture;
static void gen_texture(const THEORAPLAY_VideoFrame *video) {
	glEnable(GL_TEXTURE_2D);
	//generate texture
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
 	//unsigned char *rgb = convertToRGB(video->pixels, video->width, video->height);
	unsigned char* rgb = video->pixels;
	//memset(rgb, 255, video->width * video->height * 3);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Load and generate the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, video->width, video->height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
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
	//gen_texture();
	// tell how vertex data is organized
	// (index of vertex attribute we want to configure, size of vertex attribute, data type, if the data should be normalized, stride - pace between consecutive vertex attribute sets, offset of first component)
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
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

//static GLuint VAO = 0, VBO = 0, EBO = 0;
//static const THEORAPLAY_VideoFrame *video = NULL;
static void game_loop(Shader ourShader, const THEORAPLAY_VideoFrame *video, GLuint VAO) {
	glfwPollEvents(); // checks if any events are triggered and calls the corresponding functions
					  // Rendering commands
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // sets the color used for glClear
	glClear(GL_COLOR_BUFFER_BIT); // clears the entire buffer (can be color, depth, and/or stencil)
								  // Draw traingle
								  //glUseProgram(ourShader);
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
	THEORAPLAY_Decoder *decoder = NULL;
	const THEORAPLAY_AudioPacket *audio = NULL;
	const THEORAPLAY_VideoFrame *video = NULL;
	GLuint framems = 0;
	int initfailed = 0;
	int quit = 0;
	if (setup() != 0) {
		return;
	} 
	Shader ourShader("texture.vs", "texture.frag");
	GLuint VAO = setupBindings(ourShader);
	
	printf("Trying file '%s' ...\n", fname);

	decoder = THEORAPLAY_startDecodeFile(fname, 30, THEORAPLAY_VIDFMT_RGB);
	if(!decoder)
	{
		fprintf(stderr, "Failed to start decoding '%s'!\n", fname);
		return;
	} // if

	  // wait until we have video and/or audio data, so we can set up hardware.
	while(!audio || !video)
	{
		if(!audio) audio = THEORAPLAY_getAudio(decoder);
		if(!video) video = THEORAPLAY_getVideo(decoder);
		//SDL_Delay(10);
	} // if
	
	framems = (video->fps == 0.0) ? 0 : ((GLuint)(1000.0 / video->fps));
	if(!window)
	{
		std::cout << "ERROR" << std::endl;// fprintf(stderr, "SDL_CreateWindow() failed: %s\n", SDL_GetError());
	}
	while(audio)
	{
		queue_audio(audio);
		audio = THEORAPLAY_getAudio(decoder);
	} // while

	baseticks = getTime();
	while(!quit && THEORAPLAY_isDecoding(decoder))
	{
		//const Uint32 now = SDL_GetTicks() - baseticks;
		const long long now = getTime() - baseticks;

		if(!video)
			video = THEORAPLAY_getVideo(decoder);

		// Play video frames when it's time.
		if(video && (video->playms <= now))
		{
			//printf("Play video frame (%u ms)!\n", video->playms);
			if(framems && ((now - video->playms) >= framems))
			{
				// Skip frames to catch up, but keep track of the last one
				//  in case we catch up to a series of dupe frames, which
				//  means we'd have to draw that final frame and then wait for
				//  more.
				const THEORAPLAY_VideoFrame *last = video;
				while((video = THEORAPLAY_getVideo(decoder)) != NULL)
				{
					THEORAPLAY_freeVideo(last);
					last = video;
					if((now - video->playms) < framems)
						break;
				} // while

				if(!video)
					video = last;
			} // if

			if(!video)  // do nothing; we're far behind and out of options.
			{
				static int warned = 0;
				if(!warned)
				{
					warned = 1;
					fprintf(stderr, "WARNING: Playback can't keep up!\n");
				} // if
			} // if
			else
			{
				game_loop(ourShader, video, VAO);
			} // else

			THEORAPLAY_freeVideo(video);
			video = NULL;
		} // if
		else  // no new video frame? Give up some CPU.
		{
			//SDL_Delay(10);
		} // else
	}

	if(initfailed)
		printf("Initialization failed!\n");
	else if(THEORAPLAY_decodingError(decoder))
		printf("There was an error decoding this file!\n");
	else
		printf("done with this file!\n");

} // playfile

int main(int argc, char **argv)
{
	int i;
	for(i = 1; i < argc; i++)
		playfile(argv[i]);

	printf("done all files!\n");

	return 0;
} // main
