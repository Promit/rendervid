/*
 * Copyright © 2010 Chris Double <chris.double@double.co.nz>
 *
 * This program is made available under the ISC license.  See the
 * accompanying file LICENSE for details.
 */
#include <iostream>
#include <fstream>
#include <cassert>
#define HAVE_STDINT_H 1
extern "C" {
#include "vpx_decoder.h"
#include "vp8dx.h"
#include "nestegg/nestegg.h"
}
#include <SDL.h>

using namespace std;

FILE _iob[] = { *stdin, *stdout, *stderr };

extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}


static unsigned int mem_get_le32(const unsigned char *mem) {
    return (mem[3] << 24)|(mem[2] << 16)|(mem[1] << 8)|(mem[0]);
}


void play_webm(char const* name);

int file_read(void *buffer, size_t size, void *context)
{
	FILE* f = (FILE*)context;
	int result = fread(buffer, size, 1, f);
	if(result)
		return result;
	if(feof(f))
		return 0;
	return -1;
}

int file_seek(int64_t n, int whence, void *context)
{
	FILE* f = (FILE*)context;
	switch(whence) {
	case NESTEGG_SEEK_SET:
		return fseek(f, whence, SEEK_SET);
	case NESTEGG_SEEK_CUR:
		return fseek(f, whence, SEEK_CUR);
	case NESTEGG_SEEK_END:
		return fseek(f, whence, SEEK_END);
	}
	return -1;
}

int64_t file_tell(void* context)
{
	FILE* f = (FILE*)context;
	return ftell(f);
}

#if 0
void logger(nestegg * ctx, unsigned int severity, char const * fmt, ...) {
  va_list ap;
  char const * sev = NULL;

  switch (severity) {
  case NESTEGG_LOG_DEBUG:
    sev = "debug:   ";
    break;
  case NESTEGG_LOG_WARNING:
    sev = "warning: ";
    break;
  case NESTEGG_LOG_CRITICAL:
    sev = "critical:";
    break;
  default:
    sev = "unknown: ";
  }

  fprintf(stderr, "%p %s ", (void *) ctx, sev);

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fprintf(stderr, "\n");
}
#endif

