#include "linux/signal.h"
#include <asm-generic/int-ll64.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>


static void display_demo_1 (unsigned char *frame, unsigned int width, unsigned int height, unsigned int stride)
{
unsigned int xcoi, ycoi;
unsigned char wRed, wBlue, wGreen;
unsigned int iPixelAddr = 0;
for(ycoi = 0; ycoi < height; ycoi++) {

for(xcoi = 0; xcoi < (width * 3); xcoi += 3) {

if (((xcoi / 4) & 0x20) ^ (ycoi & 0x20)) {
wRed = 255;
wGreen = 255;
wBlue = 255;
} else {
wRed = 0;
wGreen = 0;
wBlue = 0;
}

frame[xcoi + iPixelAddr + 0] = wRed;
frame[xcoi + iPixelAddr + 1] = wGreen;
frame[xcoi + iPixelAddr + 2] = wBlue;
}

iPixelAddr += stride;
}
}

static void display_demo_2 (unsigned char *frame, unsigned int width, unsigned int height, unsigned int stride)
{
unsigned int xcoi, ycoi;
unsigned int iPixelAddr = 0;
unsigned char wRed, wBlue, wGreen;
unsigned int xInt;

xInt = width * 3 / 8;
for(ycoi = 0; ycoi < height; ycoi++) {

for(xcoi = 0; xcoi < (width*3); xcoi+=3) {

if (xcoi < xInt) { //White color
wRed = 255;
wGreen = 255;
wBlue = 255;
}
else if ((xcoi >= xInt) && (xcoi < xInt*2)) { //YELLOW color
wRed = 255;
wGreen = 255;
wBlue = 0;
}
else if ((xcoi >= xInt * 2) && (xcoi < xInt * 3)) {//CYAN color
wRed = 0;
wGreen = 255;
wBlue = 255;
}
else if ((xcoi >= xInt * 3) && (xcoi < xInt * 4)) {//GREEN color
wRed = 0;
wGreen = 255;
wBlue = 0;
}
else if ((xcoi >= xInt * 4) && (xcoi < xInt * 5)) {//MAGENTA color
wRed = 255;
wGreen = 0;
wBlue = 255;
}
else if ((xcoi >= xInt * 5) && (xcoi < xInt * 6)) {//RED color
wRed = 255;
wGreen = 0;
wBlue = 0;
}
else if ((xcoi >= xInt * 6) && (xcoi < xInt * 7)) {//BLUE color
wRed = 0;
wGreen = 0;
wBlue = 255;
}
else { //BLACK color
wRed = 0;
 wGreen = 0;
 wBlue = 0;
 }

 frame[xcoi+iPixelAddr + 0] = wRed;
 frame[xcoi+iPixelAddr + 1] = wGreen;
 frame[xcoi+iPixelAddr + 2] = wBlue;
 }

 iPixelAddr += stride;
 }
 }


int main(int argc,char **argv)
{
    int fd;
    struct fb_var_screeninfo vinfo; //可变参数
    struct fb_fix_screeninfo finfo; //固定参数
    unsigned int screensize;                //屏幕大小
    unsigned char *base;                       //映射基地址

    int ret;
    
    fd = open("/dev/fb0",O_RDWR);
    if(fd<0)
    {
        printf("Error: cannot open framebuffer device.\n");
        return fd;
    }

    /*获取frambuffer设备信息*/
    ret = ioctl(fd,FBIOGET_FSCREENINFO,&finfo);
    if(ret)
    {
        printf("Error reading fixed information\n");
        return ret;
    }
    ret = ioctl(fd,FBIOGET_VSCREENINFO,&vinfo);
    if(ret)
    {
        printf("Error reading variable information\n");
        return ret;
    }

    /*mmap映射*/
    screensize = vinfo.yres_virtual * finfo.line_length;
    base = (unsigned char *)mmap(NULL,screensize,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
    if((int)base == -1)
    {
        printf("Error: failed to map framebuffer device to memory.\n");
        return -1;
    }

    memset(base,0,screensize);

    for(;;)
    {
        display_demo_1(base,vinfo.xres,vinfo.yres,finfo.line_length);
        sleep(1);
        display_demo_2(base,vinfo.xres,vinfo.yres,finfo.line_length);
        sleep(1);
    }

    memset(base,0,screensize); 
    munmap(base,screensize);
    close(fd);
    return 0;



}