# signlang_manager — BLE Handpose Stream and Gesture Database Manager

## Overview

`signlang_manager` exposes a BLE GATT access point for external tools:

- streams latest `handpose_det` results through BLE notifications
- lists existing gesture prototypes
- adds gestures from uploaded handpose recordings
- deletes gestures from the SQLite prototype database
- asks `signlang_det` to reload prototypes after database mutations

The module initializes `prototypes.sqlite` automatically. If the file is empty, missing required tables, has a wrong
schema version, or has an embedding dimension mismatch, it recreates a valid empty database with no gestures.

## BLE Service

Custom service UUID:

```text
3b5f1000-4ad2-4f53-9a65-6f6d65796573
```

Characteristics:

| UUID | Direction | Flags | Description |
|------|-----------|-------|-------------|
| `3b5f1001-4ad2-4f53-9a65-6f6d65796573` | external -> device | write, write-without-response | command packets and upload chunks |
| `3b5f1002-4ad2-4f53-9a65-6f6d65796573` | device -> external | notify, read | responses and handpose stream packets |

Payloads use the versioned packet format in `protocol.{hpp,cpp}`. Larger notifications are split by
`--max-notify-payload`; the receiver should reassemble packets using the protocol header length and payload length.
Only one BLE client may subscribe to streaming notifications at a time. Additional `StartNotify` attempts are rejected
until the current streaming client calls `StopNotify` or disconnect handling releases the subscription.

## Commands

| Command | ID | Purpose |
|---------|----|---------|
| `GetCapabilities` | `0x0001` | Returns protocol/model/stream status |
| `SetStreamConfig` | `0x0101` | Enables or disables handpose streaming |
| `ListGestures` | `0x0201` | Lists gesture id/name/enabled/sample count |
| `AddGestureBegin` | `0x0202` | Starts an upload session |
| `AddGestureChunk` | `0x0203` | Writes an upload chunk |
| `AddGestureCommit` | `0x0204` | Encodes uploaded handpose frames and stores samples |
| `AddGestureAbort` | `0x0205` | Cancels current upload |
| `DeleteGesture` | `0x0206` | Deletes by id or name |
| `GetStatus` | `0x0301` | Returns current status |

No application-level authentication is implemented.

## Upload Format

`AddGestureBegin` declares a transfer id, total byte size, replace flag, and gesture name. `AddGestureChunk` writes byte
ranges. `AddGestureCommit` fails unless every byte in the declared upload has arrived.

The uploaded blob contains:

```text
u32 frame_count
repeat frame_count:
  u32 frame_payload_size
  wire_handpose_frame payload
```

Each frame payload is the raw-f32 wire format produced by `wire_handpose.{hpp,cpp}`.

During commit, the manager:

1. decodes handpose frames
2. copies the feature extraction logic used by `signlang_det`
3. runs its own RKNN BiLSTM encoder instance
4. writes encoded prototype samples to SQLite via `SQLiteCpp`
5. sends a prototype reload request to `signlang_det`

## Command-Line Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--input-service` | required | handpose iceoryx2 service |
| `--signlang-control-service` | required | prototype reload request-response service |
| `--bluetooth-name` | `SignLang Eyes` | BLE advertising local name |
| `--adapter-path` | `/org/bluez/hci0` | BlueZ adapter object path |
| `--model` | `models/signlang/signlang.rknn` | RKNN encoder model |
| `--prototypes` | `models/signlang/prototypes.sqlite` | SQLite prototype DB |
| `--min-confidence` | `0.3` | minimum hand confidence for uploaded samples |
| `--motion-weight` | `0.0` | velocity feature weight |
| `--upload-window-overlap` | `0.5` | overlap when long uploads are split into samples |
| `--subscriber-buffer` | `2` | iceoryx2 handpose queue size |
| `--stream-fps` | `30` | max BLE stream frame rate |
| `--max-notify-payload` | `180` | notification chunk size |
| `--npu-core` | `auto` | RKNN NPU core selection |
| `--enable-streaming-by-default` | false | stream as soon as a client subscribes |

## Running Board BLE Check

Run these on the target board before relying on BLE:

```bash
bluetoothctl list
bluetoothctl show
bluetoothctl power on
bluetoothctl advertise on
```

Then run the installed stack and inspect logs:

```bash
./launcher --config conf/conf.toml
tail -f log/*signlang_manager*.log
```

Expected manager log line:

```text
BLE GATT service registered on /org/bluez/hci0
```

From a phone or Linux central, scan for the configured local name and connect. If registration fails, capture:

```bash
bluetoothctl show
ps -ef | grep bluetoothd
journalctl -u bluetooth -n 100 --no-pager
```
