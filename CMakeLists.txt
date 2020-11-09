# CMakeList.txt: ffmpegThumbnail 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)
set(OpenCV_DIR "E:/opencv/build")
FIND_PACKAGE( OpenCV REQUIRED )   
# 头文件
include_directories("include")
include_directories(${OpenCV_INCLUDE_DIRS})
# 库文件
link_directories("lib")

# 将源代码添加到此项目的可执行文件。
add_executable (ffmpegThumbnail "ffmpegThumbnail.cpp" "ffmpegThumbnail.h")

# 链接库
target_link_libraries(
	ffmpegThumbnail
	avcodec		#音视频编码核心库 , 用于音视频或图片编解码
	avdevice	#硬件采集 , 加速 , 显示 库
	avfilter	#音视频滤波器
	avformat	#音视频封装格式生成或解析 
	avutil		#该库中封装了一些公共的工具函数
	postproc	#封装了同步, 时间计算的简单算法
	swresample	#音视频编解码格式预设
	swscale		#原始视频格式转换库
)
target_link_libraries(ffmpegThumbnail ${OpenCV_LIBS})
# TODO: 如有需要，请添加测试并安装目标。