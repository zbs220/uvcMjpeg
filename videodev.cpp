#include "videodev.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <QDebug>

struct buffer
{
    void *start;
    size_t length;
};

struct buffer *buffers = NULL;


VideoDev::VideoDev(const QString & dev)
{
    m_fd = -1;
    img = QImage(PIC_WIDTH, PIC_HEIGHT, QImage::Format_RGB888);

    b_avilable = false;
    if(openDev(dev.toStdString().data())){
        if(configDev() == 0){
            b_avilable = true;
        }
    }

    qDebug() << "##############" << dev << " ready ?" << b_avilable << "##############";
}

VideoDev::~VideoDev()
{
    closeDev();
}

bool VideoDev::openDev(const char * dev)
{
    if (m_fd != -1){  //不需要重复打开
        return false;
    }

    printf("open %s\n", dev);
    m_fd = open(dev, O_RDWR);
    if (m_fd == -1){
        printf("open error.\n");
        return false;
    }

    return true;
}

static int enum_frame_intervals(int fd, int pixfmt, int width, int height)
{
    int ret;
    struct v4l2_frmivalenum fival;

    memset(&fival, 0, sizeof(struct v4l2_frmivalenum));
    fival.index = 0;
    fival.pixel_format = pixfmt;
    fival.width = width;
    fival.height = height;

    printf("\tTime interval between frame: ");
    while ((ret = ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0)
    {
        if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
        {
            printf("%u/%u, ",
                   fival.discrete.numerator, fival.discrete.denominator);
        }
        else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS)
        {
            printf("{min { %u/%u } .. max { %u/%u } }, ",
                   fival.stepwise.min.numerator, fival.stepwise.min.numerator,
                   fival.stepwise.max.denominator, fival.stepwise.max.denominator);
            break;
        }
        else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE)
        {
            printf("{min { %u/%u } .. max { %u/%u } / "
                   "stepsize { %u/%u } }, ",
                   fival.stepwise.min.numerator, fival.stepwise.min.denominator,
                   fival.stepwise.max.numerator, fival.stepwise.max.denominator,
                   fival.stepwise.step.numerator, fival.stepwise.step.denominator);
            break;
        }
        fival.index++;
    }
    printf("\n");

    if (ret != 0 && errno != EINVAL)
    {
        printf("ERROR enumerating frame intervals: %d\n", errno);
        return errno;
    }

    return 0;
}

static int enum_frame_sizes(int fd, int pixfmt)
{
    int ret;
    struct v4l2_frmsizeenum fsize;

    memset(&fsize, 0, sizeof(struct v4l2_frmsizeenum));
    fsize.index = 0;
    fsize.pixel_format = pixfmt;
    while ((ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0)
    {
        if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
        {
            printf("{ discrete: width = %u, height = %u }\n",
                   fsize.discrete.width, fsize.discrete.height);

            ret = enum_frame_intervals(fd, pixfmt,
                                       fsize.discrete.width,
                                       fsize.discrete.height);
            if (ret != 0)
            {
                printf("  Unable to enumerate frame sizes.\n");
            }
        }
        else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
        {
            printf("{ continuous: min { width = %u, height = %u } .. "
                   "max { width = %u, height = %u } }\n",
                   fsize.stepwise.min_width, fsize.stepwise.min_height,
                   fsize.stepwise.max_width, fsize.stepwise.max_height);
            printf("  Refusing to enumerate frame intervals.\n");
            break;
        }
        else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
        {
            printf("{ stepwise: min { width = %u, height = %u } .. "
                   "max { width = %u, height = %u } / "
                   "stepsize { width = %u, height = %u } }\n",
                   fsize.stepwise.min_width, fsize.stepwise.min_height,
                   fsize.stepwise.max_width, fsize.stepwise.max_height,
                   fsize.stepwise.step_width, fsize.stepwise.step_height);
            printf("  Refusing to enumerate frame intervals.\n");
            break;
        }
        fsize.index++;
    }

    if (ret != 0 && errno != EINVAL)
    {
        printf("ERROR enumerating frame sizes: %d\n", errno);
        return errno;
    }

    return 0;
}



static int camera_query_cap(int fd)
{
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(struct v4l2_capability));
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        perror("query capabilities failed");
        exit(EXIT_FAILURE);
    }

    printf("driver:\t\t%s\n", cap.driver);
    printf("card:\t\t%s\n", cap.card);
    printf("buf info:\t%s\n", cap.bus_info);
    printf("version:\t%d\n", cap.version);
    printf("capabilities:\t%x\n", cap.capabilities);

    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE)
    {
        printf("capabilities:\tsupport capture\n");
    }

    if ((cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING)
    {
        printf("capabilities:\tsupport streaming\n");
    }

    //判断设备厂家信息
    if(NULL != strstr((const char *)cap.card, SUOER_USED_CAM_DESCRIPTION)){
        return DEVICES_MANUFACTURER_SUOER;
    }else{
        return DEVICES_MANUFACTURER_OTHER;
    }
}

