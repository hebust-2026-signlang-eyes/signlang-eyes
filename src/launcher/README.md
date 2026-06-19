# launcher — System Orchestrator

## Overview

The **launcher** module is the system entry point that reads per-module configuration from a TOML file and spawns all 7 sub-modules as child processes. It monitors child health and performs a clean shutdown of the entire system if any module exits unexpectedly.

- **Executable**: `signlang_eyes_launcher`
- **Input**: TOML configuration file (`conf/conf.toml` by default)
- **Output**: Spawns and supervises all child modules

## File Inventory

| File | Description |
|------|-------------|
| `main.cpp` | Entry point; TOML parsing, argument building, fork+exec, child monitoring |
| `program_options.{cpp,hpp}` | CLI argument parsing via cxxopts (`--config` flag) |

## Command-Line Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--config` / `-c` | `conf/conf.toml` | Path to the TOML configuration file |
| `--help` / `-h` | — | Print usage |

## Configuration File

The TOML file has one `[section]` per module. All keys are optional — omitted keys fall back to each module's built-in default. IPC service names are **not** configurable; they are hardcoded in the launcher. Defining IPC keys (e.g. `input_service`) in the TOML emits a warning and the value is ignored.

See `conf/conf.toml` for the default configuration with all available keys documented as comments.

## Technical Details

### IPC Service Names (Hardcoded)

```
audio_capture          → audio_frontend → speech_asr, env_sound_det
video_capture          → video_frontend → handpose_det
speech_asr_result      ← speech_asr
env_sound_result       ← env_sound_det
handpose_result        ← handpose_det → signlang_det
signlang_result        ← signlang_det
app_state_event        ↔ state_machine → speech_asr, handpose_det, signlang_det
app_state_blackboard   ↔ state_machine → speech_asr, handpose_det, signlang_det
app_state_control      ↔ state_machine → env_sound_det
```

### Startup Order

1. `state_machine` — global state controller (must start first)
2. `audio_frontend` — audio capture
3. `video_frontend` — video capture
4. `speech_asr` — speech recognition
5. `env_sound_det` — environmental sound detection
6. `handpose_det` — hand keypoint detection
7. `signlang_det` — sign language recognition

### Process Lifecycle

- **Launch**: `fork()` + `execvp()`. A `pipe2(…, O_CLOEXEC)` detects exec failures — if the child writes back `errno`, the parent knows the exec failed and aborts the entire launch.
- **Monitor**: `waitpid(-1, &status, WNOHANG)` in a 500 ms loop. On any child exit (normal or abnormal), all remaining children receive `SIGTERM`.
- **Shutdown**: `SIGINT`/`SIGTERM` on the launcher itself triggers `SIGTERM` to all children, then `waitpid` to reap them.

### TOML Parsing

- Each module section is read independently; missing sections are silently skipped (the module runs with its own defaults).
- String, integer, floating-point, and boolean values are supported via `toml++` accessors.
- Numeric keys use underscore naming (`capture_rate`, `window_ms`) and are mapped to each module's CLI flags (`--capture-rate`, `--window-ms`).

## Dependencies

- **cxxopts**: CLI argument parsing (header-only)
- **toml++**: TOML configuration parsing (header-only, single `toml.hpp`)
- **POSIX**: `fork`, `execvp`, `waitpid`, `kill`, `pipe2`, `nanosleep`

## Install Layout

```
install/
├── signlang_eyes_launcher       ← this executable (root, not bin/)
├── bin/
│   ├── state_machine
│   ├── audio_frontend
│   ├── video_frontend
│   ├── speech_asr
│   ├── env_sound_det
│   ├── handpose_det
│   └── signlang_det
├── conf/
│   └── conf.toml
├── lib/
│   ├── libiceoryx2_cxx.so
│   ├── libiceoryx2_ffi_c.so
│   └── librknnrt.so
└── models/
    ├── whisper/
    ├── yamnet/
    ├── yolov8n-handpose/
    └── signlang/
```

## Usage Example

```bash
# Start all modules with the default configuration
./signlang_eyes_launcher

# Use a custom configuration file
./signlang_eyes_launcher --config /etc/signlang/config.toml
```
