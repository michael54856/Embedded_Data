#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/fb.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <sys/ioctl.h>
#include <unistd.h>

struct framebuffer_info
{
    uint32_t bits_per_pixel;    // depth of framebuffer
    uint32_t xres_virtual;      // how many pixel in a row in virtual screen
};

struct framebuffer_info get_framebuffer_info ( const char *framebuffer_device_path );

int main ( int argc, const char *argv[] )
{
    // variable to store the frame get from video stream
    cv::Mat frame;

    // open video stream device
    cv::VideoCapture camera ( 2 );

    // get info of the framebuffer
    framebuffer_info fb_info = get_framebuffer_info("/dev/fb0");

    // open the framebuffer device
    std::ofstream ofs("/dev/fb0", std::ios::out | std::ios::binary);

    if( !camera.isOpened())
    {
        std::cerr << "Could not open video device." << std::endl;
        return 1;
    }

    // set property of the frame
    camera.set(cv::CAP_PROP_FRAME_WIDTH, fb_info.xres_virtual);

    while ( true )
    {
        camera >> frame;
        if (frame.empty()) {
            std::cerr << "Error: Could not grab a frame." << std::endl;
            break;
        }

        cv::Size2f frame_size = frame.size();

        // Convert BGR to BGR565 (16-bit format)
        cv::Mat frame_16bit;
        cv::cvtColor(frame, frame_16bit, cv::COLOR_BGR2BGR565);

        // Calculate the padding to center the image on the screen
        int x_offset = (fb_info.xres_virtual - frame_16bit.cols) / 2;
        int y_offset = (fb_info.xres_virtual - frame_16bit.rows) / 2;

        // Output the video frame to framebuffer row by row
        for ( int y = 0; y < frame_size.height; y++ )
        {
            // Move to the correct position in the framebuffer with padding
            ofs.seekp(((y + y_offset) * fb_info.xres_virtual + x_offset) * (fb_info.bits_per_pixel / 8));

            // Write the frame to the framebuffer
            ofs.write(reinterpret_cast<const char*>(frame_16bit.ptr(y)), frame_16bit.cols * (fb_info.bits_per_pixel / 8));
        }
    }

    camera.release();
    return 0;
}

struct framebuffer_info get_framebuffer_info ( const char *framebuffer_device_path )
{
    struct framebuffer_info fb_info;        // Used to return the required attrs.
    struct fb_var_screeninfo screen_info;   // Used to get attributes of the device from OS kernel.

    // open deive with linux system call "open( )"
    // https://man7.org/linux/man-pages/man2/open.2.html
	int fd = open(framebuffer_device_path, O_RDWR);
    if (fd == -1) {
        std::cerr << "Error: Cannot open framebuffer device." << std::endl;
        exit(1);
    }

    // get attributes of the framebuffer device thorugh linux system call "ioctl()"
    // the command you would need is "FBIOGET_VSCREENINFO"
    // https://man7.org/linux/man-pages/man2/ioctl.2.html
    // https://www.kernel.org/doc/Documentation/fb/api.txt
	if (ioctl(fd, FBIOGET_VSCREENINFO, &screen_info)) {
        std::cerr << "Error reading variable information." << std::endl;
        close(fd);
        exit(2);
    }

    // put the required attributes in variable "fb_info" you found with "ioctl() and return it."
    // fb_info.xres_virtual = ......
    // fb_info.bits_per_pixel = ......
	fb_info.xres_virtual = screen_info.xres_virtual;
    fb_info.bits_per_pixel = screen_info.bits_per_pixel;

    close(fd);
    return fb_info;
};