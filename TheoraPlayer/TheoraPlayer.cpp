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

#include "TheoraPlayer.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <utility>

#include "theora/theoradec.h"
#include "vorbis/codec.h"

#define THEORAPLAY_INTERNAL 1

typedef THEORAPLAYER_VideoFrame VideoFrame;
typedef THEORAPLAYER_AudioPacket AudioPacket;

// !!! FIXME: these all count on the pixel format being TH_PF_420 for now.
typedef void (*ConvertVideoFrameFn)(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels);

static void ConvertVideoFrame420ToYUVPlanar(const th_info *tinfo, const th_ycbcr_buffer ycbcr, const int p0, const int p1, const int p2, unsigned char* yuv)
{
	assert(tinfo);
	assert(yuv);

	//output size is w * h * 2
	int i;
	const int w = tinfo->pic_width;
	const int h = tinfo->pic_height;
	const int yoff = (tinfo->pic_x & ~1) + ycbcr[0].stride * (tinfo->pic_y & ~1);
	const int uvoff = (tinfo->pic_x / 2) + (ycbcr[1].stride) * (tinfo->pic_y / 2);
	if(yuv)
	{
		unsigned char *dst = yuv;
		for(i = 0; i < h; i++, dst += w)
			memcpy(dst, ycbcr[p0].data + yoff + ycbcr[p0].stride * i, w);
		for(i = 0; i < (h / 2); i++, dst += w / 2)
			memcpy(dst, ycbcr[p1].data + uvoff + ycbcr[p1].stride * i, w / 2);
		for(i = 0; i < (h / 2); i++, dst += w / 2)
			memcpy(dst, ycbcr[p2].data + uvoff + ycbcr[p2].stride * i, w / 2);
	} // if
} // ConvertVideoFrame420ToYUVPlanar


static void ConvertVideoFrame420ToYV12(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels)
{
	return ConvertVideoFrame420ToYUVPlanar(tinfo, ycbcr, 0, 2, 1, pixels);
} // ConvertVideoFrame420ToYV12


static void ConvertVideoFrame420ToIYUV(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels)
{
	return ConvertVideoFrame420ToYUVPlanar(tinfo, ycbcr, 0, 1, 2, pixels);
} // ConvertVideoFrame420ToIYUV

static void THEORAPLAY_CVT_420_RGB(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels, THEORAPLAYER_VideoFormat format)
{
	assert(tinfo);
	assert(pixels);

	const int w = tinfo->pic_width;
	const int h = tinfo->pic_height;

	unsigned char *dst = pixels;
	const int ystride = ycbcr[0].stride;
	const int cbstride = ycbcr[1].stride;
	const int crstride = ycbcr[2].stride;
	const int yoff = (tinfo->pic_x & ~1) + ystride * (tinfo->pic_y & ~1);
	const int cboff = (tinfo->pic_x / 2) + (cbstride) * (tinfo->pic_y / 2);
	const unsigned char *py = ycbcr[0].data + yoff;
	const unsigned char *pcb = ycbcr[1].data + cboff;
	const unsigned char *pcr = ycbcr[2].data + cboff;
	int posx, posy;

	for(posy = 0; posy < h; posy++)
	{
		for(posx = 0; posx < w; posx++)
		{
			// http://www.theora.org/doc/Theora.pdf, 1.1 spec,
			//  chapter 4.2 (Y'CbCr -> Y'PbPr -> R'G'B')
			// These constants apparently work for NTSC _and_ PAL/SECAM.
			const float yoffset = 16.0f;
			const float yexcursion = 219.0f;
			const float cboffset = 128.0f;
			const float cbexcursion = 224.0f;
			const float croffset = 128.0f;
			const float crexcursion = 224.0f;
			const float kr = 0.299f;
			const float kb = 0.114f;

			const float y = (((float)py[posx]) - yoffset) / yexcursion;
			const float pb = (((float)pcb[posx / 2]) - cboffset) / cbexcursion;
			const float pr = (((float)pcr[posx / 2]) - croffset) / crexcursion;
			const float r = (y + (2.0f * (1.0f - kr) * pr)) * 255.0f;
			const float g = (y - ((2.0f * (((1.0f - kb) * kb) / ((1.0f - kb) - kr))) * pb) - ((2.0f * (((1.0f - kr) * kr) / ((1.0f - kb) - kr))) * pr)) * 255.0f;
			const float b = (y + (2.0f * (1.0f - kb) * pb)) * 255.0f;

			*(dst++) = (unsigned char)((r < 0.0f) ? 0.0f : (r > 255.0f) ? 255.0f : r);
			*(dst++) = (unsigned char)((g < 0.0f) ? 0.0f : (g > 255.0f) ? 255.0f : g);
			*(dst++) = (unsigned char)((b < 0.0f) ? 0.0f : (b > 255.0f) ? 255.0f : b);
			if(format == THEORAPLAYER_VIDFMT_BGR || format == THEORAPLAYER_VIDFMT_BGRA)
				std::swap(*(dst - 1), *(dst - 3));
			if(format == THEORAPLAYER_VIDFMT_RGBA || format == THEORAPLAYER_VIDFMT_BGRA)
				*(dst++) = 0xFF;
		} // for

			// adjust to the start of the next line.
		py += ystride;
		pcb += cbstride * (posy % 2);
		pcr += crstride * (posy % 2);
	} // for
}

 // RGB
