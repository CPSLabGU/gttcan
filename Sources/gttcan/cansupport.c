/**
 * @file cansupport.c
 * @brief This file contains low-level CAN-bus support functions.
 */
#include <stdint.h>
#include <stdbool.h>

#include "gttcan.h"

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
uint32_t GTTCAN_calculate_stuffed_bits(const uint8_t * const buffer, const uint32_t length) // cppcheck-suppress misra-c2012-8.7
{
    uint32_t stuffed_bits = 0U;
    uint32_t consecutive_bits = 0U;
    uint8_t last_bit = 0U;

    for (uint32_t i = 0U; i < length; i++)
    {
        uint8_t remaining_bits = buffer[i];
        for (uint32_t j = 0U; j < 8U; j++)
        {
            const uint8_t bit = remaining_bits & 1U;
            remaining_bits >>= 1U;
            if (bit == last_bit)
            {
                consecutive_bits++;
                if (consecutive_bits == 5U)
                {
                    stuffed_bits++;
                    consecutive_bits = 0U;
                }
            }
            else
            {
                consecutive_bits = 1U;
                last_bit = bit;
            }
        }
    }
    return stuffed_bits;
}

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
// Function to calculate total bits in CAN frame
uint32_t GTTCAN_calculate_can_frame_bits(const uint8_t * const buffer, const uint32_t length, const bool is_extended)
{
    // Fixed bits: SOF(1), Identifier(11 or 29), RTR(1), IDE(1), r0(1), DLC(4), CRC(15), CRC Delimiter(1), ACK Slot(1), ACK Delimiter(1), EOF(7)
    const uint32_t identifier_bits = is_extended ? 29U : 11U;
    const uint32_t fixed_bits = 1U + identifier_bits + 1U + 1U + 1U + 4U + 15U + 1U + 1U + 1U + 7U;
    const uint32_t payload_bits = length * 8U; // 8 bits per byte
    const uint32_t stuffed_bits = GTTCAN_calculate_stuffed_bits(buffer, length);

    return fixed_bits + payload_bits + stuffed_bits;
}
