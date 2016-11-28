#ifndef __CAPVIDEO_H
#define __CAPVIDEO_H

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define USB_VIDEO "/dev/video0"

struct cam_buffer
{
	void* start;
	unsigned int length;
};

extern int fd_cam;
extern int frameIndex;
/************����˳��***************
 * initVideo();
 * startCapture();
 *ע�⣺����������������˳�ʱҪ�ر��豸
 ************ֹͣ˳��***************
 * stopCapture();
 * freeBuffers();
 * closeVideo();
 */
void initVideo();
void closeVideo();
int initBuffers();
int startCapture();
int stopCapture();
int freeBuffers();
int getFrame(void **frame_buf, size_t* len);
int backFrame();
int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb,
		unsigned int width, unsigned int height);
int convert_yuv_to_rgb_pixel(int y, int u, int v);

#endif
