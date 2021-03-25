## ��������ͼ�Ĳ���
1. ʹ��ffmpeg������Ƶ
2. ֡��ʽת��
3. ��������ͼ����������Ƶ����ȡ֡
4. ʹ��opencv������������������ͼ

## ffmpeg������Ƶ
![](https://ximikang-ciu.oss-cn-beijing.aliyuncs.com/img/ffmpeg������Ƶ.png?x-oss-process=style/common)
## ��������ͼ����������Ƶ����ȡ֡
1. ��ȡͼƬ֮���ʱ����
```c++
	// Read media file and read the header information from container format
	AVFormatContext* pFormatContext = avformat_alloc_context();
	if (!pFormatContext) {
		logging("ERROR could not allocate memory for format context");
		return -1;
	}
	
	if (avformat_open_input(&pFormatContext, inputFilePath.string().c_str(), NULL, NULL) != 0) {
		logging("ERROR could not open media file");
	}
	
	logging("format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);
	cout << "��Ƶʱ����" << pFormatContext->duration / 1000.0 / 1000.0 << "s" << endl;
	int64_t video_duration = pFormatContext->duration;
	int sum_count = rowNums * colNums;
	//��ת�ļ�� ms
	int64_t time_step = video_duration / sum_count / 1000;
```
2. ������תʱ���ȡ��ͬ����ƵPacket
```c++
	for (int i = 0; i < sum_count ; ++i) {
		cv::Mat tempImage;
		// ÿ�ζ�ȡ��ͬʱ������ͼ�񲢴���vImage��
		while (av_read_frame(pFormatContext, pPacket) >= 0) {
			if (pPacket->stream_index == video_stream_index) {
				response = decode_packet_2mat(pPacket, pCodecContext, pFrame, tempImage);// ����
			}
			if (response == 0)// �ɹ���ȡһ֡
				break;
			if (response < 0)
				continue;
		}
		vImage.push_back(tempImage);
		// ��ת��Ƶ
		av_seek_frame(pFormatContext, -1, ((double)time_step / (double)1000)* AV_TIME_BASE*(double)(i+1) + (double)pFormatContext->start_time, AVSEEK_FLAG_BACKWARD);
	}


```
3.��ȡFrame
�ڹ̶���ʱ�������޷���ȡ�ӵ�ǰʱ����Packet��ȡ��Ӧ��Frame��������Ҫ�Ի�ȡ��Packet�����жϣ����û�л�ȡ����Ӧ��FrameӦ�ü�����ȡ��һPacketֱ����ȡ����Ӧ��FrameΪֹ��
```c++
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
			// ��ȡ��Frame
			image = frame2Mat(pFrame, pCodecContext->pix_fmt);
		} 
		return 0;
	}
}
```
## ֡��ʽת��
���ڴ���Ƶ����ȡ��֡��YUV��ʽ��Frame��ʽ������ʹ��opencv���в������Խ��и�ʽת����

��ʹ��ffmpeg�е�SwsContext������Ƶ�г�ȡ����֡��YUVת����BGR��ʽ���ٴ�BGRFrame�е��ڴ��л�ȡԭʼ���ݣ���ת����opencv��Mat���͡�
```c++
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
	mRGB.data = (uchar*)pRGBFrame->data[0];//ע�ⲻ��дΪ��(uchar*)pFrameBGR->data

	av_free(pRGBFrame);
	sws_freeContext(rgbSwsContext);
	return mRGB;
}
```

## ʹ��opencv������������������ͼ
ͨ��������Ҫ�Ĵ�С������������ɫ�������ٶԻ���������䡣
```c++
cv::Mat makeThumbnail(vector<cv::Mat> vImage, const unsigned int rowNums, const unsigned int colNums)
{
	// �ж�ͼƬʱ����������
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

	// �������
	for (int i = 0; i < rowNums; ++i) {
		for (int j = 0; j < colNums; ++j) {
			int no = i * rowNums + j;
			int widthOffset = (vImage[0].size().width + interval) * j + interval;
			int heightOffset = (vImage[0].size().height + interval) * i + interval;
			vImage[no].copyTo(thumbnail(cv::Rect(widthOffset, heightOffset, vImage[0].size().width, vImage[0].size().height)));
		}
	}
	return thumbnail;
}
```
## ����Ч��
![](https://ximikang-ciu.oss-cn-beijing.aliyuncs.com/img/thumbnail.jpg?x-oss-process=style/common)