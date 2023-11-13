#ifndef GTTCAN_H
#define GTTCAN_H

#include <stdint.h>
#include <stdbool.h>
#include "slot_defs.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef GTTCAN_MAX_SLOTS
#define GTTCAN_MAX_SLOTS 512
#endif

#ifndef GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH
#define GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH 32
#endif

typedef void (*transmit_callback_fp)(uint32_t, uint64_t, void*);
typedef void (*set_timer_int_callback_fp)(uint32_t, void*);
typedef uint64_t (*read_value_fp)(uint16_t, void*);
typedef void (*write_value_fp)(uint16_t, uint64_t, void*);

typedef struct gttcan_s {

    uint32_t slots[256]; // Array of 29 bit values masked by 0x1FFFFFFF
    uint32_t localSchedule[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];
    uint32_t slotduration; // in NUT (0.1us)
    uint8_t scheduleLength; // number of schedule entries
        
    uint8_t localNodeId;  
    uint8_t localScheduleLength;
    uint8_t localScheduleIndex;

    bool isActive;

    transmit_callback_fp transmit_callback;
    set_timer_int_callback_fp set_timer_int_callback;
    read_value_fp read_value;
    write_value_fp write_value;
    void *context_pointer;
    

} gttcan_t;

extern gttcan_t gttcan;

void GTTCAN_init(gttcan_t *gttcan, uint8_t localNodeId, uint32_t slotduration, uint8_t scheduleLength, transmit_callback_fp transmit_callback, set_timer_int_callback_fp set_timer_int_callback, read_value_fp read_value, write_value_fp write_value, void *readwrite_data);
void GTTCAN_process_frame(gttcan_t *gttcan, uint32_t can_frame_id_field, uint64_t data);
void GTTCAN_transmit_next_frame(gttcan_t *gttcan);
void GTTCAN_start(gttcan_t *gttcan);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // GTTCAN_H
