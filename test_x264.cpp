// Quick test to verify x264 integration
#include <iostream>
#include <x264.h>

int main() {
    std::cout << "Testing x264 integration..." << std::endl;
    
    // Test x264 library is linked correctly
    std::cout << "x264 build: " << X264_BUILD << std::endl;
    
    // Test basic x264 functionality  
    x264_param_t param;
    x264_param_default_preset(&param, "ultrafast", "zerolatency");
    param.i_width = 320;
    param.i_height = 240;
    param.i_fps_num = 30;
    param.i_fps_den = 1;
    
    x264_t* encoder = x264_encoder_open(&param);
    if (encoder) {
        std::cout << "x264 encoder test: SUCCESS!" << std::endl;
        x264_encoder_close(encoder);
        return 0;
    } else {
        std::cout << "x264 encoder test: FAILED" << std::endl;
        return 1;
    }
}