static void ConvertVideoFrame420ToRGB(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels)
{
	THEORAPLAY_CVT_420_RGB(tinfo, ycbcr, pixels, THEORAPLAYER_VIDFMT_RGB);
}

// RGBA
static void ConvertVideoFrame420ToRGBA(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels)
{
	THEORAPLAY_CVT_420_RGB(tinfo, ycbcr, pixels, THEORAPLAYER_VIDFMT_RGBA);
}

 // BGR
static void ConvertVideoFrame420ToBGR(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels)
{
	THEORAPLAY_CVT_420_RGB(tinfo, ycbcr, pixels, THEORAPLAYER_VIDFMT_BGR);
}

 // BGRA
static void ConvertVideoFrame420ToBGRA(const th_info *tinfo, const th_ycbcr_buffer ycbcr, unsigned char* pixels)
{
	THEORAPLAY_CVT_420_RGB(tinfo, ycbcr, pixels, THEORAPLAYER_VIDFMT_BGRA);
}


struct THEORAPLAYER_Decoder
{
	// API state...
	THEORAPLAYER_Io *io;
	unsigned int audioms;  // currently buffered audio samples.
	int decode_error;

	THEORAPLAYER_VideoFormat vidfmt;
	ConvertVideoFrameFn vidcvt;

	~THEORAPLAYER_Decoder()
	{
		io->close(io);
	}
};

static int FeedMoreOggData(THEORAPLAYER_Io *io, ogg_sync_state *sync)
{
	long buflen = 4096;
	char *buffer = ogg_sync_buffer(sync, buflen);
	if(buffer == NULL)
		return -1;

	buflen = io->read(io, buffer, buflen);
	if(buflen <= 0)
		return 0;

	return (ogg_sync_wrote(sync, buflen) == 0) ? 1 : -1;
} // FeedMoreOggData


struct THEORAPLAYER_State
{
	~THEORAPLAYER_State()
	{
		if(tdec != NULL) th_decode_free(tdec);
		if(tsetup != NULL) th_setup_free(tsetup);
		if(vblock_init) vorbis_block_clear(&vblock);
		if(vdsp_init) vorbis_dsp_clear(&vdsp);
		if(tpackets) ogg_stream_clear(&tstream);
		if(vpackets) ogg_stream_clear(&vstream);
		th_info_clear(&tinfo);
		th_comment_clear(&tcomment);
		vorbis_comment_clear(&vcomment);
		vorbis_info_clear(&vinfo);
		ogg_sync_clear(&sync);
	}

	THEORAPLAYER_Decoder* ctx;
	double fps = 0.0;
	int eos = 0;
	ogg_packet packet;
	ogg_sync_state sync;
	ogg_page page;
	int vpackets = 0;
	vorbis_info vinfo;
	vorbis_comment vcomment;
	ogg_stream_state vstream;
	int vdsp_init = 0;
	vorbis_dsp_state vdsp;
	int tpackets = 0;
	th_info tinfo;
	th_comment tcomment;
	ogg_stream_state tstream;
	int vblock_init = 0;
	vorbis_block vblock;
	th_dec_ctx *tdec = NULL;
	th_setup_info *tsetup = NULL;

	void QueueOggPage()
	{
		if(tpackets) ogg_stream_pagein(&tstream, &page);
		if(vpackets) ogg_stream_pagein(&vstream, &page);
	}

