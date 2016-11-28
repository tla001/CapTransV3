#ifndef __IPUPROCESS_H
#define __IPUPROCESS_H

#define IPUDEV "/dev/mxc_ipu"

extern unsigned int ipuOutputSize,ipuInputSize;

void IPUConvent(void *in,void *out);
//static unsigned int fmt_to_bpp(unsigned int pixelformat);
void initIPU();
void closeIPU();

unsigned int fmt_to_bpp(unsigned int pixelformat);
#endif
