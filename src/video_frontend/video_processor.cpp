#include "video_processor.hpp"

#include "video_frame.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace signlang::video_frontend {
  namespace {

    constexpr auto kYuyvBytesPerPixel = std::uint32_t{2};

    auto checked_size_bytes(VideoFormat format, std::uint32_t bytes_per_pixel, const char* label) -> std::uint32_t {
      const auto size_bytes = static_cast<std::uint64_t>(format.width) * format.height * bytes_per_pixel;
      if (size_bytes > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string{label} + " video frame exceeds supported payload size");
      }

      return static_cast<std::uint32_t>(size_bytes);
    }

    auto is_even(std::uint32_t value) -> bool { return (value % 2) == 0; }

    auto clamp_to_u8(int value) -> std::uint8_t {
      return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
    }

    void yuv_to_rgb(std::uint8_t y, std::uint8_t u, std::uint8_t v, std::uint8_t* output) {
      const auto c = static_cast<int>(y) - 16;
      const auto d = static_cast<int>(u) - 128;
      const auto e = static_cast<int>(v) - 128;
      output[0] = clamp_to_u8((298 * c + 409 * e + 128) >> 8);
      output[1] = clamp_to_u8((298 * c - 100 * d - 208 * e + 128) >> 8);
      output[2] = clamp_to_u8((298 * c + 516 * d + 128) >> 8);
    }

  } // namespace

  VideoProcessor::VideoProcessor(VideoFormat capture_format, VideoFormat output_format) :
      capture_format_{capture_format}, output_format_{output_format}, jpeg_decompressor_{nullptr} {
    if (output_format_.pixel_format != kPixelFormatRgb24) {
      throw std::runtime_error("Video output pixel format must be RGB24");
    }

    if (capture_format_.pixel_format != kPixelFormatYuyv && capture_format_.pixel_format != kPixelFormatMjpeg) {
      throw std::runtime_error("Video capture pixel format must be YUYV or MJPEG");
    }

    if (capture_format_.pixel_format == kPixelFormatYuyv && !is_even(capture_format_.width)) {
      throw std::runtime_error("YUYV capture width must be even");
    }

    if (capture_format_.pixel_format == kPixelFormatMjpeg) {
      jpeg_decompressor_ = tjInitDecompress();
      if (jpeg_decompressor_ == nullptr) {
        throw std::runtime_error("Failed to initialize TurboJPEG decompressor");
      }
      mjpeg_rgb_buffer_.resize(rgb_capture_size_bytes());
    }

    initialize_resize_maps();
  }

  VideoProcessor::~VideoProcessor() {
    if (jpeg_decompressor_ != nullptr) {
      static_cast<void>(tjDestroy(jpeg_decompressor_));
    }
  }

  auto VideoProcessor::capture_format() const -> VideoFormat { return capture_format_; }

  auto VideoProcessor::output_format() const -> VideoFormat { return output_format_; }

  auto VideoProcessor::max_output_size_bytes(std::uint32_t capture_max_frame_size_bytes) const -> std::uint32_t {
    (void)capture_max_frame_size_bytes;
    return rgb_output_size_bytes();
  }

  auto VideoProcessor::output_size_bytes(const CapturedVideoFrame& captured_frame) const -> std::uint32_t {
    (void)captured_frame;
    return rgb_output_size_bytes();
  }

  void VideoProcessor::process(const CapturedVideoFrame& captured_frame,
                               iox2::bb::MutableSlice<std::uint8_t> output_payload) const {
    if (capture_format_.pixel_format == kPixelFormatYuyv) {
      yuyv_to_resized_rgb(captured_frame, output_payload);
      return;
    }

    mjpeg_to_resized_rgb(captured_frame, output_payload);
  }

  auto VideoProcessor::rgb_output_size_bytes() const -> std::uint32_t {
    return checked_size_bytes(output_format_, kRgbBytesPerPixel, "RGB");
  }

  auto VideoProcessor::yuyv_capture_size_bytes() const -> std::uint32_t {
    return checked_size_bytes(capture_format_, kYuyvBytesPerPixel, "YUYV");
  }

  auto VideoProcessor::rgb_capture_size_bytes() const -> std::uint32_t {
    return checked_size_bytes(capture_format_, kRgbBytesPerPixel, "RGB");
  }

  void VideoProcessor::initialize_resize_maps() {
    source_y_indices_.resize(output_format_.height);
    for (std::uint32_t output_y = 0; output_y < output_format_.height; ++output_y) {
      source_y_indices_[output_y] =
          static_cast<std::uint32_t>((static_cast<std::uint64_t>(output_y) * capture_format_.height) /
                                     output_format_.height);
    }

    source_x_indices_.resize(output_format_.width);
    for (std::uint32_t output_x = 0; output_x < output_format_.width; ++output_x) {
      source_x_indices_[output_x] =
          static_cast<std::uint32_t>((static_cast<std::uint64_t>(output_x) * capture_format_.width) /
                                     output_format_.width);
    }
  }

  void VideoProcessor::yuyv_to_resized_rgb(const CapturedVideoFrame& captured_frame,
                                           iox2::bb::MutableSlice<std::uint8_t> output_payload) const {
    const auto required_input_size = yuyv_capture_size_bytes();
    if (captured_frame.size_bytes < required_input_size) {
      throw std::runtime_error("Captured YUYV frame is smaller than expected");
    }

    const auto required_output_size = rgb_output_size_bytes();
    if (required_output_size > output_payload.number_of_elements()) {
      throw std::runtime_error("RGB video frame exceeds loaned output payload size");
    }

    const auto* input_data = captured_frame.data;
    auto* output_data = output_payload.data();
    const auto capture_stride_bytes = capture_format_.width * kYuyvBytesPerPixel;
    const auto output_stride_bytes = output_format_.width * kRgbBytesPerPixel;

    for (std::uint32_t output_y = 0; output_y < output_format_.height; ++output_y) {
      const auto* source_row =
          input_data + (static_cast<std::uint64_t>(source_y_indices_[output_y]) * capture_stride_bytes);
      auto* output_row = output_data + (static_cast<std::uint64_t>(output_y) * output_stride_bytes);

      for (std::uint32_t output_x = 0; output_x < output_format_.width; ++output_x) {
        const auto source_x = source_x_indices_[output_x];
        const auto* source_pair = source_row + (static_cast<std::uint64_t>(source_x / 2) * 4U);
        const auto y_value = (source_x % 2U) == 0U ? source_pair[0] : source_pair[2];
        auto* output_pixel = output_row + (static_cast<std::uint64_t>(output_x) * kRgbBytesPerPixel);

        yuv_to_rgb(y_value, source_pair[1], source_pair[3], output_pixel);
      }
    }
  }

  void VideoProcessor::mjpeg_to_resized_rgb(const CapturedVideoFrame& captured_frame,
                                            iox2::bb::MutableSlice<std::uint8_t> output_payload) const {
    if (jpeg_decompressor_ == nullptr) {
      throw std::runtime_error("TurboJPEG decompressor is not initialized");
    }

    int jpeg_width = 0;
    int jpeg_height = 0;
    int jpeg_subsampling = 0;
    int jpeg_colorspace = 0;
    if (tjDecompressHeader3(jpeg_decompressor_, captured_frame.data, captured_frame.size_bytes, &jpeg_width,
                            &jpeg_height, &jpeg_subsampling, &jpeg_colorspace) != 0) {
      throw std::runtime_error(std::string{"Failed to read MJPEG header: "} + tjGetErrorStr2(jpeg_decompressor_));
    }

    if (jpeg_width != static_cast<int>(capture_format_.width) ||
        jpeg_height != static_cast<int>(capture_format_.height)) {
      throw std::runtime_error("MJPEG frame dimensions do not match negotiated capture format");
    }

    const auto pitch = static_cast<int>(capture_format_.width * kRgbBytesPerPixel);
    if (tjDecompress2(jpeg_decompressor_, captured_frame.data, captured_frame.size_bytes, mjpeg_rgb_buffer_.data(),
                      jpeg_width, pitch, jpeg_height, TJPF_RGB, TJFLAG_FASTDCT) != 0) {
      throw std::runtime_error(std::string{"Failed to decode MJPEG frame: "} + tjGetErrorStr2(jpeg_decompressor_));
    }

    resize_rgb(mjpeg_rgb_buffer_.data(), output_payload);
  }

  void VideoProcessor::resize_rgb(const std::uint8_t* input_data,
                                  iox2::bb::MutableSlice<std::uint8_t> output_payload) const {
    const auto required_output_size = rgb_output_size_bytes();
    if (required_output_size > output_payload.number_of_elements()) {
      throw std::runtime_error("RGB video frame exceeds loaned output payload size");
    }

    const auto capture_stride_bytes = capture_format_.width * kRgbBytesPerPixel;
    const auto output_stride_bytes = output_format_.width * kRgbBytesPerPixel;
    auto* output_data = output_payload.data();

    for (std::uint32_t output_y = 0; output_y < output_format_.height; ++output_y) {
      const auto* source_row =
          input_data + (static_cast<std::uint64_t>(source_y_indices_[output_y]) * capture_stride_bytes);
      auto* output_row = output_data + (static_cast<std::uint64_t>(output_y) * output_stride_bytes);

      for (std::uint32_t output_x = 0; output_x < output_format_.width; ++output_x) {
        const auto* source_pixel =
            source_row + (static_cast<std::uint64_t>(source_x_indices_[output_x]) * kRgbBytesPerPixel);
        auto* output_pixel = output_row + (static_cast<std::uint64_t>(output_x) * kRgbBytesPerPixel);
        output_pixel[0] = source_pixel[0];
        output_pixel[1] = source_pixel[1];
        output_pixel[2] = source_pixel[2];
      }
    }
  }

} // namespace signlang::video_frontend