	int Prepare()
	{
		ogg_sync_init(&sync);
		vorbis_info_init(&vinfo);
		vorbis_comment_init(&vcomment);
		th_info_init(&tinfo);
		th_comment_init(&tcomment);

		int readingHeader = 1;
		while(readingHeader)
		{
			if(FeedMoreOggData(ctx->io, &sync) <= 0)
				return -1;

			// parse out the initial header.
			while(ogg_sync_pageout(&sync, &page) > 0)
			{
				ogg_stream_state test;

				if(!ogg_page_bos(&page))  // not a header.
				{
					QueueOggPage();
					readingHeader = 0;
					break;
				} // if

				ogg_stream_init(&test, ogg_page_serialno(&page));
				ogg_stream_pagein(&test, &page);
				ogg_stream_packetout(&test, &packet);

				if(!tpackets && (th_decode_headerin(&tinfo, &tcomment, &tsetup, &packet) >= 0))
				{
					memcpy(&tstream, &test, sizeof(test));
					tpackets = 1;
				} // if
				else if(!vpackets && (vorbis_synthesis_headerin(&vinfo, &vcomment, &packet) >= 0))
				{
					memcpy(&vstream, &test, sizeof(test));
					vpackets = 1;
				} // else if
				else
				{
					// whatever it is, we don't care about it
					ogg_stream_clear(&test);
				} // else
			} // while
		}

		//Missing audio or video streams?
		//FIXME: No audio should be fine to continue, though?
		if(!vpackets || !tpackets)
			return -1;

		// apparently there are two more theora and two more vorbis headers next.
		while((tpackets && (tpackets < 3)) || (vpackets && (vpackets < 3)))
		{
			while(tpackets && (tpackets < 3))
			{
				if(ogg_stream_packetout(&tstream, &packet) != 1)
					break; // get more data?
				if(!th_decode_headerin(&tinfo, &tcomment, &tsetup, &packet))
					return -1;
				tpackets++;
			} // while

			while(vpackets && (vpackets < 3))
			{
				if(ogg_stream_packetout(&vstream, &packet) != 1)
					break;  // get more data?
				if(vorbis_synthesis_headerin(&vinfo, &vcomment, &packet))
					return -1;
				vpackets++;
			} // while

			  // get another page, try again?
			if(ogg_sync_pageout(&sync, &page) > 0)
				QueueOggPage();
			else if(FeedMoreOggData(ctx->io, &sync) <= 0)
				return -1;
		} // while

		  // okay, now we have our streams, ready to set up decoding.
		if(tpackets)
		{
			// th_decode_alloc() docs say to check for insanely large frames yourself.
			if((tinfo.frame_width > 99999) || (tinfo.frame_height > 99999))
				return -1;

			// We treat "unspecified" as NTSC. *shrug*
			if((tinfo.colorspace != TH_CS_UNSPECIFIED) &&
				(tinfo.colorspace != TH_CS_ITU_REC_470M) &&
				(tinfo.colorspace != TH_CS_ITU_REC_470BG))
			{
				assert(0 && "Unsupported colorspace.");  // !!! FIXME
				return -1;
			} // if

			if(tinfo.pixel_fmt != TH_PF_420)
			{
				assert(0);
				return -1; // !!! FIXME
			}

			if(tinfo.fps_denominator != 0)
				fps = ((double)tinfo.fps_numerator) / ((double)tinfo.fps_denominator);

			tdec = th_decode_alloc(&tinfo, tsetup);
			if(!tdec)
				return -1;

			// Set decoder to maximum post-processing level.
			//  Theoretically we could try dropping this level if we're not keeping up.
			int pp_level_max = 0;
			// !!! FIXME: maybe an API to set this?
			//th_decode_ctl(tdec, TH_DECCTL_GET_PPLEVEL_MAX, &pp_level_max, sizeof(pp_level_max));
			th_decode_ctl(tdec, TH_DECCTL_SET_PPLEVEL, &pp_level_max, sizeof(pp_level_max));
		} // if

		//Don't need the tsetup object anymore
		if(tsetup)
		{
			th_setup_free(tsetup);
			tsetup = nullptr;
		}

		if(vpackets)
		{
			vdsp_init = (vorbis_synthesis_init(&vdsp, &vinfo) == 0);
			if(!vdsp_init)
				return -1;
			vblock_init = (vorbis_block_init(&vdsp, &vblock) == 0);
			if(!vblock_init)
				return -1;
		}

		return 1;
	} //Prepare

	int DecodeNextVideoFrame(VideoFrame* frame)
	{
		if(eos)
		{
			return 0;
		}

		int saw_video_frame = 0;

		if(tpackets)
		{
			// Theora, according to example_player.c, is
			//  "one [packet] in, one [frame] out."
			while(ogg_stream_packetout(&tstream, &packet) <= 0)
			{
				const int rc = FeedMoreOggData(ctx->io, &sync);
				if(rc == 0)
				{
					eos = 1;  // end of stream
					return 0;
				}
				else if(rc < 0)
					return -1;  // i/o error, etc.
				else
				{
					while(ogg_sync_pageout(&sync, &page) > 0)
						QueueOggPage();
				} // else
			}

			ogg_int64_t granulepos = 0;

			// you have to guide the Theora decoder to get meaningful timestamps, apparently.  :/
			if(packet.granulepos >= 0)
				th_decode_ctl(tdec, TH_DECCTL_SET_GRANPOS, &packet.granulepos, sizeof(packet.granulepos));

			if(th_decode_packetin(tdec, &packet, &granulepos) == 0)  // new frame!
			{
				th_ycbcr_buffer ycbcr;
				if(th_decode_ycbcr_out(tdec, ycbcr) == 0)
				{
					const double videotime = th_granule_time(tdec, granulepos);
					frame->playms = (unsigned int)(videotime * 1000.0);
					frame->fps = fps;
					frame->width = tinfo.pic_width;
					frame->height = tinfo.pic_height;
					frame->format = ctx->vidfmt;
					if(!frame->pixels)
					{
						//FIXME: use a user-supplied allocator
						size_t allocSize = frame->width * frame->height * 4;
						if(frame->format == THEORAPLAYER_VIDFMT_RGB || frame->format == THEORAPLAYER_VIDFMT_BGR)
							allocSize = frame->width * frame->height * 3;
						frame->pixels = new unsigned char[allocSize];
					}

					//copy the pixels over in the requested format
					ctx->vidcvt(&tinfo, ycbcr, frame->pixels);
					if(frame->pixels == NULL)
					{
						return -1;
					} // if

					saw_video_frame = 1;
				} // if
			} // if
		} // if

		return saw_video_frame;
	}
};

