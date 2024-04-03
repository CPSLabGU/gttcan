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
#define GTTCAN_DEFAULT_SLOT_OFFSET 1600U
#else
#define GTTCAN_DEFAULT_SLOT_OFFSET 1480U
#endif

typedef void (*transmit_callback_fp)(uint32_t, uint64_t, void*);
typedef void (*set_timer_int_callback_fp)(uint32_t, void*);
typedef uint64_t (*read_value_fp)(uint16_t, void*);
typedef void (*write_value_fp)(uint16_t, uint64_t, void*);

typedef struct gttcan_s {

    uint32_t slots[256]; // Array of 29 bit values masked by 0x1FFFFFFF
    uint32_t localSchedule[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];

    uint16_t localScheduleSlotID[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];
    uint16_t localScheduleDataID[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];

    uint32_t slotduration; // in NUT (0.1us)
    uint32_t action_time; // The time the next transmission interrupt will fire
    int32_t error_offset; // Timer correction in NUT (0.1us)
    int32_t state_correction;
    int32_t error_accumulator; // accumulated error
    int32_t previous_accumulator;
    int32_t lower_outlier;  // lower-end outlier error
    int32_t upper_outlier;  // higher-end outlier error

    uint16_t slots_accumulated; // the number of slots we have accumulated errors for

    uint16_t globalScheduleLength; // number of schedule entries        
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

/**
 * @brief Initialize a GTTCAN instance.
 *
 * @param gttcan The GTTCAN instance to initialize.
 * @param localNodeId The local node ID.
 * @param slotduration The duration of a slot.
 * @param globalScheduleLength The length of the global schedule.
 * @param transmit_callback The transmit callback function.
 * @param set_timer_int_callback The set timer interrupt callback function.
 * @param read_value The read value function.
 * @param write_value The write value function.
 * @param context_pointer The context pointer.
 */
void GTTCAN_init(gttcan_t *gttcan,
                uint8_t localNodeId,
                uint32_t slotduration,
                uint16_t globalScheduleLength,
                transmit_callback_fp transmit_callback,
                set_timer_int_callback_fp set_timer_int_callback,
                read_value_fp read_value,
                write_value_fp write_value,
                void *context_pointer);

/**
 * @brief Process a received CAN frame.
 *
 * This function processes a received CAN frame, updates the global time,
 * and handles the data based on the slot ID. It also calculates the time
 * to the next entry and sets a timer interrupt for that time.
 *
 * When a reference frame is received, the node is activated,
 * and the local schedule is reset.
 *
 * If no reference fram has been received for the duration
 * of the global schedule, clock synchronisation is performed
 * using the fault-tolerant average error.
 *
 * @param gttcan The GTTCAN instance.
 * @param can_frame_id_field The ID field of the received CAN frame.
 * @param received_data The data of the received CAN frame.
 */
void GTTCAN_process_frame(gttcan_t *gttcan, uint32_t current_time, uint32_t can_frame_id_field, const uint64_t received_data);

/**
 * @brief Transmit the next frame in the GTTCAN schedule.
 *
 * This function checks if the node is active, and if so,
 * transmits the next frame in the local schedule.
 * It also calculates the time to the next entry and
 * runs the callback function to set a timer interrupt
 * for that point in time.
 *
 * @param gttcan The GTTCAN instance.
 */
void GTTCAN_transmit_next_frame(gttcan_t *gttcan);

/**
 * @brief Start the GTTCAN instance.
 *
 * This function should only ever be called on master.
 * It resets the node to the start of the schedule,
 * activates the node, and sends the first message in the schedule.
 *
 * @param gttcan The GTTCAN instance.
 */
void GTTCAN_start(gttcan_t * gttcan);

/**
 * @brief Get the number of slots to the next transmit.
 *
 * This function calculates the number of slots to the next transmit slot in the schedule.
 *
 * @param gttcan The GTTCAN instance.
 * @param currentScheduleIndex The current index in the schedule.
 * @return The number of slots to the next transmit.
 */
uint16_t GTTCAN_get_slots_to_next_transmit(gttcan_t *gttcan, uint16_t currentScheduleIndex);

/**
 * @brief Get the number of slots since the last transmit.
 *
 * This function calculates the number of slots since the last transmit in the schedule.
 *
 * @param gttcan The GTTCAN instance.
 * @param currentScheduleIndex The current index in the schedule.
 * @return The number of slots since the last transmit.
 */
uint16_t GTTCAN_get_slots_since_last_transmit(gttcan_t * gttcan, uint16_t currentScheduleIndex);

/**
 * @brief Return the fault-tolerant average error.
 *
 * This function returns the fault-tolerant average error
 * and resets the error accumulator.
 * If not enough errors have been accumulated, this function
 * will degrade to an arithmetic average.
 *
 * @param gttcan The gttcan instance.
 * @return The fault-tolerant average error.
 */
int32_t GTTCAN_fta(gttcan_t *gttcan);

/**
 * @brief Accumulate an error sample.
 *
 * This function adds the given error to the accumulator
 * and updates the lower and upper outliers as necessary.
 *
 * @param gttcan The gttcan instance to operate on.
 * @param error  The error to accumulate.
 */
void GTTCAN_accumulate_error(gttcan_t *gttcan, int32_t error);

/**
 * @brief Calculate the number of stuffed bits in a CAN frame.
 *
 * This function calculates the number of stuffed bits in a CAN frame.
 * The CAN frame is assumed to be in the format specified by ISO 11898-1.
 * The CRC delimiter and ACK slot are assumed to be present.
 *
 * @param buffer Pointer to the buffer containing the CAN frame.
 * @param length Length of the CAN frame.
 */
uint32_t GTTCAN_calculate_stuffing_bits(const uint8_t * const buffer, const uint32_t length);

/**
 * @brief Calculate the total number of bits in a CAN frame.
 *
 * This function calculates the number of bits in a CAN frame on the bus.
 * The CAN frame is assumed to be in the format specified by ISO 11898-1.
 * The CRC delimiter and ACK slot are assumed to be present.
 *
 * @param buffer Pointer to the buffer containing the CAN frame.
 * @param length Length of the CAN frame.
 * @param is_extended Whether the CAN frame has an extended identifier.
 */
uint32_t GTTCAN_calculate_can_frame_bits(const uint8_t * const buffer, const uint32_t length, const bool is_extended);

/**
 * @brief Compute the CRC-15 of a CAN frame.
 *
 * This function calculates a 15-bit CRC using the polynomial
 * x^15 + x^14 + x^10 + x^8 + x^7 + x^4 + x^3 + 1 (0x4599),
 * which is used in the CAN protocol.
 *
 * @param buffer Pointer to the buffer containing the CAN frame.
 * @param length The length of the CAN frame.
 */
uint16_t GTTCAN_crc15(const uint8_t * const buffer, const uint32_t length);

/**
 * @brief Append a CRC to a CAN frame.
 *
 * This function appends a CRC to a CAN frame.
 * The CAN frame is assumed to be in the format specified by ISO 11898-1.
 * The buffer needs to have enough space to hold the CRC, i.e.,
 * len + 2 bytes.
 *
 * @param frame Pointer to the buffer containing the CAN frame.
 * @param length Length of the CAN frame.
 */
uint32_t GTTCAN_append_crc_to_can_frame(uint8_t * const frame, const uint32_t length);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // GTTCAN_H
