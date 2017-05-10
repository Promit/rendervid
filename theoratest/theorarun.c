#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include "theorarun.h"
#include "theora/theoradec.h"
#include "vorbis/codec.h"



#define THEORARUN_INTERNAL 1

typedef THEORARUN_VideoFrame VideoFrame;
typedef THEORARUN_AudioPacket AudioPacket;

typedef struct TheoraDecoder
{

	// API state...
	THEORARUN_Io *io;
	th_info *tinfo;
	unsigned int maxframes;  // Max video frames to buffer.
	volatile unsigned int prepped;
	volatile unsigned int videocount;  // currently buffered frames.
	volatile unsigned int audioms;  // currently buffered audio samples.
	volatile int hasvideo;
	volatile int hasaudio;
	volatile int decode_error;

	THEORARUN_VideoFormat vidfmt;
	//ConvertVideoFrameFn vidcvt;

	VideoFrame *videolist;
	VideoFrame *videolisttail;

	AudioPacket *audiolist;
	AudioPacket *audiolisttail;
} TheoraDecoder;

static long IoFopenRead(THEORARUN_Io *io, void *buf, long buflen)
{
	FILE *f = (FILE *)io->userdata;
	const size_t br = fread(buf, 1, buflen, f);
	if ((br == 0) && ferror(f))
		return -1;
	return (long)br;
} // IoFopenRead


static void IoFopenClose(THEORARUN_Io *io)
{
	FILE *f = (FILE *)io->userdata;
	fclose(f);
	free(io);
} // IoFopenClose

static THEORARUN_Io* open(const char *filename) {
	THEORARUN_Io *io = (THEORARUN_Io *)malloc(sizeof(THEORARUN_Io));
	if (io == NULL)
		return NULL;

	FILE *f = fopen(filename, "rb");
	if (f == NULL)
	{
		free(io);
		return NULL;
	}
	io->read = IoFopenRead;
	io->close = IoFopenClose;
	io->userdata = f;
	return io;
}

static int getWidth(TheoraDecoder *ctx) { //th_info *tinfo) {
	return ctx->tinfo->pic_width;
}

static int getHeight(TheoraDecoder *ctx) {
	return ctx->tinfo->pic_height;
}

static THEORARUN_VideoFormat getFormat(TheoraDecoder *ctx) {
	return ctx->vidfmt;
}

static void init_decoder(THEORARUN_Io *io, const unsigned int maxframes, THEORARUN_VideoFormat vidfmt) {
	TheoraDecoder *ctx = NULL;
	//ConvertVideoFrameFn vidcvt = NULL;

	/*switch (vidfmt)
	{
			// !!! FIXME: current expects TH_PF_420.
		#define VIDCVT(t) case THEORARUN_VIDFMT_##t: vidcvt = ConvertVideoFrame420To##t; break;
		VIDCVT(YV12)
		VIDCVT(IYUV)
		VIDCVT(RGB)
		VIDCVT(RGBA)
	#undef VIDCVT
		default: goto startdecode_failed;  // invalid/unsupported format.
	} // */

	ctx = (TheoraDecoder *)malloc(sizeof(TheoraDecoder));
	//if (ctx == NULL)
		//std::cout << "error" << std::endl;
		//goto startdecode_failed;

	memset(ctx, '\0', sizeof(TheoraDecoder));
	ctx->maxframes = maxframes;
	ctx->vidfmt = vidfmt;
	//ctx->vidcvt = vidcvt;
	ctx->io = io;
}
static void init() {
	th_info tinfo;
	th_info_init(&tinfo);
}

