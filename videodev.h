#ifndef VIDEODEV_H
#define VIDEODEV_H

#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include <QPixmap>

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int

#define PIC_WIDTH  640
#define PIC_HEIGHT 480
#define PIC_BYTE    3

#define SUOER_USED_CAM_DESCRIPTION      "USB 2.0 PC Cam"
#define DEVICES_MANUFACTURER_SUOER  0   //索维使用的双目摄像头
#define DEVICES_MANUFACTURER_OTHER  1   //其他厂家的摄像头

class VideoDev
{
public:
    VideoDev(const QString & dev);
    ~VideoDev();

    bool deviceAvilable() {return b_avilable;}
    u8 *getDataPtr(){return pSrc;}
    void getImage(QImage &img);
    void getPix(QPixmap &pix);
    int ReadFrame(void);
    void Yuyv2RGB24(u8 *BufYuyv, u8 *BufRGB, int width, int height);
    int releaseFrame();

private:
    bool openDev(const char * dev);
    int configDev();
    bool closeDev();

public:
    int m_fd;

private:
    bool m_IsMJPEG;     //是否支持MJPEG格式
    u8 m_BufRGB[PIC_WIDTH * PIC_HEIGHT * PIC_BYTE];
    QImage img;

    u8 *pSrc;
    int index;
    bool b_avilable;

    u8  m_Manufacturer; //设备厂家
};

#endif // VIDEODEV_H
