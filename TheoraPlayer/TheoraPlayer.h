// Copyright (c) Promit Roy.
// All rights reserved.
//
// This code is licensed under the MIT License.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//This code is derived from Ryan C. Gordon's TheoraPlay:
//https://www.icculus.org/theoraplay/
//This is mostly a C++ rework, cleaned up for general ease of use, and with threading delegated to the caller

#ifndef THEORAPLAYER_H
#define THEORAPLAYER_H
#pragma once

struct THEORAPLAYER_Io
{
	size_t(*read)(THEORAPLAYER_Io *io, void *buf, long buflen);
	void(*close)(THEORAPLAYER_Io *io);
	void *userdata;
};

/* YV12 is YCrCb, not YCbCr; that's what SDL uses for YV12 overlays. */
enum THEORAPLAYER_VideoFormat
{
	THEORAPLAYER_VIDFMT_YV12,  /* NTSC colorspace, planar YCrCb 4:2:0 */
	THEORAPLAYER_VIDFMT_IYUV,  /* NTSC colorspace, planar YCbCr 4:2:0 */
	THEORAPLAYER_VIDFMT_RGB,   /* 24 bits packed pixel RGB */
	THEORAPLAYER_VIDFMT_RGBA,   /* 32 bits packed pixel RGBA (full alpha). */
	THEORAPLAYER_VIDFMT_BGR,   /* 24 bits packed pixel BGR */
	THEORAPLAYER_VIDFMT_BGRA   /* 32 bits packed pixel BGRA (full alpha). */
};

//Structure to hold one video frame, both metadata and pixel data
struct THEORAPLAYER_VideoFrame
{
	//The timestamp of this frame
	unsigned int playms;
	//Playback framerate for this frame
	double fps;
	//Image width of this frame
	unsigned int width;
	//Image height of this frame
	unsigned int height;
	//Pixel format of this frame
	THEORAPLAYER_VideoFormat format;
	//Pixel data of this frame (owned by this struct)
	unsigned char *pixels;
};

struct THEORAPLAYER_AudioPacket
{
	unsigned int playms;  /* playback start time in milliseconds. */
	int channels;
	int freq;
	int frames;
	float *samples;  /* frames * channels float32 samples. */
	struct THEORAPLAYER_AudioPacket *next;
};

class TheoraPlayer
{
public:
	TheoraPlayer();
	~TheoraPlayer();

	//Open a video file by name for decode to the specified output format
	int OpenDecode(const char* filename, THEORAPLAYER_VideoFormat outputFormat);
	//Open a video file with user-supplied IO for decode to the specified output format
	int OpenDecode(THEORAPLAYER_Io* io, THEORAPLAYER_VideoFormat outputFormat);
	
	//Begin decoding from the start of the video
	int Prepare();
	//True if we are currently in the midst of decoding this video and not at the end of the stream
	int IsDecoding() const;
	//Decode the next frame and save the data to the supplied frame. If the frame does not have pixel data, one will be allocated.
	int GetVideoFrame(THEORAPLAYER_VideoFrame* frame);
	//Free the previously allocated pixel data inside this frame.
	void FreeFrameData(THEORAPLAYER_VideoFrame* frame);

private:
	struct THEORAPLAYER_Decoder* _decoder = nullptr;
	struct THEORAPLAYER_State* _state = nullptr;
	THEORAPLAYER_Io* _io = nullptr;
};

#endif