static void camera_query_cur_fps(int fd)
{
    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(struct v4l2_streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_PARM, &streamparm) == -1){
        perror("get stream parameters failed");
        exit(EXIT_FAILURE);
    }

    printf("current fps:\t%u frames per %u second\n",
            streamparm.parm.capture.timeperframe.denominator,
            streamparm.parm.capture.timeperframe.numerator);

    if ((streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) == V4L2_CAP_TIMEPERFRAME){
        printf("capabilities:\t support pragrammable frame rates\n");
    }

}

static int camera_query_support_format(int fd)
{
    int ret;
    struct v4l2_fmtdesc fmt;

    memset(&fmt, 0, sizeof(struct v4l2_fmtdesc));
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    printf("camera format ->\n");
    while ((ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmt)) == 0)
    {
        fmt.index++;
        printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",
               fmt.pixelformat & 0xFF,
               (fmt.pixelformat >> 8) & 0xFF,
               (fmt.pixelformat >> 16) & 0xFF,
               (fmt.pixelformat >> 24) & 0xFF,
               fmt.description);

        ret = enum_frame_sizes(fd, fmt.pixelformat);
        if (ret != 0)
        {
            printf("  Unable to enumerate frame sizes.\n");
        }
    }

    if (errno != EINVAL)
    {
        printf("ERROR enumerating frame formats: %d\n", errno);
        return errno;
    }

    return 0;
}

int VideoDev::configDev()
{
    u32 n_buffers = 0;

    //首先默认支持MJPEG格式，后面再判断是否支持，如果支持且使用MJPEG格式，则需要MJEPG解码
    m_IsMJPEG = true;
    printf("Default MJPEG format !\n");

    //
    m_Manufacturer = camera_query_cap(m_fd);
    printf("m_Manufacturer = %d\n", m_Manufacturer);
    camera_query_cur_fps(m_fd);
    camera_query_support_format(m_fd);

    //显示所有支持的格式
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index=0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("Supportformat:\n");
    while(ioctl(m_fd, VIDIOC_ENUM_FMT, &fmtdesc)!=-1)
    {
        printf("%d.%s\n",fmtdesc.index+1,fmtdesc.description);
        fmtdesc.index++;
        if(strstr((char *)fmtdesc.description, (char *)("YUV")))
        {
            m_IsMJPEG = false;
            printf("Not MJPEG format !\n");
            break;
        }
    }

    //设置图像格式
    struct v4l2_format fmt;
    m_IsMJPEG = true;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = m_IsMJPEG ? V4L2_PIX_FMT_MJPEG : V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.width = PIC_WIDTH;
    fmt.fmt.pix.height = PIC_HEIGHT;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        if (errno==EINVAL)
            printf(" set  format JPEG!\n");
    }

    //视频分配捕获内存
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    //申请缓冲,count是申请的数量
    if (ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0){
        printf("failture VIDIOC_REQBUFS\n");
        return -1;
    }
    printf("VIDIOC_REQBUFS ok\n");

    //内存中建立对应空间,获取缓冲帧的地址、长度
    buffers = (buffer *)calloc(req.count, sizeof(*buffers));  //在内存的动态存储区中分配n个长度为size的连续空间，函数返回一个指向分配起始地址的指针
    if (!buffers){
        printf("Out of memory\n");
        return -1;
    }
    printf("buffer memory ok\n");

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers){

        struct v4l2_buffer buf;	  //驱动中的一帧
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;  // 要获取内核视频缓冲区的信息编号

        if (-1 == ioctl(m_fd, VIDIOC_QUERYBUF, &buf)){ //映射用户空间
            printf("VIDIOC_QUERYBUF error\n");
            return -1;
        }
        buffers[n_buffers].length = buf.length;
        printf("buf.length = %d\n", buf.length);

        // 把内核空间缓冲区映射到用户空间缓冲区
        buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
        if (MAP_FAILED == buffers[n_buffers].start){
            printf("mmap failed\n");
            return -1;
        }
    }

    //投放一个空的视频缓冲区到视频缓冲区输入队列中
    //把四个缓冲帧放入队列，并启动数据流

    // 将缓冲帧放入队列
    enum v4l2_buf_type type;
    for (int i=0; i<n_buffers; ++i)
    {
        struct v4l2_buffer buf;
        memset (&buf, 0, sizeof(v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;  //指定要投放到视频输入队列中的内核空间视频缓冲区的编号

        if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf))  //申请到的缓冲进入列队
            printf("VIDIOC_QBUF failed\n");
    }

    //开始捕捉图像数据
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(m_fd, VIDIOC_STREAMON, &type)){
        printf("VIDIOC_STREAMON failed\n");
        return -1;
    }

    return 0;
}

