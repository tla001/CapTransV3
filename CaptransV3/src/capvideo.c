#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdint.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <asm-generic/ioctl.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <linux/videodev2.h>

#include <sys/mman.h>
#include <linux/fb.h>
#include <time.h>
#include <ipu.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "capvideo.h"

int fd_cam;
struct cam_buffer *buffers=NULL;
unsigned int n_buffers=0;
int frameIndex=0;

void initVideo()
{
	int ret;
	struct 	v4l2_capability cam_cap;		//显示设备信息
	struct  v4l2_cropcap 	cam_cropcap;	//设置摄像头的捕捉能力
	struct 	v4l2_fmtdesc	cam_fmtdesc;	//查询所有支持的格式：VIDIOC_ENUM_FMT
	struct  v4l2_crop		cam_crop;		//图像的缩放
	struct  v4l2_format 	cam_format;		//设置摄像头的视频制式、帧格式等

	/*设备的打开*/
	fd_cam = open( USB_VIDEO, O_RDWR );
	if( fd_cam<0 )
	printf("Can't open video device\n");

	/* 使用IOCTL命令VIDIOC_QUERYCAP，获取摄像头的基本信息*/
	ret = ioctl( fd_cam,VIDIOC_QUERYCAP,&cam_cap );
	if( ret<0 ) {
	printf("Can't get device information: VIDIOCGCAP\n");
	}
	printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n",
			cam_cap.driver,cam_cap.card,cam_cap.bus_info,(cam_cap.version>>16)&0XFF,
			(cam_cap.version>>8)&0XFF,cam_cap.version&0XFF);

	/* 使用IOCTL命令VIDIOC_ENUM_FMT，获取摄像头所有支持的格式*/
	cam_fmtdesc.index=0;
	cam_fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("Support format:\n");
	while(ioctl(fd_cam, VIDIOC_ENUM_FMT, &cam_fmtdesc) != -1)
	{
		printf("\t%d.%s\n",cam_fmtdesc.index+1,cam_fmtdesc.description);
		cam_fmtdesc.index++;
	}

	/* 使用IOCTL命令VIDIOC_CROPCAP，获取摄像头的捕捉能力*/
	cam_cropcap.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(0 == ioctl(fd_cam, VIDIOC_CROPCAP, &cam_cropcap)){
		printf("Default rec:\n\tleft:%d\n\ttop:%d\n\twidth:%d\n\theight:%d\n",
				cam_cropcap.defrect.left,cam_cropcap.defrect.top,
				cam_cropcap.defrect.width,cam_cropcap.defrect.height);
		/* 使用IOCTL命令VIDIOC_S_CROP，获取摄像头的窗口取景参数*/
		cam_crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cam_crop.c = cam_cropcap.defrect;//默认取景窗口大小
		if(-1 == ioctl(fd_cam, VIDIOC_S_CROP, &cam_crop)){
			//printf("Can't set crop para\n");
		}
	}
	else{
		printf("Can't set cropcap para\n");
	}

	/* 使用IOCTL命令VIDIOC_S_FMT，设置摄像头帧信息*/
	cam_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam_format.fmt.pix.width = 640;
	cam_format.fmt.pix.height = 480;
	cam_format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//要和摄像头支持的类型对应
	cam_format.fmt.pix.field = V4L2_FIELD_INTERLACED;
	ret=ioctl(fd_cam, VIDIOC_S_FMT, &cam_format);
	if(ret<0){
			printf("Can't set frame information\n");
	}
	/* 使用IOCTL命令VIDIOC_G_FMT，获取摄像头帧信息*/
	cam_format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret=ioctl(fd_cam, VIDIOC_G_FMT, &cam_format);
	if(ret<0){
		printf("Can't get frame information\n");
	}
	printf("Current data format information:\n\twidth:%d\n\theight:%d\n",
			cam_format.fmt.pix.width,cam_format.fmt.pix.height);
	ret=initBuffers();
	if(ret<0){
		printf("Buffers init error\n");
		//exit(-1);
	}
}

void closeVideo()
{
	//stopCapture();
	//freeBuffers();
	close(fd_cam);
}

