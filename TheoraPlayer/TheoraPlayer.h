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

struct THEORAPLAYER_VideoFrame
{
	unsigned int playms;
	double fps;
	unsigned int width;
	unsigned int height;
	THEORAPLAYER_VideoFormat format;
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

	int OpenDecode(const char* filename, THEORAPLAYER_VideoFormat outputFormat);
	int OpenDecode(THEORAPLAYER_Io* io, THEORAPLAYER_VideoFormat outputFormat);
	
	int Prepare();
	int IsDecoding() const;
	int GetVideoFrame(THEORAPLAYER_VideoFrame* frame);

private:
	struct THEORAPLAYER_Decoder* _decoder = nullptr;
	struct THEORAPLAYER_State* _state = nullptr;
	THEORAPLAYER_Io* _io = nullptr;
};

#endif
