/**
 * @file gttcan.c
 * @brief This file contains the implementation of the GTTCAN protocol.
 */
#include "gttcan.h"
#include "slot_defs.h"

static uint32_t GTTCAN_create_entry(uint8_t id, uint16_t dataslot) {
    return ((uint32_t)id << 16) | dataslot;
}


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
                 void *context_pointer)
{
    gttcan->globalScheduleLength = globalScheduleLength;
    gttcan->slotduration = slotduration;
    gttcan->isActive = false;
    gttcan->transmitted = false;
    gttcan->localNodeId = localNodeId;
    gttcan->action_time = 0;
    gttcan->error_offset = 0;

    gttcan->transmit_callback = transmit_callback;
    gttcan->set_timer_int_callback = set_timer_int_callback;
    gttcan->read_value = read_value;
    gttcan->write_value = write_value;
    gttcan->context_pointer = context_pointer;

    // FIXME: the following hard-coded slot initialisations need to be removed
    // Create global schedule. This could be changed to whiteboard slots
    gttcan->slots[0] = GTTCAN_create_entry(1, NETWORK_TIME_SLOT); // Reference Frame from Time Master
    gttcan->slots[1] = GTTCAN_create_entry(10, NODE10_NUM_RECEIVED_FRAMES);
    gttcan->slots[2] = GTTCAN_create_entry(8, NODE8_NUM_RECEIVED_FRAMES);
    gttcan->slots[3] = GTTCAN_create_entry(9, NODE9_NUM_RECEIVED_FRAMES);
    /*
     gttcan->slots[1] = GTTCAN_create_entry(8, NODE8_NUM_RECEIVED_FRAMES);
     gttcan->slots[2] = GTTCAN_create_entry(9, NODE9_NUM_RECEIVED_FRAMES);
     gttcan->slots[3] = GTTCAN_create_entry(10, NODE10_NUM_RECEIVED_FRAMES);
     gttcan->slots[4] = GTTCAN_create_entry(8, NODE8_NUM_RECEIVED_FRAMES);
     gttcan->slots[5] = GTTCAN_create_entry(9, NODE9_NUM_RECEIVED_FRAMES);
     gttcan->slots[6] = GTTCAN_create_entry(10, NODE10_NUM_RECEIVED_FRAMES);
     gttcan->slots[7] = GTTCAN_create_entry(8, NODE8_NUM_RECEIVED_FRAMES);
     gttcan->slots[8] = GTTCAN_create_entry(9, NODE9_NUM_RECEIVED_FRAMES);
     gttcan->slots[9] = GTTCAN_create_entry(10, NODE10_NUM_RECEIVED_FRAMES);
     gttcan->slots[10] = GTTCAN_create_entry(8, NODE8_NUM_RECEIVED_FRAMES);
     gttcan->slots[11] = GTTCAN_create_entry(9, NODE9_NUM_RECEIVED_FRAMES);
     gttcan->slots[12] = GTTCAN_create_entry(10, NODE10_NUM_RECEIVED_FRAMES);
     gttcan->slots[13] = GTTCAN_create_entry(8, NODE8_NUM_RECEIVED_FRAMES);
     gttcan->slots[14] = GTTCAN_create_entry(9, NODE9_NUM_RECEIVED_FRAMES);*/

    // Create Local Schedule
    gttcan->localScheduleIndex = 0;
    gttcan->localScheduleLength = 0;
    for (uint16_t i = 0; i < globalScheduleLength; i++)
    {
        uint8_t nodeid = (uint8_t)((uint32_t)(gttcan->slots[i] >> 16) & 0XFFU);
        uint16_t dataid = (uint16_t)(gttcan->slots[i] & 0xFFFFU);
        if (nodeid == localNodeId)
        {
            gttcan->localScheduleSlotID[gttcan->localScheduleLength] = i;
            gttcan->localScheduleDataID[gttcan->localScheduleLength] = dataid;
            gttcan->localScheduleLength++;
            if (gttcan->localScheduleLength >= (uint8_t)GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH)
            {
                break;
            }
        }
    }
    // Reset the FTA
    (void) GTTCAN_fta(gttcan);
}

