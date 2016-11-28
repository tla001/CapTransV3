/*
 * virtualfile.c
 *
 *  Created on: Nov 27, 2016
 *      Author: tla001
 */
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/mman.h>
#include "vpu_test.h"
#include "capvideo.h"
#include "ipuprocess.h"

unsigned char *yuv420=NULL;
unsigned char *yuv422=NULL;

void virtualCapInit(){
	yuv420=(unsigned char *)malloc(640* 480* 3/2* sizeof(char));
	initVideo();
	initIPU();
	if(startCapture()<0){
		printf("start capture failed\n");
		closeVideo();
		exit(-1);
	}
}
void virualCapStart(){
	int ret,len;
	ret=getFrame((void **)&yuv422,&len);
	if(ret<0){
		printf("getFrame exit\n");
		exit(-1);
	}
	printf("getFrame return len=%d\n",len);
	IPUConvent(yuv422,yuv420);
	ret=backFrame();
	if(ret<0){
		printf("backFrame exit\n");
		exit(-1);
	}
}
int read_virtual_file(struct encode *enc)
{
	u32 y_addr, u_addr, v_addr;
	struct frame_buf *pfb = enc->pfbpool[enc->src_fbid];
	int divX, divY;
	int src_fd = enc->cmdl->src_fd;
	int format = enc->mjpg_fmt;
	int chromaInterleave = enc->cmdl->chromaInterleave;
	int img_size, y_size, c_size;
	int ret = 1;

	if (enc->src_picwidth != pfb->strideY) {
		err_msg("Make sure src pic width is a multiple of 16\n");
		return -1;
	}

	divX = (format == MODE420 || format == MODE422) ? 2 : 1;
	divY = (format == MODE420 || format == MODE224) ? 2 : 1;

	y_size = enc->src_picwidth * enc->src_picheight;
	c_size = y_size / divX / divY;
	img_size = y_size + c_size * 2;

	y_addr = pfb->addrY + pfb->desc.virt_uaddr - pfb->desc.phy_addr;
	u_addr = pfb->addrCb + pfb->desc.virt_uaddr - pfb->desc.phy_addr;
	v_addr = pfb->addrCr + pfb->desc.virt_uaddr - pfb->desc.phy_addr;

//	if (img_size == pfb->desc.size) {
//		ret = freadn(src_fd, (void *)y_addr, img_size);
//	} else {
//		ret = freadn(src_fd, (void *)y_addr, y_size);
//		if (chromaInterleave == 0) {
//			ret = freadn(src_fd, (void *)u_addr, c_size);
//			ret = freadn(src_fd, (void *)v_addr, c_size);
//		} else {
//			ret = freadn(src_fd, (void *)u_addr, c_size * 2);
//		}
//	}
	//printf("here 2\n");
	virualCapStart();
	//printf("here 3\n");
	memcpy(y_addr,yuv420,y_size);
	memcpy(u_addr,yuv420+y_size,y_size/4);
	memcpy(v_addr,yuv420+y_size*5/4,y_size/4);
	return ret;
}



