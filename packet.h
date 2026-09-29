#pragma once

#include <stdint.h>
#include <stddef.h>

#define MAX_PAYLOAD_BYTES   256u
#define SENSOR_ID_GPS       0x01u
#define SENSOR_ID_IMU       0x02u
#define SENSOR_ID_TEMP      0x03u
#define SENSOR_ID_CAN       0x04u

typedef struct SensorPacket {
    uint8_t   sensor_id;
    uint32_t  sequence_num;
    uint64_t  timestamp_us;          // microseconds since epoch
    uint16_t  payload_len;           // reported bytes used in payload[]
    uint8_t   payload[MAX_PAYLOAD_BYTES];
    uint32_t  checksum;              // CRC-32 over sensor_id..payload (see consumer validation rules)
} SensorPacket;

static inline const char* sensormesh_sensor_name(uint8_t sensor_id) {
    switch (sensor_id) {
        case SENSOR_ID_GPS:  return "GPS";
        case SENSOR_ID_IMU:  return "IMU";
        case SENSOR_ID_TEMP: return "TEMP";
        case SENSOR_ID_CAN:  return "CAN";
        default:             return "UNK";
    }
}


