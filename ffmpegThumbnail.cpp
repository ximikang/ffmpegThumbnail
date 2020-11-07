// ffmpegThumbnail.cpp: 定义应用程序的入口点。
//

#include "ffmpegThumbnail.h"
#include <string>
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <stdio.h>
	#include <stdarg.h>
	#include <stdlib.h>
	#include <string.h>
	#include <inttypes.h>
}

using namespace std;

static void save_gray_frame(unsigned char* buf, int wrap, int xsize, int ysize, char* filename);
static int decode_packet(AVPacket* pPacket, AVCodecContext* pCodecContext, AVFrame* pFrame);
static void logging(const char* fmt, ...);
static int saveJPEG(AVCodecContext* pCodecConetext,AVFrame* pFrame, const char* filename);
int main()
{
	char* filename = "C:/Users/ximik/Source/Repos/ffmpegThumbnail/test3.mp4";
	
	// Read media file and read the header information from container format
	AVFormatContext* pFormatContext = avformat_alloc_context();
	if (!pFormatContext) {
		logging("ERROR could not allocate memory for format context");
		return -1;
	}
	
	if (avformat_open_input(&pFormatContext, filename, NULL, NULL) != 0) {
		logging("ERROR could not open media file");
	}
	
	logging("format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);
	cout << "视频时常：" << pFormatContext->duration / 1000.0 / 1000.0 << "s" << endl;
	int64_t video_duration = pFormatContext->duration;
	// find stream info from format
	if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
		logging("ERROR could not get stream inof");
		return -1;
	}

	AVCodec* pCodec = NULL;
	AVCodecParameters* pCodecParameters = NULL;
	int video_stream_index = -1;

	// loop all the stream and print its info
	for (int i = 0; i < pFormatContext->nb_streams; ++i) {
		AVCodecParameters* pLocalCodecParameters = NULL;
		pLocalCodecParameters = pFormatContext->streams[i]->codecpar;

		if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
			if (video_stream_index == -1) {
				video_stream_index = i;
				pCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
				if (pCodec == NULL) {
					logging("ERROR could not find decoder");
					return -1;
				}
				pCodecParameters = pLocalCodecParameters;
			}
		}
	}


	logging("Codec %s ID %d", pCodec->name, pCodec->id);

	AVCodecContext* pCodecContext = avcodec_alloc_context3(pCodec);
	if (!pCodecContext) {
		logging("failed to copy codec params to codec context");
		return -1;
	}

	avcodec_parameters_to_context(pCodecContext, pCodecParameters);
	if (avcodec_open2(pCodecContext, pCodec, NULL) < 0) {
		logging("failed to open codec through avcodec_open2");
		return -1;
	}

	AVFrame* pFrame = av_frame_alloc();
	if (!pFrame) {
		logging("failed to allocated memory for AvFrame");
		return -1;
	}

	AVPacket* pPacket = av_packet_alloc();
	if (!pPacket) {
		logging("failed to allocated memory for AvPacket");
		return -1;
	}

	int response = 0;
	int sum_count = 5;
	//跳转的间隔 ms
	int64_t time_step = video_duration / sum_count / 1000;

	for (int i = 0; i < sum_count+1; ++i) {
		// 每次读取相同时间间隔的一帧并保存
		while (av_read_frame(pFormatContext, pPacket) >= 0) {
			if (pPacket->stream_index == video_stream_index) {
				response = decode_packet(pPacket, pCodecContext, pFrame);
			}
			if (response == 0) 
				break;
			if (response < 0)
				continue;
		}
		// 跳转视频
		av_seek_frame(pFormatContext, -1, ((double)time_step / (double)1000)* AV_TIME_BASE*(double)(i+1) + (double)pFormatContext->start_time, AVSEEK_FLAG_BACKWARD);
	}

	// free memory
	avformat_close_input(&pFormatContext);
	av_packet_free(&pPacket);
	av_frame_free(&pFrame);
	avcodec_free_context(&pCodecContext);
	return 0;
}

