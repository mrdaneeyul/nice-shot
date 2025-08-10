// NiceShot Standalone Video Converter
// Converts raw RGBA frames to H.264 using x264
// Usage: NiceShot_Converter.exe recording.json

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdio>
#include <iomanip>

#ifdef HAVE_X264
#include <x264.h>
#endif

// Simple JSON parser for our specific format
struct RecordingInfo {
    std::string raw_file;
    std::string h264_file;
    std::string mp4_file;
    uint32_t width;
    uint32_t height;
    double fps;
    uint64_t frame_count;
    bool valid;
    
    RecordingInfo() : width(0), height(0), fps(0), frame_count(0), valid(false) {}
};

// Extract value from JSON line (simple parser for our specific format)
std::string extract_json_string(const std::string& line) {
    size_t start = line.find('\"');
    if (start == std::string::npos) return "";
    start = line.find('\"', start + 1);
    if (start == std::string::npos) return "";
    start++;
    
    size_t end = line.find('\"', start);
    if (end == std::string::npos) return "";
    
    return line.substr(start, end - start);
}

double extract_json_number(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return 0;
    
    std::string value_part = line.substr(colon + 1);
    size_t comma = value_part.find(',');
    if (comma != std::string::npos) {
        value_part = value_part.substr(0, comma);
    }
    
    // Remove whitespace and quotes
    value_part.erase(0, value_part.find_first_not_of(" \t\""));
    value_part.erase(value_part.find_last_not_of(" \t\",\n\r") + 1);
    
    try {
        return std::stod(value_part);
    } catch (...) {
        return 0;
    }
}

RecordingInfo parse_recording_json(const std::string& json_path) {
    RecordingInfo info;
    std::ifstream file(json_path);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << json_path << std::endl;
        return info;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("\"raw_file\"") != std::string::npos) {
            info.raw_file = extract_json_string(line);
        }
        else if (line.find("\"target_h264\"") != std::string::npos) {
            info.h264_file = extract_json_string(line);
        }
        else if (line.find("\"target_mp4\"") != std::string::npos) {
            info.mp4_file = extract_json_string(line);
        }
        else if (line.find("\"width\"") != std::string::npos) {
            info.width = static_cast<uint32_t>(extract_json_number(line));
        }
        else if (line.find("\"height\"") != std::string::npos) {
            info.height = static_cast<uint32_t>(extract_json_number(line));
        }
        else if (line.find("\"fps\"") != std::string::npos) {
            info.fps = extract_json_number(line);
        }
        else if (line.find("\"frame_count\"") != std::string::npos) {
            info.frame_count = static_cast<uint64_t>(extract_json_number(line));
        }
    }
    
    info.valid = !info.raw_file.empty() && !info.h264_file.empty() && 
                 info.width > 0 && info.height > 0 && info.frame_count > 0;
    
    return info;
}

// Fast RGBA to YUV420p conversion (same as DLL version)
static void convert_rgba_to_yuv420p_fast(const uint8_t* rgba_data, uint32_t width, uint32_t height, 
                                         uint8_t* y_plane, uint8_t* u_plane, uint8_t* v_plane) {
    const uint32_t uv_width = width / 2;
    
    for (uint32_t y = 0; y < height; y += 2) {
        for (uint32_t x = 0; x < width; x += 2) {
            uint32_t rgba_idx0 = (y * width + x) * 4;
            uint32_t rgba_idx1 = (y * width + x + 1) * 4;
            uint32_t rgba_idx2 = ((y + 1) * width + x) * 4;
            uint32_t rgba_idx3 = ((y + 1) * width + x + 1) * 4;
            
            uint8_t r0 = rgba_data[rgba_idx0 + 0], g0 = rgba_data[rgba_idx0 + 1], b0 = rgba_data[rgba_idx0 + 2];
            uint8_t r1 = rgba_data[rgba_idx1 + 0], g1 = rgba_data[rgba_idx1 + 1], b1 = rgba_data[rgba_idx1 + 2];
            uint8_t r2 = rgba_data[rgba_idx2 + 0], g2 = rgba_data[rgba_idx2 + 1], b2 = rgba_data[rgba_idx2 + 2];
            uint8_t r3 = rgba_data[rgba_idx3 + 0], g3 = rgba_data[rgba_idx3 + 1], b3 = rgba_data[rgba_idx3 + 2];
            
            y_plane[y * width + x] = (77 * r0 + 150 * g0 + 29 * b0) >> 8;
            y_plane[y * width + x + 1] = (77 * r1 + 150 * g1 + 29 * b1) >> 8;
            y_plane[(y + 1) * width + x] = (77 * r2 + 150 * g2 + 29 * b2) >> 8;
            y_plane[(y + 1) * width + x + 1] = (77 * r3 + 150 * g3 + 29 * b3) >> 8;
            
            uint32_t avg_r = (r0 + r1 + r2 + r3) / 4;
            uint32_t avg_g = (g0 + g1 + g2 + g3) / 4;
            uint32_t avg_b = (b0 + b1 + b2 + b3) / 4;
            
            uint32_t uv_idx = (y / 2) * uv_width + (x / 2);
            u_plane[uv_idx] = 128 + ((-43 * avg_r - 84 * avg_g + 127 * avg_b) >> 8);
            v_plane[uv_idx] = 128 + ((127 * avg_r - 106 * avg_g - 21 * avg_b) >> 8);
        }
    }
}

