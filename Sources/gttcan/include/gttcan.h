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

#ifdef STM32
#define GTTCAN_DEFAULT_SLOT_OFFSET 1600
#else
#define GTTCAN_DEFAULT_SLOT_OFFSET 1480
#endif

typedef void (*transmit_callback_fp)(uint32_t, uint64_t, void*);
typedef void (*set_timer_int_callback_fp)(uint32_t, void*);
typedef uint64_t (*read_value_fp)(uint16_t, void*);
typedef void (*write_value_fp)(uint16_t, uint64_t, void*);

typedef struct gttcan_s {

    uint32_t slots[256]; // Array of 29 bit values masked by 0x1FFFFFFF
    uint32_t localSchedule[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];
    uint32_t slotduration; // in NUT (0.1us)

    uint32_t action_time; // The time the next transmission interrupt will fire
    int32_t error_offset; // Timer correction in NUT (0.1us)
    int32_t state_correction;
    int32_t error_accumulator; // accumulated error
    int32_t previous_accumulator;
    int32_t lower_outlier;  // lower-end outlier error
    int32_t upper_outlier;  // higher-end outlier error

    uint16_t slots_accumulated; // the number of slots we have accumulated errors for

    uint8_t scheduleLength; // number of schedule entries        
    uint8_t localNodeId;  
    uint8_t localScheduleLength;
    uint8_t localScheduleIndex;

    bool isActive;
    bool transmitted;

    transmit_callback_fp transmit_callback;
    set_timer_int_callback_fp set_timer_int_callback;
    read_value_fp read_value;
    write_value_fp write_value;
    void *context_pointer;
    

} gttcan_t;

extern gttcan_t gttcan;

void GTTCAN_init(gttcan_t *, uint8_t, uint32_t, uint8_t, transmit_callback_fp, set_timer_int_callback_fp, read_value_fp, write_value_fp, void *);
void GTTCAN_process_frame(gttcan_t *, uint32_t, uint64_t);
void GTTCAN_transmit_next_frame(gttcan_t *);
void GTTCAN_start(gttcan_t *);
void GTTCAN_accumulate_error(gttcan_t *, int32_t);
int32_t GTTCAN_fta(gttcan_t *);
uint16_t GTTCAN_get_nth_slot_since_last_transmit(gttcan_t * gttcan, uint16_t n);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // GTTCAN_H
