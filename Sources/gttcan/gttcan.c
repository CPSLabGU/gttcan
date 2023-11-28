#include "gttcan.h"
#include "slot_defs.h"

gttcan_t gttcan;

static uint32_t GTTCAN_create_entry(uint8_t id, uint16_t dataslot) {
    return ((uint32_t)id << 16) | dataslot;
}

void GTTCAN_init(gttcan_t *gttcan,
                uint8_t localNodeId,
                uint32_t slotduration, 
                uint8_t globalScheduleLength, 
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
    uint16_t i = 0;
    for(i = 0; i < globalScheduleLength; i++) {
        uint8_t nodeid = (gttcan->slots[i] >> 16) & 0XFF;
        uint16_t dataid = gttcan->slots[i] & 0xFFFF;
        if(nodeid == localNodeId) {
            gttcan->localScheduleSlotID[gttcan->localScheduleLength] = i;
            gttcan->localScheduleDataID[gttcan->localScheduleLength] = dataid;
            gttcan->localScheduleLength++;
            if(gttcan->localScheduleLength >= GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH) {
                i = globalScheduleLength; // dont add any more entries
            }
        }
    }
    // Reset the FTA
    (void) GTTCAN_fta(gttcan);
}

void GTTCAN_process_frame(gttcan_t *gttcan, uint32_t can_frame_id_field, uint64_t data) {
    gttcan->action_time -= gttcan->state_correction;
    uint16_t slotID = can_frame_id_field & 0x3FFF; // TODO: CHECK IF THESE ARE VALID
    uint16_t scheduleIndex = (can_frame_id_field >> 14) & 0x3FFF; // TODO: CHECK IF THESE ARE VALID
   
    int32_t slot_since_last = GTTCAN_get_slots_since_last_transmit(gttcan, scheduleIndex);
    int32_t expected_time = slot_since_last * gttcan->slotduration;
    int32_t error = expected_time - gttcan->action_time; // positive if we received the frame earlier than expected

    GTTCAN_accumulate_error(gttcan, error);

    if(slotID == NETWORK_TIME_SLOT) 
    {  // If Reference Frame
        data+=GTTCAN_DEFAULT_SLOT_OFFSET; // Add 150us offset for transmission time - not exact because of stuffing bits, but gets it close
        // Update global time using Data && 0x3FFFFFFFFFFFFFFF
        gttcan->write_value(NETWORK_TIME_SLOT, (data & 0x3FFFFFFFFFFFFFFF), gttcan->context_pointer);
        if ((data >> 63) == 1) 
        { // If Start-of-schedule frame
            gttcan->isActive = true; // Activate node (if not already)
            gttcan->localScheduleIndex = 0;
            gttcan->error_offset = GTTCAN_fta(gttcan);
            
            //gttcan->slotduration -= gttcan->error_offset; TODO: add this back in
            uint32_t timeToFirstEntry = gttcan->localScheduleSlotID[gttcan->localScheduleIndex] * gttcan->slotduration;
            gttcan->state_correction = 0;
            gttcan->set_timer_int_callback(timeToFirstEntry, gttcan->context_pointer);
        } 
        else 
        { // else normal reference frame 
            // TODO: Move above 'time' calculation to occur on any reference frame (not just start of schedule).
            // Calculation will need to be updated as per re-calibration
        }
    } // Else if Normal message (id between 8 and 2^numIdBits-1), slotID between 1 and WBSIZE-1) 
    else if (slotID >=1) 
    {
            // Update datastructure (whiteboard) with data in slot slotID
            gttcan->write_value(slotID, data, gttcan->context_pointer);
    } 
    else 
    {
        // Error - invalid frame recieved
        return;
    }
    if (gttcan->slots_accumulated >= gttcan->globalScheduleLength && error > gttcan->lower_outlier && error < gttcan->upper_outlier)
    {
        gttcan->error_offset = GTTCAN_fta(gttcan);
        //gttcan->slotduration -= gttcan->error_offset; TODO: add this back in
        uint16_t nextTransmitSlot = gttcan->localScheduleSlotID[gttcan->localScheduleIndex];
        int32_t slotsToNextEntry = 0;
        if (scheduleIndex < nextTransmitSlot) 
        {
            slotsToNextEntry = nextTransmitSlot - scheduleIndex;
        }
        else
        {
            slotsToNextEntry = gttcan->globalScheduleLength - scheduleIndex + nextTransmitSlot;
        }
        int32_t timeToNextEntry = slotsToNextEntry * gttcan->slotduration;
        int32_t state_correction = gttcan->error_offset * slotsToNextEntry - (error - gttcan->error_offset);
        // TODO: this should never happen, so we should signal the error, so the system can reset
        while (state_correction > timeToNextEntry)
            timeToNextEntry += gttcan->globalScheduleLength * gttcan->slotduration;
        int32_t corrected_time_to_next_entry = timeToNextEntry - state_correction;
        gttcan->set_timer_int_callback(corrected_time_to_next_entry, gttcan->context_pointer);
    }
}