int initBuffers()
{
	int ret;
	/* 使用IOCTL命令VIDIOC_REQBUFS，申请帧缓冲*/
	struct v4l2_requestbuffers req;
	CLEAR(req);
	req.count=4;
	req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	ret=ioctl(fd_cam, VIDIOC_REQBUFS, &req);
	if(ret<0){
		printf("Request frame buffers failed\n");
		return -1;
	}
	if(req.count<2){
		printf("Request frame buffers while insufficient buffer memory\n");
		return -1;
	}
	buffers = (struct cam_buffer*)calloc(req.count, sizeof(*buffers));
	if(!buffers){
		printf("Out of memory\n");
		return -1;
	}
	for(n_buffers = 0; n_buffers < req.count; n_buffers++){
		struct v4l2_buffer buf;
		CLEAR(buf);
		// 查询序号为n_buffers 的缓冲区，得到其起始物理地址和大小
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;
		ret=ioctl(fd_cam, VIDIOC_QUERYBUF, &buf);
		if(ret<0 )
		{
			printf("VIDIOC_QUERYBUF %d failed\n",n_buffers);
			return -1;
		}
		buffers[n_buffers].length = buf.length;
		// 映射内存
		buffers[n_buffers].start =
		 mmap(NULL, // start anywhere
			  buf.length,
			  PROT_READ | PROT_WRITE,
			  MAP_SHARED,
			  fd_cam, buf.m.offset);
		if(MAP_FAILED == buffers[n_buffers].start)
		{
			printf("mmap buffer%d failed\n",n_buffers);
			return -1;
		}

	}
	return 0;
}
int startCapture()
{
	unsigned int i;
	//struct v4l2_buffer buf;
	for(i=0;i<n_buffers;i++){
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory =V4L2_MEMORY_MMAP;
		buf.index = i;
		//        fprintf(stderr, "n_buffers: %d\n", i);
		if(-1 == ioctl(fd_cam, VIDIOC_QBUF, &buf))	{
			printf("VIDIOC_QBUF buffer%d failed\n",i);
			return -1;
		}
	}
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == ioctl(fd_cam, VIDIOC_STREAMON, &type)){
		 printf("VIDIOC_STREAMON error");
		 return -1;
	}
	return 0;
}
int stopCapture()
{
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == ioctl(fd_cam, VIDIOC_STREAMOFF, &type)){
		printf("VIDIOC_STREAMOFF error\n");
		return -1;
	}
	return 0;
}
int freeBuffers()
{
	unsigned int i;
	for(i = 0; i < n_buffers; ++i){
		if(-1 == munmap(buffers[i].start, buffers[i].length)){
			printf("munmap buffer%d failed\n",i);
			return -1;
		}
	}
	free(buffers);
	return 0;
}
int getFrame(void **frame_buf, size_t* len)
{
	struct v4l2_buffer queue_buf;
	CLEAR(queue_buf);
	queue_buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	queue_buf.memory = V4L2_MEMORY_MMAP;
	if(-1 == ioctl(fd_cam, VIDIOC_DQBUF, &queue_buf)){
		printf("VIDIOC_DQBUF error\n");
		return -1;
	}
	printf("queue_buf.index=%d\n",queue_buf.index);
	//pthread_rwlock_wrlock(&rwlock);
	*frame_buf = buffers[queue_buf.index].start;
	*len = buffers[queue_buf.index].length;
	frameIndex = queue_buf.index;
	//pthread_rwlock_unlock(&rwlock);
	return 0;
}
int backFrame()
{
	if(frameIndex != -1){
		struct v4l2_buffer queue_buf;
		CLEAR(queue_buf);
		queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		queue_buf.memory = V4L2_MEMORY_MMAP;
		queue_buf.index = frameIndex;
		if(-1 == ioctl(fd_cam, VIDIOC_QBUF, &queue_buf)){
			printf("VIDIOC_QBUF error\n");
			return -1;
		}
		return 0;
	}
	return -1;
}

/*yuv格式转换为rgb格式*/
int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
	unsigned int in, out = 0;
	unsigned int pixel_16;
	unsigned char pixel_24[3];
	unsigned int pixel32;
	int y0, u, y1, v;
	struct timeval starttime,endtime;
	gettimeofday(&starttime,0);
	for(in = 0; in < width * height * 2; in += 4) {
		pixel_16 =
		yuv[in + 3] << 24 |
		yuv[in + 2] << 16 |
		yuv[in + 1] <<  8 |
		yuv[in + 0];
		y0 = (pixel_16 & 0x000000ff);
		u  = (pixel_16 & 0x0000ff00) >>  8;
		y1 = (pixel_16 & 0x00ff0000) >> 16;
		v  = (pixel_16 & 0xff000000) >> 24;
		pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
		//pthread_rwlock_wrlock(&rwlock);
		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
		//pthread_rwlock_unlock(&rwlock);
		pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
		//pthread_rwlock_wrlock(&rwlock);
		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
		//pthread_rwlock_unlock(&rwlock);
	}
	 gettimeofday(&endtime,0);
	 double timeuse = 1000000*(endtime.tv_sec - starttime.tv_sec)+endtime.tv_usec-starttime.tv_usec;
	      timeuse /=1000;//除以1000则进行毫秒计时，如果除以1000000则进行秒级别计时，如果除以1则进行微妙级别计时
	 printf("yuv2rgb use %f ms\n",timeuse);
	return 0;
}
int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
	unsigned int pixel32 = 0;
	unsigned char *pixel = (unsigned char *)&pixel32;
	int r, g, b;
	r = y + (1.370705 * (v-128));
	g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
	b = y + (1.732446 * (u-128));
	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;
	if(r < 0) r = 0;
	if(g < 0) g = 0;
	if(b < 0) b = 0;
	pixel[0] = r * 220 / 256;
	pixel[1] = g * 220 / 256;
	pixel[2] = b * 220 / 256;
	return pixel32;
}




