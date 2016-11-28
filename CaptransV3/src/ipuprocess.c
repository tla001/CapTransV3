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

#include "ipuprocess.h"
#include "capvideo.h"

int fd_ipu=0;
struct ipu_task taskCam;
struct timeval begintime, endtime;
unsigned int ipuOutputSize=0,ipuInputSize=0;
void *inbuf=NULL;
void *outbuf=NULL;
/***************与软件解码对应的IPU解码**************************/
void initIPU()
{
	int ret;
	CLEAR(taskCam);
	// Input image size and format
	taskCam.input.width    = 640;
	taskCam.input.height   = 480;
	taskCam.input.format   = v4l2_fourcc('Y', 'U', 'Y', 'V');
//
//	taskCam.input.crop.pos.x = 0;
//	taskCam.input.crop.pos.y = 0;
//	taskCam.input.crop.w = 0;
//	taskCam.input.crop.h = 0;

	// Output image size and format
	taskCam.output.width   = 640;
	taskCam.output.height  = 480;
	taskCam.output.format  = v4l2_fourcc('N', 'V', '1', '2');

//	taskCam.output.crop.pos.x = 300;
//	taskCam.output.crop.pos.y = 300;
//	taskCam.output.crop.w = 300;
//	taskCam.output.crop.h = 300;
	// Open IPU device
	fd_ipu = open(IPUDEV, O_RDWR, 0);
	if (fd_ipu < 0) {
		printf("open ipu dev fail\n");
	}
	ipuOutputSize=taskCam.output.paddr= taskCam.output.width * taskCam.output.height
			* fmt_to_bpp(taskCam.output.format)/8;
	printf("ipuOutputSize=%d\n",ipuOutputSize);
	ret = ioctl(fd_ipu, IPU_ALLOC, &taskCam.output.paddr);
		if (ret < 0) {
			printf("ioctl IPU_ALLOC fail\n");
	}
	outbuf= mmap(0, ipuOutputSize, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd_ipu, taskCam.output.paddr);
	if (!outbuf) {
			printf("mmap ipu output image fail\n");
		}
	ipuInputSize  =taskCam.input.paddr=taskCam.input.width * taskCam.input.height
				* fmt_to_bpp(taskCam.input.format)/8;
	printf("ipuInputSize=%d\n",ipuInputSize);
	ret = ioctl(fd_ipu, IPU_ALLOC, &taskCam.input.paddr);
	if (ret < 0) {
		printf("ioctl IPU_ALLOC fail: (errno = %d)\n", errno);
	}
	inbuf = mmap(0, ipuInputSize, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd_ipu, taskCam.input.paddr);
	if (!inbuf) {
				printf("mmap ipu input image fail\n");
			}
}
void IPUConvent(void *in,void *out)
{
	int ret;
	memcpy(inbuf, in, ipuInputSize);
	gettimeofday(&begintime, NULL);
	// Perform color space conversion
	ret = ioctl(fd_ipu, IPU_QUEUE_TASK, &taskCam);
	if (ret < 0) {
		printf("ioct IPU_QUEUE_TASK fail %x\n", ret);
	}
	gettimeofday(&endtime, NULL);
	double timeuse = 1000000*(endtime.tv_sec - begintime.tv_sec)+endtime.tv_usec-begintime.tv_usec;
	timeuse /=1000;//除以1000则进行毫秒计时，如果除以1000000则进行秒级别计时，如果除以1则进行微妙级别计时
	printf("yuv2rgb use %f ms\n",timeuse);
	memcpy(out,outbuf,ipuOutputSize);
}
void closeIPU()
{
	//if(rgbFrame)munmap(rgbFrame, ipuOutputSize);
	if(inbuf)munmap(inbuf, ipuInputSize);
	if (taskCam.input.paddr)
			ioctl(fd_ipu, IPU_FREE, &taskCam.input.paddr);
}
unsigned int fmt_to_bpp(unsigned int pixelformat)
{
	unsigned int bpp;

        switch (pixelformat)
        {
                case IPU_PIX_FMT_RGB565:
               /*interleaved 422*/
                case IPU_PIX_FMT_YUYV:
                case IPU_PIX_FMT_UYVY:
                /*non-interleaved 422*/
                case IPU_PIX_FMT_YUV422P:
                case IPU_PIX_FMT_YVU422P:
                        bpp = 16;
                        break;
                case IPU_PIX_FMT_BGR24:
                case IPU_PIX_FMT_RGB24:
                case IPU_PIX_FMT_YUV444:
                case IPU_PIX_FMT_YUV444P:
                        bpp = 24;
                        break;
                case IPU_PIX_FMT_BGR32:
                case IPU_PIX_FMT_BGRA32:
                case IPU_PIX_FMT_RGB32:
                case IPU_PIX_FMT_RGBA32:
                case IPU_PIX_FMT_ABGR32:
                        bpp = 32;
                        break;
                /*non-interleaved 420*/
                case IPU_PIX_FMT_YUV420P:
                case IPU_PIX_FMT_YVU420P:
                case IPU_PIX_FMT_YUV420P2:
                case IPU_PIX_FMT_NV12:
    case IPU_PIX_FMT_TILED_NV12:
                        bpp = 12;
                        break;
                default:
                        bpp = 8;
                        break;
        }
        return bpp;
}
