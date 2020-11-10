#include "ffmpegThumbnail.h"

int main(int arg, char** argv)
{
	char* filename = "C:/Users/ximik/Source/Repos/ffmpegThumbnail/test.mkv";
	
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
	int rowNums = 4;
	int colNums = 5;
	int sum_count = rowNums * colNums;
	//跳转的间隔 ms
	int64_t time_step = video_duration / sum_count / 1000;
	vector<cv::Mat> vImage;
	

	for (int i = 0; i < sum_count ; ++i) {
		cv::Mat tempImage;
		// 每次读取相同时间间隔的图像并存入vImage中
		while (av_read_frame(pFormatContext, pPacket) >= 0) {
			if (pPacket->stream_index == video_stream_index) {
				response = decode_packet_2mat(pPacket, pCodecContext, pFrame, tempImage);// 返回
			}
			if (response == 0)// 成功读取一帧
				break;
			if (response < 0)
				continue;
		}
		vImage.push_back(tempImage);
		// 跳转视频
		av_seek_frame(pFormatContext, -1, ((double)time_step / (double)1000)* AV_TIME_BASE*(double)(i+1) + (double)pFormatContext->start_time, AVSEEK_FLAG_BACKWARD);
	}

	cv::Mat thumbnail = makeThumbnail(vImage, rowNums, colNums);
	// free memory
	avformat_close_input(&pFormatContext);
	av_packet_free(&pPacket);
	av_frame_free(&pFrame);
	avcodec_free_context(&pCodecContext);
	return 0;
}

static int decode_packet_2mat(AVPacket* pPacket, AVCodecContext* pCodecContext, AVFrame* pFrame, cv::Mat& image) {
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
			image = frame2Mat(pFrame, pCodecContext->pix_fmt);
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

cv::Mat frame2Mat(AVFrame* pFrame, AVPixelFormat pPixFormat)
{
	// image init
	AVFrame* pRGBFrame = av_frame_alloc();
	uint8_t* out_buffer = new uint8_t[avpicture_get_size(AV_PIX_FMT_BGR24, pFrame->width, pFrame->height)];
	avpicture_fill((AVPicture*)pRGBFrame, out_buffer, AV_PIX_FMT_BGR24, pFrame->width, pFrame->height);
	SwsContext* rgbSwsContext = sws_getContext(pFrame->width, pFrame->height, pPixFormat, pFrame->width, pFrame->height, AV_PIX_FMT_BGR24,SWS_BICUBIC, NULL, NULL, NULL);
	if (!rgbSwsContext) {
		logging("Error could not create frame to rgbframe sws context");
		exit(-1);
	}
	if (sws_scale(rgbSwsContext, pFrame->data, pFrame->linesize, 0, pFrame->height, pRGBFrame->data, pRGBFrame->linesize) < 0) {
		logging("Error could not sws to rgb frame");
		exit(-1);
	}

	cv::Mat mRGB(cv::Size(pFrame->width, pFrame->height), CV_8UC3);
	mRGB.data = (uchar*)pRGBFrame->data[0];//注意不能写为：(uchar*)pFrameBGR->data

	av_free(pRGBFrame);
	sws_freeContext(rgbSwsContext);
	return mRGB;
}

cv::Mat makeThumbnail(vector<cv::Mat> vImage, const unsigned int rowNums, const unsigned int colNums)
{
	// 判断图片时候满足条件
	if (vImage.size() != rowNums * colNums) {
		logging("Error image size not equal input size");
		logging("vImage length: %d, rowNums: %d, col number: %d", vImage.size(), rowNums, colNums);
		exit(-1);
	}
	int interval = 100;
	int height = vImage[0].size().height * rowNums + interval * (rowNums + 1);
	int width = vImage[0].size().width * colNums + interval * (colNums + 1);
	logging("thumbnail size: %d * %d", height, width);
	cv::Mat thumbnail(cv::Size(width, height), CV_8UC3);
	thumbnail.setTo(255);

	// 进行填充
	for (int i = 0; i < rowNums; ++i) {
		for (int j = 0; j < colNums; ++j) {
			int no = i * rowNums + j;
			int widthOffset = (vImage[0].size().width + interval) * j;
			int heightOffset = (vImage[0].size().height + interval) * i;
			vImage[no].copyTo(thumbnail(cv::Rect(widthOffset, heightOffset, vImage[0].size().width, vImage[0].size().height)));
		}
	}
	return thumbnail;
}
