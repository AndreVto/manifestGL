#pragma once

#include <stdint.h>

#define WINE_PRESENT_CONTROL_MAGIC 0x57505243u /* 'WPRC' */
#define WINE_PRESENT_CONTROL_VERSION 6u

#define WINE_PRESENT_CONTROL_FLAG_ACTIVE 0x1u
#define WINE_PRESENT_CONTROL_FLAG_NATIVE_PRESENT 0x2u
#define WINE_PRESENT_CONTROL_DISPLAY_REFRESH_MILLIHZ_INDEX 0u
#define WINE_PRESENT_CONTROL_SC_SWAPCHAIN_GENERATION_INDEX 1u
#define WINE_PRESENT_CONTROL_SC_LIVE_SWAPCHAIN_COUNT_INDEX 2u
#define WINE_PRESENT_CONTROL_RECORD_SOURCE_GENERATION_INDEX 3u

enum wine_present_record_command {
    WINE_PRESENT_RECORD_COMMAND_NONE = 0,
    WINE_PRESENT_RECORD_COMMAND_START = 1,
    WINE_PRESENT_RECORD_COMMAND_PAUSE = 2,
    WINE_PRESENT_RECORD_COMMAND_RESUME = 3,
    WINE_PRESENT_RECORD_COMMAND_STOP = 4,
};

enum wine_present_record_state {
    WINE_PRESENT_RECORD_STATE_IDLE = 0,
    WINE_PRESENT_RECORD_STATE_STARTING = 1,
    WINE_PRESENT_RECORD_STATE_RECORDING = 2,
    WINE_PRESENT_RECORD_STATE_PAUSED = 3,
    WINE_PRESENT_RECORD_STATE_STOPPING = 4,
    WINE_PRESENT_RECORD_STATE_ERROR = 5,
};

enum wine_present_record_flags {
    WINE_PRESENT_RECORD_FLAG_CAPTURE_AUDIO = 0x1u,
};

struct wine_present_control_shared {
    uint32_t magic;
    uint32_t version;
    uint32_t target_fps_millihz;
    uint32_t flags;
    uint64_t session_id;
    uint64_t present_count;
    uint64_t last_present_ns;
    uint64_t reserved[4];

    /* UI/application -> renderer recorder commands. The writer stores payload
     * fields first and command_serial last. Readers use command_serial as the
     * acquire/release publication point. */
    uint64_t screenshot_request_serial;
    uint64_t screenshot_ack_serial;
    uint64_t record_command_serial;
    uint64_t record_ack_serial;
    uint32_t record_command;
    uint32_t record_state;
    uint32_t record_target_fps_millihz;
    uint32_t record_error_code;

    /* Recording settings are published before START's command serial.  A zero
     * video bitrate means "automatic" and lets the recorder derive a bitrate
     * from the fixed output dimensions and target frame rate. */
    uint32_t record_video_bitrate_bps;
    uint32_t record_audio_bitrate_bps;
    uint32_t record_keyframe_interval_millis;
    uint32_t record_flags;

    /* The host may suspend the rendering process during overlay or background
     * transitions. Recording lifecycle state remains active in the host while
     * the frame pump is suspended. The discontinuity serial tracks transitions
     * until the frame pump resumes. */
    uint64_t record_lifecycle_state_serial;
    uint64_t record_lifecycle_ack_serial;
    uint64_t record_lifecycle_discontinuity_serial;
    uint32_t record_lifecycle_paused;
    uint32_t record_lifecycle_reserved;

    /* Frame-pump to host-recorder timeline. Video scheduling uses the source AHB;
     * the host owns audio, codec, and muxer resources. Anchor serials publish
     * source_monotonic_ns to output_pts_ns segment mappings. */
    uint64_t record_control_wake_serial;
    uint64_t record_video_anchor_serial;
    uint64_t record_video_anchor_source_ns;
    uint64_t record_video_anchor_pts_ns;
    uint64_t record_video_end_pts_ns;
    uint32_t record_source_gap;
    uint32_t record_pump_drained;

    uint64_t record_frames_submitted;
    uint64_t record_frames_dropped;
    uint64_t record_start_ns;
    uint64_t record_elapsed_ns;
};

static inline uint32_t wine_present_control_load_u32(const uint32_t *value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static inline uint64_t wine_present_control_load_u64(const uint64_t *value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static inline void wine_present_control_store_u32(uint32_t *value, uint32_t next) {
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
}

static inline void wine_present_control_store_u64(uint64_t *value, uint64_t next) {
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
}

static inline uint64_t wine_present_control_add_u64(uint64_t *value, uint64_t increment) {
    return __atomic_add_fetch(value, increment, __ATOMIC_ACQ_REL);
}

static inline uint64_t wine_present_control_sub_u64(uint64_t *value, uint64_t decrement) {
    return __atomic_sub_fetch(value, decrement, __ATOMIC_ACQ_REL);
}

static inline void wine_present_control_max_u64(uint64_t *value, uint64_t candidate) {
    uint64_t current = __atomic_load_n(value, __ATOMIC_ACQUIRE);
    while (current < candidate &&
           !__atomic_compare_exchange_n(value, &current, candidate, 0, __ATOMIC_RELEASE,
                                        __ATOMIC_ACQUIRE)) {}
}

static inline uint32_t wine_present_control_or_u32(uint32_t *value, uint32_t bits) {
    return __atomic_or_fetch(value, bits, __ATOMIC_ACQ_REL);
}