bool convert_raw_to_h264(const RecordingInfo& info) {
    std::cout << "Starting H.264 conversion..." << std::endl;
    std::cout << "Input:  " << info.raw_file << std::endl;
    std::cout << "Output: " << info.h264_file << std::endl;
    std::cout << "Format: " << info.width << "x" << info.height << " @ " << info.fps << " fps" << std::endl;
    std::cout << "Frames: " << info.frame_count << std::endl;
    std::cout << std::endl;
    
#ifdef HAVE_X264
    // Create high-quality x264 encoder
    x264_param_t param;
    x264_param_default_preset(&param, "slow", "film");
    
    param.i_width = info.width;
    param.i_height = info.height;
    param.i_fps_num = static_cast<int>(info.fps * 1000);
    param.i_fps_den = 1000;
    param.i_keyint_max = static_cast<int>(info.fps) * 10;
    param.b_intra_refresh = 0;
    param.rc.i_rc_method = X264_RC_CRF;
    param.rc.f_rf_constant = 18.0f; // Very high quality
    param.i_csp = X264_CSP_I420;
    
    // Maximum quality settings
    param.i_threads = 0; // Use all CPU cores
    param.b_deterministic = 1;
    param.i_sync_lookahead = 60;
    param.rc.i_lookahead = 60;
    param.i_bframe = 16;
    param.i_bframe_adaptive = X264_B_ADAPT_TRELLIS;
    param.analyse.i_me_method = X264_ME_TESA;
    param.analyse.i_subpel_refine = 11;
    
    x264_param_apply_profile(&param, "high");
    
    x264_t* encoder = x264_encoder_open(&param);
    if (!encoder) {
        std::cerr << "Error: Failed to create x264 encoder" << std::endl;
        return false;
    }
    
    // Open files
    FILE* raw_file = fopen(info.raw_file.c_str(), "rb");
    if (!raw_file) {
        std::cerr << "Error: Could not open raw file: " << info.raw_file << std::endl;
        x264_encoder_close(encoder);
        return false;
    }
    
    FILE* h264_file = fopen(info.h264_file.c_str(), "wb");
    if (!h264_file) {
        std::cerr << "Error: Could not create H.264 file: " << info.h264_file << std::endl;
        fclose(raw_file);
        x264_encoder_close(encoder);
        return false;
    }
    
    // Allocate buffers
    x264_picture_t pic_in, pic_out;
    x264_picture_alloc(&pic_in, param.i_csp, param.i_width, param.i_height);
    
    size_t frame_size = info.width * info.height * 4;
    std::vector<uint8_t> rgba_frame(frame_size);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "Encoding with maximum quality settings..." << std::endl;
    
    // Process frames
    for (uint64_t i = 0; i < info.frame_count; i++) {
        size_t read_bytes = fread(rgba_frame.data(), 1, frame_size, raw_file);
        if (read_bytes != frame_size) {
            std::cerr << "Warning: Could only read " << read_bytes << " bytes for frame " << i << std::endl;
            break;
        }
        
        convert_rgba_to_yuv420p_fast(rgba_frame.data(), info.width, info.height,
                                    pic_in.img.plane[0], pic_in.img.plane[1], pic_in.img.plane[2]);
        
        pic_in.i_pts = i;
        
        x264_nal_t* nal;
        int i_nal;
        int encoded_size = x264_encoder_encode(encoder, &nal, &i_nal, &pic_in, &pic_out);
        
        if (encoded_size > 0) {
            for (int j = 0; j < i_nal; j++) {
                fwrite(nal[j].p_payload, 1, nal[j].i_payload, h264_file);
            }
        }
        
        if (i % 60 == 0) {
            auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
            auto elapsed_seconds = std::chrono::duration<double>(elapsed).count();
            double progress = (double)i / info.frame_count * 100.0;
            double fps_encoding = i / elapsed_seconds;
            
            std::cout << "Progress: " << std::fixed << std::setprecision(1) << progress 
                      << "% (" << i << "/" << info.frame_count << " frames, " 
                      << std::setprecision(1) << fps_encoding << " fps)" << std::endl;
        }
    }
    
    // Flush delayed frames
    std::cout << "Flushing delayed frames..." << std::endl;
    int flushed = 0;
    while (1) {
        x264_nal_t* nal;
        int i_nal;
        int frame_size = x264_encoder_encode(encoder, &nal, &i_nal, nullptr, &pic_out);
        if (frame_size <= 0) break;
        
        for (int i = 0; i < i_nal; i++) {
            fwrite(nal[i].p_payload, 1, nal[i].i_payload, h264_file);
        }
        flushed++;
    }
    
    auto total_time = std::chrono::high_resolution_clock::now() - start_time;
    auto total_seconds = std::chrono::duration<double>(total_time).count();
    
    std::cout << std::endl;
    std::cout << "Conversion complete!" << std::endl;
    std::cout << "Total time: " << std::fixed << std::setprecision(1) << total_seconds << " seconds" << std::endl;
    std::cout << "Flushed frames: " << flushed << std::endl;
    std::cout << "Output file: " << info.h264_file << std::endl;
    
    // Cleanup
    x264_picture_clean(&pic_in);
    x264_encoder_close(encoder);
    fclose(raw_file);
    fclose(h264_file);
    
    // Delete raw file to save space
    if (remove(info.raw_file.c_str()) == 0) {
        std::cout << "Deleted raw file to save disk space" << std::endl;
    }
    
    return true;
    
#else
    std::cout << "Error: x264 library not available in this build" << std::endl;
    std::cout << "Alternative: Use FFmpeg directly:" << std::endl;
    std::cout << "ffmpeg -f rawvideo -pix_fmt rgba -s " << info.width << "x" << info.height 
              << " -r " << info.fps << " -i \"" << info.raw_file 
              << "\" -c:v libx264 -preset slow -crf 18 \"" << info.h264_file << "\"" << std::endl;
    return false;
#endif
}