void GTTCAN_transmit_next_frame(gttcan_t * gttcan) 
{
    // Check Node is active
    if (!gttcan->isActive) 
    {
        return;
    }
    gttcan->transmitted = true;
    // Transmit local schedule entry
    uint16_t globalScheduleIndex = gttcan->localScheduleSlotID[gttcan->localScheduleIndex];
    uint16_t dataID = gttcan->localScheduleDataID[gttcan->localScheduleIndex];
    uint64_t data = gttcan->read_value(dataID, gttcan->context_pointer);
    if(globalScheduleIndex == 0U) // if this is a start of schedule
    { 
        data = data | 0x8000000000000000; // set MSB to 1 (we may need to clear 62nd bit for TTCan compatibility)
    }
    uint32_t can_frame_header = ((uint32_t)globalScheduleIndex) << 14 | dataID;
    gttcan->transmit_callback(can_frame_header, data, gttcan->context_pointer);
    // If next not end of local schedule
    if(gttcan->localScheduleIndex < gttcan->localScheduleLength-1) 
    {
        gttcan->localScheduleIndex++;
        int32_t slotsToNextEntry = (gttcan->localScheduleSlotID[gttcan->localScheduleIndex]) - globalScheduleIndex;
        uint32_t timeToNextEntry = slotsToNextEntry * gttcan->slotduration;
        gttcan->set_timer_int_callback(timeToNextEntry, gttcan->context_pointer);
        gttcan->state_correction = 0;
    }
    else 
    {
        uint32_t slotsRemainingInSchedule = gttcan->globalScheduleLength - globalScheduleIndex;
        gttcan->localScheduleIndex = 0; // Reset local schedule index
        // calculate timer for first entry in next schedule round 
        uint32_t slotsToFirstEntry = gttcan->localScheduleSlotID[gttcan->localScheduleIndex];
        // (in case master misses start of schedule, we should still transmit on schedule). 
        // This timer will be re-calculated if we receive a start of schedule frame
        int32_t slotsToNextEntry = slotsRemainingInSchedule + slotsToFirstEntry;
        uint32_t timeToNextEntry = slotsToNextEntry * gttcan->slotduration;
        gttcan->set_timer_int_callback(timeToNextEntry, gttcan->context_pointer);
        gttcan->state_correction = 0;
    }
}

void GTTCAN_start(gttcan_t * gttcan) 
{
    gttcan->localScheduleIndex = 0; // reset node to start of schedule.
    gttcan->isActive = true; // activate this node
    GTTCAN_transmit_next_frame(gttcan); // send first message in schedule - shoule be start of sxhedule frame for master
}

uint16_t GTTCAN_get_slots_since_last_transmit(gttcan_t * gttcan, uint16_t currentScheduleIndex) 
{
    if (!gttcan->transmitted) return currentScheduleIndex;
    const uint16_t lastTransmitIndex = gttcan->localScheduleIndex > 0
        ? (uint16_t) (gttcan->localSchedule[gttcan->localScheduleIndex - 1] >> 16)
        : (uint16_t) (gttcan->localSchedule[gttcan->localScheduleLength - 1] >> 16);
    return currentScheduleIndex > lastTransmitIndex
        ? currentScheduleIndex - lastTransmitIndex
        : (gttcan->globalScheduleLength - lastTransmitIndex) + currentScheduleIndex;
}

/// @brief Return the fault-tolerant average error.
///
/// This function returns the fault-tolerant average error
/// and resets the error accumulator.
/// If not enough errors have been accumulated, this function
/// will degrade to an arithmetic average.
///
/// @param gttcan The gttcan instance.
/// @return The fault-tolerant average error.
int32_t GTTCAN_fta(gttcan_t *gttcan)
{
    int32_t error;
    switch (gttcan->slots_accumulated) 
    {
        case 0: 
            gttcan->state_correction = error = 0; 
            break; // no errors accumulated
        case 1: // not enough errors to run an FTA, so
        case 2: // degrade to an arithmetic average
            error = gttcan->error_accumulator / gttcan->slots_accumulated;
            gttcan->state_correction = gttcan->error_accumulator;
            break;

        default: // we have enough error samples to do fault-tolerant averaging
            error = (gttcan->error_accumulator - gttcan->lower_outlier - gttcan->upper_outlier) / (gttcan->slots_accumulated - 2);
            gttcan->state_correction = error * gttcan->slots_accumulated;
    }

    gttcan->error_accumulator = 0;
    gttcan->lower_outlier = INT32_MAX;
    gttcan->upper_outlier = INT32_MIN;
    gttcan->slots_accumulated = 0;

    return error;
}

/// @brief Accumulate an error sample.
///
/// This function adds the given error to the accumulator
/// and updates the lower and upper outliers as necessary.
///
/// @param gttcan The gttcan instance to operate on.
/// @param error  The error to accumulate.
void GTTCAN_accumulate_error(gttcan_t *gttcan, int32_t error)
{
    if(!gttcan->transmitted) return;
    gttcan->previous_accumulator = gttcan->error_accumulator;
    gttcan->error_accumulator += error;

    if (error < gttcan->lower_outlier)
        gttcan->lower_outlier = error;
    if (error > gttcan->upper_outlier)
        gttcan->upper_outlier = error;

    gttcan->slots_accumulated++;
}
