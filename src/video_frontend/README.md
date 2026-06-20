# video_frontend — Video Capture & Publishing

## Overview

The **video_frontend** module captures video frames from a V4L2 (Video4Linux2) camera device and publishes RGB24 frames as an iceoryx2 byte slice with `VideoFrameMetadata` user-header metadata. It supports YUYV and MJPEG capture, normalizes both to RGB24, and applies optional nearest-neighbor resize.

- **Executable**: `video_frontend` (installed under `bin/`)
- **IPC Pattern**: Publish-Subscribe (producer)
- **Input**: V4L2 camera device (YUYV 4:2:2 or MJPEG)
- **Output**: `iox2::bb::Slice<std::uint8_t>` with `signlang::video_frontend::VideoFrameMetadata` user header on iceoryx2

## File Inventory

| File | Description |
|------|-------------|
| `main.cpp` | Entry point; signal handling (SIGINT/SIGTERM), main capture loop |
| `program_options.{cpp,hpp}` | CLI argument parsing via cxxopts |
| `v4l2_capture_device.{cpp,hpp}` | V4L2 device enumeration, format negotiation, frame capture |
| `video_format.hpp` | `VideoFormat`, `VideoFormatRequest` structs and dimension constants |
| `video_frame.hpp` | `VideoFrameMetadata` IPC user-header definition (shared header) |
| `video_processor.{cpp,hpp}` | YUYV/MJPEG to RGB24 conversion and nearest-neighbor resize |
| `video_publisher.{cpp,hpp}` | iceoryx2 publish-subscribe publisher wrapper with payload management |

## Command-Line Parameters

| Parameter | Default | Range | Description |
|-----------|---------|----|-------------|
| `--device` / `-d` | *(required)* | —  | V4L2 camera device name (e.g., `/dev/video0`) |
| `--service` / `-s` | *(required)* | —  | iceoryx2 publish-subscribe service name for video output |
| `--capture-width` | (device default) | `>0` | Requested camera capture width in pixels |
| `--capture-height` | (device default) | `>0` | Requested camera capture height in pixels |
| `--output-width` | (matches capture) | `>0` | Published output width in pixels |
| `--output-height` | (matches capture) | `>0` | Published output height in pixels |
| `--fps` | `30` | `>0` | Requested camera frame rate |
| `--help` / `-h` | — | —  | Print usage |

> **Note**: `--capture-width` and `--capture-height` must be specified together (or omitted together). Same rule applies to `--output-width` and `--output-height`.

## Technical Details

### Video Format

- **Capture Pixel Format**: YUYV 4:2:2 or MJPEG
- **Published Pixel Format**: RGB24
- **Output Frame Size**: `width × height × 3` bytes
- **Resize**: Nearest-neighbor interpolation for both YUYV and MJPEG capture

### RGB Normalization Logic

1. Source row and column indices are precomputed via integer scaling
2. YUYV frames are converted directly to resized RGB24
3. MJPEG frames are decoded with TurboJPEG, then resized to RGB24

### Published Sample Structure

Each published sample contains:
- **User header**: Width, height, pixel format, frame size in bytes
- **Timestamp**: Captured from `std::chrono::steady_clock`
- **Sequence number**: Monotonically increasing frame counter
- **Payload**: Raw video bytes as a mutable byte slice

### Capture Loop

1. Dequeue buffer from V4L2
2. Process (resize if needed)
3. Publish via iceoryx2
4. Requeue buffer to V4L2 driver

## Dependencies

- **V4L2** (Linux kernel API): Camera capture
- **libjpeg-turbo**: MJPEG decode
- **iceoryx2**: Zero-copy IPC publishing
- **pthread**: Thread synchronization

## Usage Example

```bash
# Basic usage — capture from /dev/video0 at default resolution and 30 fps
./video_frontend \
    --device /dev/video0 \
    --service video_capture

# Custom resolution and frame rate
./video_frontend \
    --device /dev/video0 \
    --service video_capture \
    --capture-width 1920 \
    --capture-height 1080 \
    --output-width 640 \
    --output-height 480 \
    --fps 60

# List available V4L2 devices and formats
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --list-formats-ext
```