void play_webm(char const* name) {
  int r = 0;
  nestegg* ne;

  FILE* ifile = fopen(name, "rb");
  nestegg_io ne_io;
  ne_io.read = file_read;
  ne_io.seek = file_seek;
  ne_io.tell = file_tell;
  ne_io.userdata = (void*)ifile;

  r = nestegg_init(&ne, ne_io, NULL /* logger */, -1);
  assert(r == 0);  

  uint64_t duration = 0;
  r = nestegg_duration(ne, &duration);
  assert(r == 0);
  cout << "Duration: " << duration << endl;

  unsigned int ntracks = 0;
  r = nestegg_track_count(ne, &ntracks);
  assert(r == 0);
  cout << "Tracks: " << ntracks << endl;

  nestegg_video_params vparams;
  vparams.width = 0;
  vparams.height = 0;

  vpx_codec_iface_t* interface;
  for (int i=0; i < ntracks; ++i) {
    int id = nestegg_track_codec_id(ne, i);
    assert(id >= 0);
    int type = nestegg_track_type(ne, i);
    cout << "Track " << i << " codec id: " << id << " type " << type << " ";
    interface = id == NESTEGG_CODEC_VP9 ? &vpx_codec_vp9_dx_algo : &vpx_codec_vp8_dx_algo;
    if (type == NESTEGG_TRACK_VIDEO) {
            
      r = nestegg_track_video_params(ne, i, &vparams);
      assert(r == 0);
      cout << vparams.width << "x" << vparams.height 
           << " (d: " << vparams.display_width << "x" << vparams.display_height << ")";
    }
    if (type == NESTEGG_TRACK_AUDIO) {
      nestegg_audio_params params;
      r = nestegg_track_audio_params(ne, i, &params);
      assert(r == 0);
      cout << params.rate << " " << params.channels << " channels " << " depth " << params.depth;
    }
    cout << endl;
  }

  vpx_codec_ctx_t  codec;
  int              flags = 0, frame_cnt = 0;
  vpx_codec_err_t  res;

  cout << "Using " << vpx_codec_iface_name(interface) << endl;
  /* Initialize codec */                                                    
  if(vpx_codec_dec_init(&codec, interface, NULL, flags)) {
    cerr << "Failed to initialize decoder" << endl;
    return;
  }

  SDL_Window* window = NULL;
  SDL_Texture* texture = NULL;
  SDL_Renderer* renderer = NULL;
 

  int video_count = 0;
  int audio_count = 0;
  nestegg_packet* packet = 0;
  // 1 = keep calling
  // 0 = eof
  // -1 = error
  while (1) {
    r = nestegg_read_packet(ne, &packet);
    if (r == 1 && packet == 0)
      continue;
    if (r <=0)
      break;
 
    unsigned int track = 0;
    r = nestegg_packet_track(packet, &track);
    assert(r == 0);

    // TODO: workaround bug
    if (nestegg_track_type(ne, track) == NESTEGG_TRACK_VIDEO) {
      cout << "video frame: " << ++video_count << " ";
      unsigned int count = 0;
      r = nestegg_packet_count(packet, &count);
      assert(r == 0);
      cout << "Count: " << count << " ";
      int nframes = 0;

      for (int j=0; j < count; ++j) {
        unsigned char* data;
        size_t length;
        r = nestegg_packet_data(packet, j, &data, &length);
        assert(r == 0);

        vpx_codec_stream_info_t si;
        memset(&si, 0, sizeof(si));
        si.sz = sizeof(si);
        vpx_codec_peek_stream_info(interface, data, length, &si);
        cout << "keyframe: " << (si.is_kf ? "yes" : "no") << " ";

        cout << "length: " << length << " ";
        /* Decode the frame */                             
        vpx_codec_err_t e = vpx_codec_decode(&codec, data, length, NULL, 0);
        if (e) {
          cerr << "Failed to decode frame. error: " << e << endl;
          return;
        }
       vpx_codec_iter_t  iter = NULL;
       vpx_image_t      *img;

        /* Write decoded data to disk */
        while((img = vpx_codec_get_frame(&codec, &iter))) {
          unsigned int plane, y;

          cout << "h: " << img->d_h << " w: " << img->d_w << endl;
          if (!window) {
            r = SDL_Init(SDL_INIT_VIDEO);
            assert(r == 0);

            window = SDL_CreateWindow("WebM Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, vparams.display_width, vparams.display_height, 0);
            assert(window);
			renderer = SDL_CreateRenderer(window, -1, 0);
			assert(renderer);
			texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, vparams.display_width, vparams.display_height);
            assert(texture);
 
          }
          nframes++;

          SDL_Rect rect;
          rect.x = 0;
          rect.y = 0;
          rect.w = vparams.display_width;
          rect.h = vparams.display_height;
    
		  int ypitch = vparams.display_width * vparams.display_height;
		  int uvpitch = ypitch / 2;
		  SDL_UpdateYUVTexture(texture, &rect, img->planes[0], ypitch, img->planes[2], uvpitch, img->planes[1], uvpitch);
		  SDL_RenderClear(renderer);
		  SDL_RenderCopy(renderer, texture, NULL, NULL);
		  SDL_RenderPresent(renderer);

          /*SDL_LockYUVOverlay(overlay);
          for (int y=0; y < img->d_h; ++y)
            memcpy(overlay->pixels[0]+(overlay->pitches[0]*y), 
	           img->planes[0]+(img->stride[0]*y), 
	           overlay->pitches[0]);
          for (int y=0; y < img->d_h>>1; ++y)
            memcpy(overlay->pixels[1]+(overlay->pitches[1]*y), 
	           img->planes[2]+(img->stride[2]*y), 
	           overlay->pitches[1]);
          for (int y=0; y < img->d_h>>1; ++y)
            memcpy(overlay->pixels[2]+(overlay->pitches[2]*y), 
	           img->planes[1]+(img->stride[1]*y), 
	           overlay->pitches[2]);
           SDL_UnlockYUVOverlay(overlay);	  
           SDL_DisplayYUVOverlay(overlay, &rect);*/
		   SDL_Delay(30);
        }

        cout << "nframes: " << nframes;
      }

      cout << endl;
    }

    if (nestegg_track_type(ne, track) == NESTEGG_TRACK_AUDIO) {
      cout << "audio frame: " << ++audio_count << endl;
    }


    SDL_Event event;
    if (SDL_PollEvent(&event) == 1) {
      if (event.type == SDL_KEYDOWN &&
          event.key.keysym.sym == SDLK_ESCAPE)
        break;
      //if (event.type == SDL_KEYDOWN &&
      //    event.key.keysym.sym == SDLK_SPACE)
      //  SDL_WM_ToggleFullScreen(surface);
    } 
  }

 if(vpx_codec_destroy(&codec)) {
    cerr << "Failed to destroy codec" << endl;
    return;
  }

  nestegg_destroy(ne);
  fclose(ifile);
  if(texture)
	  SDL_DestroyTexture(texture);
  if(renderer)
	  SDL_DestroyRenderer(renderer);
  if (window) {
    SDL_DestroyWindow(window);
  }

  SDL_Quit();
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: webm filename" << endl;
    return 1;
  }

  play_webm(argv[1]);
  

  return 0;
}
