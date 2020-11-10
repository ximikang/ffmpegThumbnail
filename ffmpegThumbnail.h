// ffmpegThumbnail.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
}
using namespace std;

static int decode_packet_2mat(AVPacket* pPacket, AVCodecContext* pCodecContext, AVFrame* pFrame, cv::Mat& image);
static void save_gray_frame(unsigned char* buf, int wrap, int xsize, int ysize, char* filename);
static void logging(const char* fmt, ...);
static int saveJPEG(AVCodecContext* pCodecConetext, AVFrame* pFrame, const char* filename);
cv::Mat frame2Mat(AVFrame* pFrame, AVPixelFormat pPixFormat);
cv::Mat makeThumbnail(vector<cv::Mat> vImage, const unsigned int rowNums, const unsigned int colNums);