/**
 * @brief Process a received CAN frame.
 *
 * This function should be called whenever a CAN frame is received.
 * For most use cases, this would be done via an interrupt, though it
 * could be called in a polling loop. Note that the time between the
 * frame being received and the polling mechanism detecting there is a
 * frame waiting to be processed will introduce timing error.
 * 
 * This function processes a received CAN frame, updates the global time
 * in the case of a reference frame, and handles the data based on the 
 * slot ID. It also calculates the time to the next entry and sets a 
 * timer interrupt for that time.
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
void GTTCAN_process_frame(gttcan_t *gttcan, uint32_t can_frame_id_field, const uint64_t received_data)
{
    //gttcan->action_time -= (uint32_t)gttcan->state_correction;
    uint64_t data = received_data;
    uint16_t slotID = (uint16_t)(can_frame_id_field & 0x3FFFU); // TODO: CHECK IF THESE ARE VALID
    uint16_t globalScheduleIndex = (uint16_t)((can_frame_id_field >> 14) & 0x3FFFU); // TODO: CHECK IF THESE ARE VALID

    uint32_t slot_since_last = GTTCAN_get_slots_since_last_transmit(gttcan, globalScheduleIndex);
    uint32_t expected_time = slot_since_last * gttcan->slotduration;
    int32_t error = (int32_t)expected_time - (int32_t)gttcan->action_time; // positive if we received the frame earlier than expected

    GTTCAN_accumulate_error(gttcan, error);

    // If Reference Frame
    if (slotID == (uint16_t)NETWORK_TIME_SLOT)
    {
        if ((data & 0x8000000000000000ULL) != 0U) // If Start-of-schedule frame
        {
            gttcan->isActive = true;    // Activate node (if not already)
            gttcan->localScheduleIndex = 0;
        }
        data |= GTTCAN_DEFAULT_SLOT_OFFSET; // Add 150us offset for transmission time - not exact because of stuffing bits, but gets it close
        // Update global time using Data && 0x3FFFFFFFFFFFFFFF
        gttcan->write_value(NETWORK_TIME_SLOT, (data & 0x3FFFFFFFFFFFFFFFULL), gttcan->context_pointer);
        gttcan->error_offset = GTTCAN_fta(gttcan);
        //gttcan->slotduration -= gttcan->error_offset; TODO: add this back in
        uint32_t slotsToNextEntry = GTTCAN_get_slots_to_next_transmit(gttcan, globalScheduleIndex);
        uint32_t timeToNextEntry = slotsToNextEntry * gttcan->slotduration;
        gttcan->state_correction = 0;
        gttcan->set_timer_int_callback(timeToNextEntry, gttcan->context_pointer);
    }
    else if (slotID >= 1U) // Else if Normal message (id between 8 and 2^numIdBits-1), slotID between 1 and WBSIZE-1)
    {
        // Update datastructure (whiteboard) with data in slot slotID
        gttcan->write_value(slotID, data, gttcan->context_pointer);
    }
    else // FIXME: not reached, may need a different check above!
    {
        // Error - invalid frame recieved
        return; // cppcheck-suppress misra-c2012-15.5
    }

    if (gttcan->slots_accumulated >= gttcan->globalScheduleLength)
    {
        gttcan->error_offset = GTTCAN_fta(gttcan);
        //gttcan->slotduration -= gttcan->error_offset; TODO: add this back in
        uint32_t slotsToNextEntry = GTTCAN_get_slots_to_next_transmit(gttcan, globalScheduleIndex);
        uint32_t timeToNextEntry = slotsToNextEntry * gttcan->slotduration;
        /*
        int32_t state_correction = gttcan->error_offset * (int32_t)slotsToNextEntry - (error - gttcan->error_offset);
        // TODO: this should never happen, so we should signal the error, so the system can reset
        while ((uint32_t)(-state_correction) > timeToNextEntry)
        {
            timeToNextEntry += gttcan->globalScheduleLength * gttcan->slotduration;
        }
        uint32_t corrected_time_to_next_entry = timeToNextEntry + (uint32_t)state_correction;
        */
        gttcan->set_timer_int_callback(timeToNextEntry, gttcan->context_pointer);
    }
}

/**
 * @brief Transmit the next frame in the GTTCAN schedule.
 *
 * This funtion should be called from the timer interrupt.
 * It checks if the node is active, and if so, transmits
 * the next frame in the local schedule. It also calculates 
 * the time to the next entry and runs the callback function 
 * to set a new timer interrupt for that point in time.
 *
 * @param gttcan The GTTCAN instance.
 */
void GTTCAN_transmit_next_frame(gttcan_t *gttcan) // cppcheck-suppress misra-c2012-8.7
{
    // Check Node is active
    if (!gttcan->isActive)
    {
        return; // cppcheck-suppress misra-c2012-15.5
    }

    gttcan->transmitted = true;
    // Transmit local schedule entry
    uint16_t globalScheduleIndex = gttcan->localScheduleSlotID[gttcan->localScheduleIndex];
    uint16_t dataID = gttcan->localScheduleDataID[gttcan->localScheduleIndex];
    uint64_t data = gttcan->read_value(dataID, gttcan->context_pointer);
    if (dataID == (uint16_t)NETWORK_TIME_SLOT)  // this is a reference frame
    {
        gttcan->error_offset = GTTCAN_fta(gttcan); // reset error
    }
    if (globalScheduleIndex == 0U) // if this is a start of schedule
    {
        data = data | 0x8000000000000000ULL; // set MSB to 1 (we may need to clear 62nd bit for TTCan compatibility)
    }
    uint32_t can_frame_header = ((uint32_t)globalScheduleIndex << 14) | dataID;

    gttcan->localScheduleIndex++; // move to next entry
    // if end of local schedule
    if (gttcan->localScheduleIndex == gttcan->localScheduleLength)
    {
        gttcan->localScheduleIndex = 0; // reset
    }

    uint16_t slotsToNextEntry = GTTCAN_get_slots_to_next_transmit(gttcan, globalScheduleIndex);
    uint32_t timeToNextEntry = slotsToNextEntry * gttcan->slotduration;
    gttcan->set_timer_int_callback(timeToNextEntry, gttcan->context_pointer);
    gttcan->state_correction = 0;

    gttcan->transmit_callback(can_frame_header, data, gttcan->context_pointer);
}