static int decode_packet(AVPacket* pPacket, AVCodecContext* pCodecContext, AVFrame* pFrame) {
	int response = avcodec_send_packet(pCodecContext, pPacket);

	if (response < 0) {
		logging("Error while sending a packet to the decoder");
		return response;
	}

	while (response >= 0) {
		// return decoded out data from a decoder
		response = avcodec_receive_frame(pCodecContext, pFrame);
		if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
			logging("averror averror_eof");
			break;
		}
		else if (response < 0) {
			logging("Error while receiving frame");
			return response;
		}

		if (response >= 0) {
			
			char frame_filename[1024];
			snprintf(frame_filename, sizeof(frame_filename), "./%s-%d.jpg", "frame", pCodecContext->frame_number);
			logging("saved file name: %s", frame_filename);
			saveJPEG(pCodecContext, pFrame, frame_filename);
		}
		return 0;
	}
}

static void logging(const char* fmt, ...) {
	va_list args;
	fprintf(stderr, "LOG:");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

static void save_gray_frame(unsigned char* buf, int wrap, int xsize, int ysize, char* filename)
{
	FILE* f;
	int i;
	f = fopen(filename, "w");
	// writing the minimal required header for a pgm file format
	// portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

	// writing line by line
	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}

static int saveJPEG(AVCodecContext* pCodecConetext,AVFrame* pFrame, const char* filename) {
	int width = pFrame->width;
	int height = pFrame->height;
	AVCodecContext* pCodeCtx = NULL;


	AVFormatContext* pFormatCtx = avformat_alloc_context();
	// 设置输出文件格式
	pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);

	// 创建并初始化输出AVIOContext
	if (avio_open(&pFormatCtx->pb, filename, AVIO_FLAG_READ_WRITE) < 0) {
		printf("Couldn't open output file.");
		return -1;
	}

	// 构建一个新stream
	AVStream* pAVStream = avformat_new_stream(pFormatCtx, 0);
	if (pAVStream == NULL) {
		return -1;
	}

	AVCodecParameters* parameters = pAVStream->codecpar;
	parameters->codec_id = pFormatCtx->oformat->video_codec;
	parameters->codec_type = AVMEDIA_TYPE_VIDEO;
	parameters->format = AV_PIX_FMT_YUVJ420P;
	parameters->width = pFrame->width;
	parameters->height = pFrame->height;

	AVCodec* pCodec = avcodec_find_encoder(pAVStream->codecpar->codec_id);

	if (!pCodec) {
		printf("Could not find encoder\n");
		return -1;
	}

	pCodeCtx = avcodec_alloc_context3(pCodec);
	if (!pCodeCtx) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	if ((avcodec_parameters_to_context(pCodeCtx, pAVStream->codecpar)) < 0) {
		fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
			av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return -1;
	}

	pCodeCtx->time_base.num = 1;
	pCodeCtx->time_base.den = 25;

	if (avcodec_open2(pCodeCtx, pCodec, NULL) < 0) {
		printf("Could not open codec.");
		return -1;
	}

	int ret = avformat_write_header(pFormatCtx, NULL);
	if (ret < 0) {
		printf("write_header fail\n");
		return -1;
	}

	int y_size = width * height;

	//Encode
	// 给AVPacket分配足够大的空间
	AVPacket pkt;
	av_new_packet(&pkt, y_size * 3);

	// 编码数据
	ret = avcodec_send_frame(pCodeCtx, pFrame);
	if (ret < 0) {
		printf("Could not avcodec_send_frame.");
		return -1;
	}

	// 得到编码后数据
	ret = avcodec_receive_packet(pCodeCtx, &pkt);
	if (ret < 0) {
		printf("Could not avcodec_receive_packet");
		return -1;
	}

	ret = av_write_frame(pFormatCtx, &pkt);

	if (ret < 0) {
		printf("Could not av_write_frame");
		return -1;
	}

	av_packet_unref(&pkt);

	//Write Trailer
	av_write_trailer(pFormatCtx);


	avcodec_close(pCodeCtx);
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);

	return 0;

}