int main(int argc, char* argv[]) {
    std::cout << "================================================" << std::endl;
    std::cout << "NiceShot Standalone Video Converter v1.0" << std::endl;
    std::cout << "================================================" << std::endl;
    std::cout << std::endl;
    
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <recording.json>" << std::endl;
        std::cout << "Example: " << argv[0] << " gameplay_recording.json" << std::endl;
        return 1;
    }
    
    std::string json_path = argv[1];
    std::cout << "Loading recording info from: " << json_path << std::endl;
    
    RecordingInfo info = parse_recording_json(json_path);
    
    if (!info.valid) {
        std::cerr << "Error: Invalid or incomplete recording information in JSON file" << std::endl;
        return 1;
    }
    
    std::cout << "Recording info loaded successfully" << std::endl;
    std::cout << std::endl;
    
    bool success = convert_raw_to_h264(info);
    
    if (success) {
        std::cout << std::endl;
        std::cout << "Conversion completed successfully!" << std::endl;
        std::cout << "H.264 file: " << info.h264_file << std::endl;
        
        if (!info.mp4_file.empty()) {
            std::cout << std::endl;
            std::cout << "To create MP4 with FFmpeg:" << std::endl;
            std::cout << "ffmpeg -r " << info.fps << " -i \"" << info.h264_file 
                      << "\" -c:v copy \"" << info.mp4_file << "\"" << std::endl;
        }
        
        return 0;
    } else {
        std::cerr << std::endl;
        std::cerr << "Conversion failed!" << std::endl;
        return 1;
    }
}