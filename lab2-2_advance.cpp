#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/fb.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <sys/ioctl.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>

struct framebuffer_info
{
    uint32_t bits_per_pixel;    // depth of framebuffer
    uint32_t xres_virtual;      // how many pixel in a row in virtual screen
};

struct framebuffer_info get_framebuffer_info ( const char *framebuffer_device_path );


std::atomic<bool> capture_screenshot(false);
int screenshot_count = 0; // To count screenshots taken
std::string screenshots_dir = "/path/to/sdcard/screenshot/"; // Set your SD card path here
cv::Mat last_frame; 

void screenshot_capture() {
    while (true) {
        if (capture_screenshot) 
		{
            std::string filename = screenshots_dir + "screenshot_" + std::to_string(screenshot_count++) + ".png";
            cv::imwrite(filename, last_frame);
            capture_screenshot = false; // Reset the flag
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Avoid busy waiting
    }
}

int main ( int argc, const char *argv[] )
{
    // variable to store the frame get from video stream
    cv::Mat frame;

    // open video stream device
    // https://docs.opencv.org/3.4.7/d8/dfe/classcv_1_1VideoCapture.html#a5d5f5dacb77bbebdcbfb341e3d4355c1
    cv::VideoCapture camera ( 2 );

    // get info of the framebuffer
    // fb_info = ......
	 framebuffer_info fb_info = get_framebuffer_info("/dev/fb0");

    // open the framebuffer device
    // http://www.cplusplus.com/reference/fstream/ofstream/ofstream/
    std::ofstream ofs("/dev/fb0", std::ios::out | std::ios::binary);

    // check if video stream device is opened success or not
    // https://docs.opencv.org/3.4.7/d8/dfe/classcv_1_1VideoCapture.html#a9d2ca36789e7fcfe7a7be3b328038585
    if( !camera.isOpened())
    {
        std::cerr << "Could not open video device." << std::endl;
        return 1;
    }

    // set propety of the frame
    // https://docs.opencv.org/3.4.7/d8/dfe/classcv_1_1VideoCapture.html#a8c6d8c2d37505b5ca61ffd4bb54e9a7c
    // https://docs.opencv.org/3.4.7/d4/d15/group__videoio__flags__base.html#gaeb8dd9c89c10a5c63c139bf7c4f5704d
	
	camera.set(cv::CAP_PROP_FRAME_WIDTH, fb_info.xres_virtual);
	
	 // Set up for 4:3 video recording
    int video_width = 640;  // Set video width
    int video_height = 480; // Set video height (4:3 aspect ratio)
    cv::VideoWriter video_writer("/path/to/sdcard/screenshot/recording.avi", cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, cv::Size(video_width, video_height));

    // Start the screenshot capture thread
    std::thread screenshot_thread(screenshot_capture);

    while ( true )
    {
        // get video frame from stream
        // https://docs.opencv.org/3.4.7/d8/dfe/classcv_1_1VideoCapture.html#a473055e77dd7faa4d26d686226b292c1
        // https://docs.opencv.org/3.4.7/d8/dfe/classcv_1_1VideoCapture.html#a199844fb74226a28b3ce3a39d1ff6765
        // frame = ......
		camera >> frame;
        if (frame.empty()) {
            std::cerr << "Error: Could not grab a frame." << std::endl;
            continue;
        }
		
		// Calculate new dimensions for 4:3 aspect ratio
        int original_width = frame.cols;
        int original_height = frame.rows;
        float aspect_ratio = static_cast<float>(original_width) / original_height;

        int new_width, new_height;

        // Maintain 4:3 aspect ratio
        if (aspect_ratio > (4.0 / 3.0)) {
            new_width = static_cast<int>(video_height * (4.0 / 3.0));
            new_height = video_height;
        } else {
            new_width = video_width;
            new_height = static_cast<int>(video_width * (3.0 / 4.0));
        }
		
		 // Resize frame to maintain aspect ratio
        cv::Mat resized_frame;
        cv::resize(frame, resized_frame, cv::Size(new_width, new_height));
		
		
		// Create a blank image for the final output
        cv::Mat output_frame(video_height, video_width, CV_8UC3, cv::Scalar(0, 0, 0)); // Black background
        // Calculate the position to place the resized frame on the output frame
        int x_offset = (video_width - new_width) / 2;
        int y_offset = (video_height - new_height) / 2;

        // Copy the resized frame to the center of the output frame
        resized_frame.copyTo(output_frame(cv::Rect(x_offset, y_offset, new_width, new_height)));

        // Start writing the frame to the video file
        video_writer.write(output_frame);

        // get size of the video frame
        // https://docs.opencv.org/3.4.7/d3/d63/classcv_1_1Mat.html#a146f8e8dda07d1365a575ab83d9828d1
        // frame_size = ......
		cv::Size2f frame_size = frame.size();

        // transfer color space from BGR to BGR565 (16-bit image) to fit the requirement of the LCD
        // https://docs.opencv.org/3.4.7/d8/d01/group__imgproc__color__conversions.html#ga397ae87e1288a81d2363b61574eb8cab
        // https://docs.opencv.org/3.4.7/d8/d01/group__imgproc__color__conversions.html#ga4e0972be5de079fed4e3a10e24ef5ef0
		
		// Convert BGR to BGR565 (16-bit format)
        cv::Mat frame_16bit;
        cv::cvtColor(frame, frame_16bit, cv::COLOR_BGR2BGR565);

        // output the video frame to framebufer row by row
        for ( int y = 0; y < frame_size.height; y++ )
        {
            // move to the next written position of output device framebuffer by "std::ostream::seekp()"
            // http://www.cplusplus.com/reference/ostream/ostream/seekp/
			ofs.seekp(y * fb_info.xres_virtual * (fb_info.bits_per_pixel / 8));

            // write to the framebuffer by "std::ostream::write()"
            // you could use "cv::Mat::ptr()" to get the pointer of the corresponding row.
            // you also need to cacluate how many bytes required to write to the buffer
            // http://www.cplusplus.com/reference/ostream/ostream/write/
            // https://docs.opencv.org/3.4.7/d3/d63/classcv_1_1Mat.html#a13acd320291229615ef15f96ff1ff738
            ofs.write(reinterpret_cast<const char*>(frame_16bit.ptr(y)),frame_16bit.cols * (fb_info.bits_per_pixel / 8));
        }
		
		// Check for keyboard input for capturing screenshots
        if (cv::waitKey(1) == 'c') { // Press 'c' to capture screenshot
			last_frame = frame.clone(); // Clone the current frame to last_frame
            capture_screenshot = true; // Set flag to capture
        }
    }

    // closing video stream
    // https://docs.opencv.org/3.4.7/d8/dfe/classcv_1_1VideoCapture.html#afb4ab689e553ba2c8f0fec41b9344ae6
    camera.release ( );

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