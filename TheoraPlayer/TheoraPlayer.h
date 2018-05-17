#pragma once

#include <memory>

struct THEORAPLAYER_Io
{
	long(*read)(THEORAPLAYER_Io *io, void *buf, long buflen);
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
	struct THEORAPLAYER_VideoFrame *next;
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
	int GetVideoFrame(THEORAPLAYER_VideoFrame** frame);

private:
	std::unique_ptr<struct THEORAPLAYER_Decoder> _decoder;
	std::unique_ptr<struct THEORAPLAYER_State> _state;
	std::unique_ptr<THEORAPLAYER_Io> _io;
};