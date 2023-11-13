#include "gttcan.h"
#include "slot_defs.h"

gttcan_t gttcan;

static uint32_t GTTCAN_create_entry(uint8_t id, uint16_t dataslot) {
    return (id << 16) | dataslot;
}

void GTTCAN_init(gttcan_t *gttcan,
                uint8_t localNodeId,
                uint32_t slotduration, 
                uint8_t scheduleLength, 
                transmit_callback_fp transmit_callback,
                set_timer_int_callback_fp set_timer_int_callback,
                read_value_fp read_value,
                write_value_fp write_value,
                void *context_pointer)
{
    gttcan->scheduleLength = scheduleLength;
    gttcan->slotduration = slotduration;
    gttcan->isActive = false;
    gttcan->localNodeId = localNodeId;

    gttcan->transmit_callback = transmit_callback;
    gttcan->set_timer_int_callback = set_timer_int_callback;
    gttcan->read_value = read_value;
    gttcan->write_value = write_value;
    gttcan->context_pointer = context_pointer;

    // Create global schedule. This could be changed to whiteboard slots
    gttcan->slots[0] = GTTCAN_create_entry(1, NETWORK_TIME_SLOT); // Reference Frame from Time Master
    gttcan->slots[1] = GTTCAN_create_entry(8, NODE8_NUM_RECEIVED_FRAMES);
    gttcan->slots[2] = GTTCAN_create_entry(9, NODE9_NUM_RECEIVED_FRAMES);
    gttcan->slots[3] = GTTCAN_create_entry(10, NODE10_NUM_RECEIVED_FRAMES);
    gttcan->slots[4] = GTTCAN_create_entry(1, NODE1_NUM_RECEIVED_FRAMES);
    gttcan->slots[5] = GTTCAN_create_entry(0, 0); // empty slot

    // Create Local Schedule
    gttcan->localScheduleIndex = 0;
    gttcan->localScheduleLength = 0;
    uint16_t i = 0;
    for(i = 0; i < scheduleLength; i++) {
        uint8_t nodeid = (gttcan->slots[i] >> 16) & 0XFF;
        uint16_t dataid = gttcan->slots[i] & 0xFFFF;
        if(nodeid == localNodeId) {
            gttcan->localSchedule[gttcan->localScheduleLength] = (i<<16) | dataid;
            gttcan->localScheduleLength++;
            if(gttcan->localScheduleLength >= 32) { // if more than 32 entries (replace 32 with #DEFINE)
                i = scheduleLength; // dont add any more entries
            }
        }
    }
}
void GTTCAN_process_frame(gttcan_t *gttcan, uint32_t can_frame_id_field, uint64_t data) {
    uint16_t scheduleIndex = (can_frame_id_field >> 14); // TODO: CHECK IF THESE ARE VALID
    uint16_t slotID = (can_frame_id_field & 0x3FFF); // TODO: CHECK IF THESE ARE VALID
    uint8_t nodeID = gttcan->slots[scheduleIndex] >> 16; // TODO: CHECK IF THESE ARE VALID

    if(slotID == NETWORK_TIME_SLOT) {  // If Reference Frame
        data+=1280; // Add 128us offset for transmission time - not exact because of stuffing bits, but gets it close
        // Update global time using Data && 0x3FFFFFFFFFFFFFFF
        gttcan->write_value(0, (data & 0x3FFFFFFFFFFFFFFF), gttcan->context_pointer);
        if ((data >> 63) == 1) { // If Start-of-schedule frame
            gttcan->isActive = true; // Activate node (if not already)
            gttcan->localScheduleIndex = 0;
            // Check if local schedule Update Required (To be discussed)
                // Update local schedule
            uint32_t timeToFirstEntry = ((gttcan->localSchedule[gttcan->localScheduleIndex] >> 16) * gttcan->slotduration) - 1280;
            gttcan->set_timer_int_callback(timeToFirstEntry, gttcan->context_pointer);
        } else { // else normal reference frame 
        // TODO: Move above 'start time' calculation to occur on any reference frame (not just start of schedule).
        // Calculation will need to be updated as per re-calibration
        }
            
  } // Else if Normal message (id between 8 and 2^numIdBits-1), slotID between 1 and WBSIZE-1) 
  else if (slotID >=1 && slotID < GSW_TOTAL_MESSAGE_TYPES-1) {
        // Update datastructure (whiteboard) with data in slot slotID
        gttcan->write_value(slotID, data, gttcan->context_pointer);
  } else {
    // Error - invalid frame recieved
  }
}

void GTTCAN_transmit_next_frame(gttcan_t * gttcan) {
    // Check Node is active
    if (!gttcan->isActive) {
        return;
    }
    // Transmit local schedule entry
    uint16_t globalScheduleIndex = gttcan->localSchedule[gttcan->localScheduleIndex] >> 16;
    uint16_t slotID = gttcan->localSchedule[gttcan->localScheduleIndex] & 0xFFFF;
    uint64_t data = gttcan->read_value(slotID, gttcan->context_pointer);
    if(globalScheduleIndex == 0U) { // if this is a start of schedule
        data = data | 0x8000000000000000; // set MSB to 1 (we may need to clear 62nd bit for TTCAn compatibility)
    }
    
    // If next not end of local schedule
    if(gttcan->localScheduleIndex < gttcan->localScheduleLength-1) {
        gttcan->localScheduleIndex++;
        uint32_t timeToNextEntry = ((gttcan->localSchedule[gttcan->localScheduleIndex] >> 16) - globalScheduleIndex) * gttcan->slotduration;
        gttcan->set_timer_int_callback(timeToNextEntry, gttcan->context_pointer);  
    }
    else {
        uint32_t slotsRemainingInSchedule = gttcan->scheduleLength - globalScheduleIndex;
        gttcan->localScheduleIndex = 0; // Reset local schedule index
        // calculate timer for first entry in next schedule round 
        uint32_t slotsToFirstEntry = gttcan->localSchedule[gttcan->localScheduleIndex] >> 16;
        // (in case master misses start of schedule, we should still transmit on schedule). 
        // This timer will be re-calculated if we receive a start of schedule frame
        gttcan->set_timer_int_callback((slotsRemainingInSchedule+slotsToFirstEntry) * gttcan->slotduration);
    }
    
    gttcan->transmit_callback(((globalScheduleIndex<<14) | slotID), data, gttcan->context_pointer);
}

void GTTCAN_start(gttcan_t * gttcan) {
    gttcan->localScheduleIndex = 0; // reset node to start of schedule.
    gttcan->isActive = true; // activate this node
    GTTCAN_transmit_next_frame(gttcan); // send first message in schedule - shoule be start of sxhedule frame for master
}
