#ifndef SIGNLANG_EYES_VIDEO_FRONTEND_VIDEO_PROCESSOR_HPP
#define SIGNLANG_EYES_VIDEO_FRONTEND_VIDEO_PROCESSOR_HPP

#include "v4l2_capture_device.hpp"
#include "video_format.hpp"

#include "iox2/bb/slice.hpp"
#include "turbojpeg.h"

#include <cstdint>
#include <vector>

namespace signlang::video_frontend {

  class VideoProcessor {
  public:
    VideoProcessor(VideoFormat capture_format, VideoFormat output_format);
    ~VideoProcessor();

    VideoProcessor(const VideoProcessor&) = delete;
    auto operator=(const VideoProcessor&) -> VideoProcessor& = delete;
    VideoProcessor(VideoProcessor&&) = delete;
    auto operator=(VideoProcessor&&) -> VideoProcessor& = delete;

    auto capture_format() const -> VideoFormat;
    auto output_format() const -> VideoFormat;
    auto max_output_size_bytes(std::uint32_t capture_max_frame_size_bytes) const -> std::uint32_t;
    auto output_size_bytes(const CapturedVideoFrame& captured_frame) const -> std::uint32_t;
    void process(const CapturedVideoFrame& captured_frame, iox2::bb::MutableSlice<std::uint8_t> output_payload) const;

  private:
    static constexpr auto kRgbBytesPerPixel = std::uint32_t{3};

    auto rgb_output_size_bytes() const -> std::uint32_t;
    auto yuyv_capture_size_bytes() const -> std::uint32_t;
    auto rgb_capture_size_bytes() const -> std::uint32_t;
    void initialize_resize_maps();
    void yuyv_to_resized_rgb(const CapturedVideoFrame& captured_frame,
                             iox2::bb::MutableSlice<std::uint8_t> output_payload) const;
    void mjpeg_to_resized_rgb(const CapturedVideoFrame& captured_frame,
                              iox2::bb::MutableSlice<std::uint8_t> output_payload) const;
    void resize_rgb(const std::uint8_t* input_data, iox2::bb::MutableSlice<std::uint8_t> output_payload) const;

    VideoFormat capture_format_;
    VideoFormat output_format_;
    std::vector<std::uint32_t> source_x_indices_;
    std::vector<std::uint32_t> source_y_indices_;
    mutable std::vector<std::uint8_t> mjpeg_rgb_buffer_;
    tjhandle jpeg_decompressor_;
  };

} // namespace signlang::video_frontend

#endif // SIGNLANG_EYES_VIDEO_FRONTEND_VIDEO_PROCESSOR_HPP
