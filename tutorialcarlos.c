// tutorial01.c
//
// This tutorial was written by Stephen Dranger (dranger@gmail.com).
//
// Code based on a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1

// A small sample program that shows how to use libavformat and libavcodec to
// read video from a file.
//
// Use the Makefile to build all examples.
//
// Run using
//
// tutorial01 myvideofile.mpg
//
// to write the first five frames from "myvideofile.mpg" to disk in PPM
// format.

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

// #include "libavutil/common.h"
// #include "libavcodec/avcodec.h"
// #include "libavformat/avformat.h"
// #include "libswscale/swscale.h"

#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// #include <SDL2_log.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>
// http://dranger.com/ffmpeg/tutorial01.html

int waitForWindowQuit()
{
  int quit = 0;
  SDL_Event event;

  while (!quit)
  {
    SDL_WaitEvent(&event);

    switch (event.type)
    {
    case SDL_QUIT:
      quit = 1;
      break;
    }
  }

  SDL_Quit();
  exit(0);
  return 0;
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
  FILE *pFile;
  char szFilename[32];
  int y;

  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile = fopen(szFilename, "wb");
  if (pFile == NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  for (y = 0; y < height; y++)
    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

  // Close file
  fclose(pFile);
}

int main(int argc, char *argv[])
{
  AVFormatContext *pFormatCtx = NULL;
  int i, videoStream;
  AVCodecContext *pCodecCtx = NULL;
  AVCodec *pCodec = NULL;
  AVFrame *pFrame = NULL;
  AVFrame *pFrameRGB = NULL;
  AVPacket packet;
  int frameFinished;
  int numBytes;
  uint8_t *buffer = NULL;

  AVDictionary *optionsDict = NULL;
  struct SwsContext *sws_ctx = NULL;
  // struct SwsContext *sws_ctx_yuv = NULL;

  // SDL_Overlay *bmp = NULL;
  // SDL_Surface *screen = NULL;
  // SDL_Rect rect;
  SDL_Event event;

  if (argc < 2)
  {
    printf("Please provide a movie file\n");
    return -1;
  }
  // Register all formats and codecs
  av_register_all();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
  {
    fprintf(stderr, "Couldn't initialize SDL: %s", SDL_GetError());
  }

  // SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN);

  // Open video file
  if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
    return -1; // Couldn't open file

  // Retrieve stream information
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1; // Couldn't find stream information

  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, argv[1], 0);

  // Find the first video stream
  videoStream = -1;
  for (i = 0; i < pFormatCtx->nb_streams; i++)
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      videoStream = i;
      break;
    }
  if (videoStream == -1)
    return -1; // Didn't find a video stream

  // Get a pointer to the codec context for the video stream
  pCodecCtx = pFormatCtx->streams[videoStream]->codec;

  // Find the decoder for the video stream
  pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  if (pCodec == NULL)
  {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }
  // Open codec
  if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0)
  {
    return -1; // Could not open codec
  }

  // Allocate video frame
  pFrame = av_frame_alloc();

  // Allocate an AVFrame structure
  pFrameRGB = av_frame_alloc();

  if (pFrameRGB == NULL)
  {
    return -1;
  }

  // #ifndef __DARWIN__
  //   screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
  // #else
  //   screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
  // #endif

  // if (!screen)
  // {
  //   fprintf(stderr, "SDL: could not set video mode - exiting");
  // }

  // IMG: Determine required buffer size and allocate buffer
  numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
                                pCodecCtx->height);
  buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

  // Video: Allocate a place to put our YUV image on that screen
  // bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height, SDL_YV12_OVERLAY, screen);

  sws_ctx =
      sws_getContext(
          pCodecCtx->width,
          pCodecCtx->height,
          pCodecCtx->pix_fmt,
          pCodecCtx->width,
          pCodecCtx->height,
          AV_PIX_FMT_RGB24,
          SWS_BILINEAR,
          NULL,
          NULL,
          NULL);

  // sws_ctx_yuv =
  //     sws_getContext(
  //         pCodecCtx->width,
  //         pCodecCtx->height,
  //         pCodecCtx->pix_fmt,
  //         pCodecCtx->width,
  //         pCodecCtx->height,
  //         AV_PIX_FMT_YUV420P,
  //         SWS_BILINEAR,
  //         NULL,
  //         NULL,
  //         NULL);

  // Assign appropriate parts of buffer to image planes in pFrameRGB
  // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
  // of AVPicture
  avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
                 pCodecCtx->width, pCodecCtx->height);

  SDL_Window *window = SDL_CreateWindow("My SDL Empty Window",
                                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width / 2, pCodecCtx->height / 2, 0);

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

  // Read frames and save first five frames to disk
  i = 0;
  while (av_read_frame(pFormatCtx, &packet) >= 0)
  {
    // Is this a packet from the video stream?
    if (packet.stream_index == videoStream)
    {
      // Decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
                            &packet);

      // Did we get a video frame?
      if (frameFinished)
      {
        // Convert the image from its native format to RGB
        sws_scale(
            sws_ctx,
            (uint8_t const *const *)pFrame->data,
            pFrame->linesize,
            0,
            pCodecCtx->height,
            pFrameRGB->data,
            pFrameRGB->linesize);

        // Save the frame to disk
        // if (++i <= 5)
        // {
        //   printf("save image!\n");
        //   SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
        // }
        // else
        // {
        //   break;
        // }

        int req_format = STBI_rgb_alpha;
        int width, height, orig_format;
        unsigned char *data = stbi_load("./test.png", &width, &height, &orig_format, req_format);
        if (data == NULL)
        {
          SDL_Log("Loading image failed: %s", stbi_failure_reason());
          exit(1);
        }

        int depth, pitch;
        Uint32 pixel_format;
        if (req_format == STBI_rgb)
        {
          depth = 24;
          pitch = 3 * width; // 3 bytes per pixel * pixels per row
          pixel_format = SDL_PIXELFORMAT_RGB24;
        }
        else
        { // STBI_rgb_alpha (RGBA)
          depth = 32;
          pitch = 4 * width;
          pixel_format = SDL_PIXELFORMAT_RGBA32;
        }

        // SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom((void *)data, width, height, depth, pitch, pixel_format);

        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pFrameRGB->data[0], pCodecCtx->width, pCodecCtx->height, 24, pFrameRGB->linesize[0], SDL_PIXELFORMAT_RGB24);

        // SDL_LockSurface(surface);

        // SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 255, 0, 0));

        // AVPicture pict;
        // pict.data[0] = bmp->pixels[0];
        // pict.data[1] = bmp->pixels[2];
        // pict.data[2] = bmp->pixels[1];

        // pict.linesize[0] = bmp->pitches[0];
        // pict.linesize[1] = bmp->pitches[2];
        // pict.linesize[2] = bmp->pitches[1];

        // sws_scale(
        //     sws_ctx_yuv,
        //     (uint8_t const *const *)pFrame->data,
        //     pFrame->linesize,
        //     0,
        //     pCodecCtx->height,
        //     pict.data,
        //     pict.linesize);

        // SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

        // SDL_UnlockSurface(surface);

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_Rect DestR;
        DestR.x = 0;
        DestR.y = pCodecCtx->height / 2;
        DestR.w = pCodecCtx->width/2;
        DestR.h = pCodecCtx->height/2;

        SDL_RenderCopy(renderer, texture, NULL, &DestR);
        SDL_RenderPresent(renderer);

        // waitForWindowQuit();

        // rect.x = 0;
        // rect.y = 0;
        // rect.w = pCodecCtx->width;
        // rect.h = pCodecCtx->height;
        // SDL_DisplayYUVOverlay(bmp, &rect);
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);
    SDL_PollEvent(&event);
  }
  fprintf(stderr, "SDL Error?: %s", SDL_GetError());

  // Free the RGB image
  av_free(buffer);
  av_free(pFrameRGB);

  // Free the YUV frame
  av_free(pFrame);

  // Close the codec
  avcodec_close(pCodecCtx);

  // Close the video file
  avformat_close_input(&pFormatCtx);

  return 0;
}