/**
 * @brief Start the GTTCAN instance.
 *
 * This function should only ever be called on master.
 * It resets the node to the start of the schedule,
 * activates the node, and sends the first message in the 
 * schedule (a reference frame).
 *
 * @param gttcan The GTTCAN instance.
 */
void GTTCAN_start(gttcan_t *gttcan)
{
    gttcan->localScheduleIndex = 0; // reset node to start of schedule.
    gttcan->isActive = true; // activate this node
    GTTCAN_transmit_next_frame(gttcan); // send first message in schedule - shoule be start of sxhedule frame for master
}

/**
 * @brief Get the number of slots to the next transmit.
 *
 * This function calculates the number of slots to the next transmit slot in the schedule.
 *
 * @param gttcan The GTTCAN instance.
 * @param currentScheduleIndex The current index in the schedule.
 * @return The number of slots to the next transmit.
 */
uint16_t GTTCAN_get_slots_to_next_transmit(gttcan_t *gttcan, uint16_t currentScheduleIndex) // cppcheck-suppress misra-c2012-8.7
{
    uint16_t nextTransmitIndex = gttcan->localScheduleSlotID[gttcan->localScheduleIndex];
    return (currentScheduleIndex >= nextTransmitIndex) ?
    (gttcan->globalScheduleLength - currentScheduleIndex + nextTransmitIndex) :
    (nextTransmitIndex - currentScheduleIndex);
}

/**
 * @brief Get the number of slots since the last transmit.
 *
 * This function calculates the number of slots since the last transmit in the schedule.
 *
 * @param gttcan The GTTCAN instance.
 * @param currentScheduleIndex The current index in the schedule.
 * @return The number of slots since the last transmit.
 */
uint16_t GTTCAN_get_slots_since_last_transmit(gttcan_t * gttcan, uint16_t currentScheduleIndex) // cppcheck-suppress misra-c2012-8.7
{
    if (!gttcan->transmitted)
    {
        return currentScheduleIndex; // cppcheck-suppress misra-c2012-15.5
    }

    const uint16_t lastTransmitIndex = (gttcan->localScheduleIndex > 0U) ? // if we are not at the first entry in our schedule
    (gttcan->localScheduleSlotID[gttcan->localScheduleIndex - 1U]) : // last transmit index was the previous entry in local schedule
    (gttcan->localScheduleSlotID[gttcan->localScheduleLength - 1U]); // else last transmit index is last entry in local schedule
    // if the current schedule position is ahead of the last transmit index
    if (currentScheduleIndex > lastTransmitIndex)
    {
        // cppcheck-suppress misra-c2012-15.5
        return (currentScheduleIndex - lastTransmitIndex); // last transmission was previous in the same schedule round
    }
    else // last transmission  was in the previous schedule round
    {
        // cppcheck-suppress misra-c2012-15.5
        return ((gttcan->globalScheduleLength - lastTransmitIndex) + currentScheduleIndex);
    }
}

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
int32_t GTTCAN_fta(gttcan_t *gttcan)
{
    int32_t error;
    switch (gttcan->slots_accumulated)
    {
        case 0:
            error = 0;
            gttcan->state_correction = 0;
            break; // no errors accumulated
        case 1: // not enough errors to run an FTA, so
        case 2: // degrade to an arithmetic average
            error = gttcan->error_accumulator / (int16_t)gttcan->slots_accumulated;
            gttcan->state_correction = gttcan->error_accumulator;
            break;

        default: // we have enough error samples to do fault-tolerant averaging
            error = (gttcan->error_accumulator - gttcan->lower_outlier - gttcan->upper_outlier) / ((int32_t)gttcan->slots_accumulated - 2);
            gttcan->state_correction = error * (int16_t)gttcan->slots_accumulated;
            break;
    }

    gttcan->error_accumulator = 0;
    gttcan->lower_outlier = INT32_MAX;
    gttcan->upper_outlier = INT32_MIN;
    gttcan->slots_accumulated = 0;

    return error;
}

/**
 * @brief Accumulate an error sample.
 *
 * This function adds the given error to the accumulator
 * and updates the lower and upper outliers as necessary.
 *
 * @param gttcan The gttcan instance to operate on.
 * @param error  The error to accumulate.
 */
void GTTCAN_accumulate_error(gttcan_t *gttcan, int32_t error) // cppcheck-suppress misra-c2012-8.7
{
    if(!gttcan->transmitted)
    {
        return; // cppcheck-suppress misra-c2012-15.5
    }

    gttcan->previous_accumulator = gttcan->error_accumulator;
    gttcan->error_accumulator += error;

    if (error < gttcan->lower_outlier)
    {
        gttcan->lower_outlier = error;
    }
    if (error > gttcan->upper_outlier)
    {
        gttcan->upper_outlier = error;
    }

    gttcan->slots_accumulated++;
}