bool VideoDev::closeDev()
{
    if (m_fd == -1)
    {
        return false;
    }

    //停止捕捉图像
    printf("close Dev");
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(m_fd, VIDIOC_STREAMOFF, &type))
    {
        printf("VIDIOC_STREAMOFF failed\n");
    }

    //关闭内存映射
    for (int i= 0; i<4; i++)
    {
        if (-1 == munmap(buffers[i].start, buffers[i].length))
        {
            free(buffers);
        }
    }

    if (m_fd != -1)
    {
        close(m_fd);
        m_fd = -1;
        return true;
    }
    return false;
}

void VideoDev::getImage(QImage &img)
{
    if(ReadFrame() == 0){

        Yuyv2RGB24(pSrc, img.bits(), PIC_WIDTH, PIC_HEIGHT);

        releaseFrame();
    }
}

void VideoDev::getPix(QPixmap &pix)
{
    if(ReadFrame() == 0){
        pix.loadFromData(pSrc, PIC_WIDTH*PIC_HEIGHT*PIC_BYTE);
        releaseFrame();
    }
}

int VideoDev::ReadFrame(void)
{
    if (m_fd == -1){
        return false;
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(v4l2_buffer));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(m_fd, VIDIOC_DQBUF, &buf) == -1){
        printf("VIDIOC_DQBUF failture\n"); //出列采集的帧缓冲
        return -1;
    }

    pSrc = (u8 *)buffers[buf.index].start;
    index = buf.index;

    return 0;
}

int VideoDev::releaseFrame()
{
    //printf("releaseFrame: \n");
    //printf("index = %d\n", index);
    if(index != -1){
        struct v4l2_buffer queue_buf;
        memset(&queue_buf, 0, sizeof(queue_buf));

        queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        queue_buf.memory = V4L2_MEMORY_MMAP;
        queue_buf.index = index;

        if(-1 == ioctl(m_fd, VIDIOC_QBUF, &queue_buf)){
            printf("releaseFrame error !\n");
            return -1;
        }
        return 0;
    }

    return -1;
}

#if 1
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

void VideoDev::Yuyv2RGB24(u8 *BufYuyv, u8 *BufRGB, int width, int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;
    for(in = 0; in < width * height * 2; in += 4) {
        pixel_16 =
            BufYuyv[in + 3] << 24 |
            BufYuyv[in + 2] << 16 |
            BufYuyv[in + 1] <<  8 |
            BufYuyv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;
        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        BufRGB[out++] = pixel_24[0];
        BufRGB[out++] = pixel_24[1];
        BufRGB[out++] = pixel_24[2];
        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        BufRGB[out++] = pixel_24[0];
        BufRGB[out++] = pixel_24[1];
        BufRGB[out++] = pixel_24[2];
    }
}
#else

bool sign3 = false;
int yuvtorgb(int y, int u, int v)
{
     unsigned int pixel24 = 0;
     unsigned char *pixel = (unsigned char *)&pixel24;
     int r, g, b;
     static long int ruv, guv, buv;

     if(sign3)
     {
         sign3 = false;
         ruv = 1159*(v-128);
         guv = 380*(u-128) + 813*(v-128);
         buv = 2018*(u-128);
     }

     r = (1164*(y-16) + ruv) / 1000;
     g = (1164*(y-16) - guv) / 1000;
     b = (1164*(y-16) + buv) / 1000;

     if(r > 255) r = 255;
     if(g > 255) g = 255;
     if(b > 255) b = 255;
     if(r < 0) r = 0;
     if(g < 0) g = 0;
     if(b < 0) b = 0;

     pixel[0] = r;
     pixel[1] = g;
     pixel[2] = b;

     return pixel24;
}

//int yuvtorgb0(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
void VideoDev::Yuyv2RGB24(u8 *yuv, u8 *rgb, int width, int height)
{
     unsigned int in, out;
     int y0, u, y1, v;
     unsigned int pixel24;
     unsigned char *pixel = (unsigned char *)&pixel24;
     unsigned int size = width*height*2;

     for(in = 0, out = 0; in < size; in += 4, out += 6)
     {
          y0 = yuv[in+0];
          u  = yuv[in+1];
          y1 = yuv[in+2];
          v  = yuv[in+3];

          sign3 = true;
          pixel24 = yuvtorgb(y0, u, v);
          rgb[out+0] = pixel[0];    //for QT
          rgb[out+1] = pixel[1];
          rgb[out+2] = pixel[2];
          //rgb[out+0] = pixel[2];  //for iplimage
          //rgb[out+1] = pixel[1];
          //rgb[out+2] = pixel[0];

          //sign3 = true;
          pixel24 = yuvtorgb(y1, u, v);
          rgb[out+3] = pixel[0];
          rgb[out+4] = pixel[1];
          rgb[out+5] = pixel[2];
          //rgb[out+3] = pixel[2];
          //rgb[out+4] = pixel[1];
          //rgb[out+5] = pixel[0];
     }
}
#endif
