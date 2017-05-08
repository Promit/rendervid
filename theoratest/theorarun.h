/**
* TheoraRun; multithreaded Ogg Theora/Ogg Vorbis decoding.
*/
#ifndef _INCL_THEORARUN_H_
#define _INCL_THEORARUN_H_

#ifdef __cplusplus
extern "C" {
#endif
	typedef struct THEORARUN_Io THEORARUN_Io;
	struct THEORARUN_Io
	{
		long(*read)(THEORARUN_Io *io, void *buf, long buflen);
		void(*close)(THEORARUN_Io *io);
		void *userdata;
	};

	typedef struct THEORARUN_Decoder THEORARUN_Decoder;

	/* YV12 is YCrCb, not YCbCr; that's what SDL uses for YV12 overlays. */
	typedef enum THEORARUN_VideoFormat
	{
		THEORARUN_VIDFMT_YV12,  /* NTSC colorspace, planar YCrCb 4:2:0 */
		THEORARUN_VIDFMT_IYUV,  /* NTSC colorspace, planar YCbCr 4:2:0 */
		THEORARUN_VIDFMT_RGB,   /* 24 bits packed pixel RGB */
		THEORARUN_VIDFMT_RGBA   /* 32 bits packed pixel RGBA (full alpha). */
	} THEORARUN_VideoFormat;

	typedef struct THEORARUN_VideoFrame
	{
		unsigned int playms;
		double fps;
		unsigned int width;
		unsigned int height;
		THEORARUN_VideoFormat format;
		unsigned char *pixels;
		struct THEORARUN_VideoFrame *next;
	} THEORARUN_VideoFrame;

	typedef struct THEORARUN_AudioPacket
	{
		unsigned int playms;  /* playback start time in milliseconds. */
		int channels;
		int freq;
		int frames;
		float *samples;  /* frames * channels float32 samples. */
		struct THEORARUN_AudioPacket *next;
	} THEORARUN_AudioPacket;

	static THEORARUN_Io* open(const char *filename);
	/*static int getWidth(TheoraDecoder *ctxt);
	static int getHeight(TheoraDecoder *ctxt);*/

#ifdef __cplusplus
}
#endif

#endif  /* include-once blocker. */

/* end of THEORARUN.h ... */
