#ifndef _SST_H
#define _SST_H

#include "../net/tcpclient.h"
#include "../ui/pushbutton.h"
#include "as5600.h"
#include "ssd1306.h"
#include "tusb.h"

enum state {
    IDLE,
    SLEEP,
    WAKING,
    REC_START,
    RECORD,
    REC_STOP,
    SYNC_DATA,
    SERVE_TCP,
    MSC,
    CAL_IDLE_1,
    CAL_EXP,
    CAL_IDLE_2,
    CAL_COMP,
};
#define STATES_COUNT 13

#define CHUNK_TYPE_RATES 0x00
#define CHUNK_TYPE_TELEMETRY 0x01
#define CHUNK_TYPE_MARKER 0x02
#define CHUNK_TYPE_IMU 0x03

struct chunk_header {
    uint8_t type;
    uint16_t length;
} __attribute__((packed));

struct rate_entry {
    uint8_t type;
    uint16_t rate;
} __attribute__((packed));

struct header {
    char magic[3];
    uint8_t version;
    uint32_t padding;
    time_t timestamp;
};

struct record {
    uint16_t fork_angle;
    uint16_t shock_angle;
};

struct imu_record {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
};

_Static_assert(sizeof(struct imu_record) == 12, "imu_record size mismatch");

enum command { OPEN, DUMP_TELEMETRY, DUMP_IMU, FINISH, MARKER };

#define BUFFER_SIZE 2048
#define FILENAME_LENGTH                                                                                                \
    10 // filename is always in 00000.SST format,
       // so length is always 10.
#endif /* _SST_H */
