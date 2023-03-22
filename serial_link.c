#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <gb/isr.h>

#include "serial_link.h"


#define LINK_SEND_INTERVAL      30u // Every two seconds
#define LINK_SEND_INTERVAL_RDY  0u
#define LINK_RECV_TIME_RESTART  (LINK_SEND_INTERVAL * 4u) // Expire link state if N transmissions in a row aren't received
#define LINK_RECV_TIME_EXPIRED  0u

#define LINK_CHK_MASK          0x88u
#define LINK_CHK_EXPECT        0x80u
#define LINK_DATA_MASK         0x07u // 3 bits worth of data
#define LINK_DATA_CHK_MASK     0x70u // 3 bits worth of data
#define LINK_CHK_DATA_UPSHIFT  4    // Upshift 4 bits for XORed check copy of data bits

#define LINK_DATA_IDLE  0x00u      // Placeholder data for serial send reg when idle

static uint8_t link_send_counter = LINK_SEND_INTERVAL;
static uint8_t link_recv_timeout = LINK_RECV_TIME_EXPIRED;

static bool link_recv_newdata = false;


// Default to no linked player
// Use MODEL_DISCONNECTED instead of MODEL_EMPTY to ensure
// there aren't accidental matches in the disconnected state.
static uint8_t link_recv_data_prev    = MODEL_DISCONNECTED;
static uint8_t link_recv_data_current = MODEL_DISCONNECTED;



// * Periodic send out of data
// * Update the Serial Link state and variables
//
// Returns: True if there has been a state change
//          that needs to be acted on
bool link_update(uint8_t model_to_send, uint8_t * p_model_recv) {

    bool state_changed = false;

    // ==== HANDLE TRANSMITTING DATA ====

    // Update timeout of received link data
    if (link_send_counter) {
        link_send_counter--;

        if (link_send_counter == LINK_SEND_INTERVAL_RDY) {
            model_to_send |= (((model_to_send & LINK_DATA_MASK) ^ LINK_DATA_MASK) << LINK_CHK_DATA_UPSHIFT) | LINK_CHK_EXPECT;
            // Try to send model for current GB, then reset counter
            LINK_SEND_IF_READY(model_to_send);
            link_send_counter = LINK_SEND_INTERVAL;
        }
    }


    // ==== HANDLE RECEIVED DATA ====

    // Check for new data, make sure Serial ISR doesn't update it at the same time
    __critical {
        if (link_recv_newdata == true) {
            // Reset expiration for received data
            link_recv_timeout = LINK_RECV_TIME_RESTART;
            link_recv_newdata = false;

            // Check if model type changed
            if (link_recv_data_prev != link_recv_data_current) {
                link_recv_data_prev = link_recv_data_current;
                state_changed = true;
            }
        }
    }

    // Update timeout of received link data
    if (link_recv_timeout) {
        link_recv_timeout--;

        // Link expired, reset received data
        if (link_recv_timeout == LINK_RECV_TIME_EXPIRED) {
            link_recv_data_current = MODEL_DISCONNECTED;
            link_recv_data_prev    = MODEL_DISCONNECTED;
            state_changed = true;
        }
    }

    // Copy updated link model into param for return
    if (state_changed) {
        *p_model_recv = link_recv_data_current;
        return true;
    }
    else
        return false;
}


// Serial link interrupt handler
void link_serial_isr() __critical __interrupt {

    uint8_t link_data = SB_REG;

    // Internal clock means this device was: transfer initiator
    if (SC_REG & LINK_CLOCK_INT) {

        // Return to reveiver mode when ready
        LINK_WAIT_RECEIVE_WHEN_READY(LINK_DATA_IDLE); // Placeholder byte
    }
    else {
        // Otherwise ISR was triggered by received data

        // Validate received link byte
        // Test flag bits for expected value
        if ((link_data & LINK_CHK_MASK) == LINK_CHK_EXPECT) {

            // Extract payload data bits
            uint8_t link_data_bits = link_data & LINK_DATA_MASK;

            // * Test data bits against upshifted and xored copy of them
            if ((link_data & LINK_DATA_CHK_MASK) == (link_data_bits ^ LINK_DATA_MASK) << LINK_CHK_DATA_UPSHIFT) {
                // data looks ok, make a copy to global var
                link_recv_data_current = link_data_bits;
                link_recv_newdata = true;
            }
        }
    }
}

// Cause the SERIAL ISR handler function to be registered as the ISR
ISR_VECTOR(VECTOR_SERIAL, link_serial_isr)



// Initialize ISR
void link_init(void) {

    link_reset();
    link_enable();
}


void link_reset(void) {

	// Default to receiver mode
	LINK_WAIT_RECEIVE_WHEN_READY(LINK_DATA_IDLE); // Placeholder byte
}


void link_enable(void) {

    // Enable link ISR
    __critical {
	    set_interrupts(IE_REG |= SIO_IFLAG);
    }
}


void link_disable(void) {

    // Disable link ISR
    __critical {
	    set_interrupts(IE_REG &= ~SIO_IFLAG);
    }
    link_reset();
}