static size_t IoFopenRead(THEORAPLAYER_Io *io, void *buf, long buflen)
{
	FILE *f = (FILE *)io->userdata;
	const size_t br = fread(buf, 1, buflen, f);
	if((br == 0) && ferror(f))
		return -1;
	return br;
} // IoFopenRead


static void IoFopenClose(THEORAPLAYER_Io *io)
{
	FILE *f = (FILE *)io->userdata;
	fclose(f);
	free(io);
} // IoFopenClose

TheoraPlayer::TheoraPlayer()
{

}

TheoraPlayer::~TheoraPlayer()
{
	delete _decoder;
	delete _state;
	delete _io;
}

int TheoraPlayer::OpenDecode(const char* filename, THEORAPLAYER_VideoFormat outputFormat)
{
	if(_decoder)
		return -1;

	FILE *f = fopen(filename, "rb");
	if(f == NULL)
	{
		return -1;
	} // if

	if(!_io)
	{
		_io = new THEORAPLAYER_Io();
		_io->read = IoFopenRead;
		_io->close = IoFopenClose;
		_io->userdata = f;
	}
	return OpenDecode(_io, outputFormat);
}

int TheoraPlayer::OpenDecode(THEORAPLAYER_Io* io, THEORAPLAYER_VideoFormat outputFormat)
{
	if(_decoder)
		return -1;

	ConvertVideoFrameFn vidcvt = nullptr;
	switch(outputFormat)
	{
	case THEORAPLAYER_VIDFMT_YV12:
		vidcvt = ConvertVideoFrame420ToYV12;
		break;
	case THEORAPLAYER_VIDFMT_IYUV:
		vidcvt = ConvertVideoFrame420ToIYUV;
		break;
	case THEORAPLAYER_VIDFMT_RGB:
		vidcvt = ConvertVideoFrame420ToRGB;
		break;
	case THEORAPLAYER_VIDFMT_RGBA:
		vidcvt = ConvertVideoFrame420ToRGBA;
		break;
	case THEORAPLAYER_VIDFMT_BGR:
		vidcvt = ConvertVideoFrame420ToBGR;
		break;
	case THEORAPLAYER_VIDFMT_BGRA:
		vidcvt = ConvertVideoFrame420ToBGRA;
		break;
	default:
		io->close(io);
		return -1;
	}

	_decoder = new THEORAPLAYER_Decoder;
	_decoder->vidfmt = outputFormat;
	_decoder->vidcvt = vidcvt;
	_decoder->io = io;
	return 1;
}

int TheoraPlayer::Prepare()
{
	//decoder should exist (OpenDecode has been called) but state should not because we will create it
	if(!_decoder)
		return -1;
	if(_state)
		return -1;

	//FIXME: Use a user-supplied allocator
	_state = new THEORAPLAYER_State;
	_state->ctx = _decoder;
	auto result = _state->Prepare();
	if(!result)
	{
		delete _state;
		_state = nullptr;
	}
	return result;
}

int TheoraPlayer::GetVideoFrame(THEORAPLAYER_VideoFrame* frame)
{
	if(!_state)
		return -1;
	if(!frame)
		return -1;

	auto result = _state->DecodeNextVideoFrame(frame);
	//If we had a decode error, nuke the internal state and refuse to provide any more data
	if(result < 0)
	{
		delete _state;
		_state = nullptr;
		return -1;
	}
	return result;
}

//FIXME: Use a user-supplied allocator
void TheoraPlayer::FreeFrameData(THEORAPLAYER_VideoFrame* frame)
{
	delete frame->pixels;
}

int TheoraPlayer::IsDecoding() const
{
	if(_decoder && _state && !_state->eos)
		return 1;
	return 0;
}
