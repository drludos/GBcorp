
#ifndef SERIAL_LINK_H
#define SERIAL_LINK_H

#include <stdbool.h>

#define LINK_CLOCK_EXT  0x00U
#define LINK_CLOCK_INT  0x01U
#define LINK_CLOCK_MASK 0x01U

#define LINK_XFER_START 0x80U // This gets cleared at the end of transfer
#define LINK_XFER_MASK  0x80U

#define LINK_SPEED_DMG  0x00U // 8KHz mode
#define LINK_SPEED_FAST 0x02U // 256KHz mode (512KHz in CGB high-speed mode)
#define LINK_SPEED_MASK 0x02U


#define LINK_SENDING_MASK   (LINK_XFER_MASK | LINK_CLOCK_MASK)
#define LINK_SENDING_ACTIVE (LINK_XFER_START | LINK_CLOCK_INT)


// Transmit one byte using internal clock
// Expects the other device to be in receiving-waiting state
// The setup sequence is important to ensure correct operation
// OPTIONAL: 0. Ensure there isn't a pending transfer (SC_REG & Transmit flag set & Internal Clock)
// 1. SC_REG -> Set Clock to internal (transfer bit cleared) to be the sender
// 2. SB_REG -> Load data
// 3. SC_REG -> Enable transfer bit (leave clock set to internal)
#define LINK_SEND_IF_READY(byte) \
    if ((SC_REG & LINK_SENDING_MASK) != LINK_SENDING_ACTIVE) {\
        SC_REG = LINK_CLOCK_INT; \
        SB_REG = (byte); \
        SC_REG = (LINK_XFER_START | LINK_CLOCK_INT); \
    }


// Set up receive-waiting state
// 1. SC_REG -> Set Clock to external (transfer bit cleared) to be waiting for external sender
// 2. SB_REG -> Load placeholder data
// 3. SC_REG -> Enable transfer bit (leave clock set to external)
#define LINK_WAIT_RECEIVE_WHEN_READY(TX_DATA) \
	while ((SC_REG & LINK_SENDING_MASK) == LINK_SENDING_ACTIVE); \
    SC_REG = LINK_CLOCK_EXT; \
    SB_REG = TX_DATA; \
    SC_REG = (LINK_XFER_START | LINK_CLOCK_EXT);


bool link_update(uint8_t model_this_gb, uint8_t * p_model_recv);

void link_init(void);
void link_reset(void);
void link_enable(void);
void link_disable(void);


#endif // SERIAL_LINK_H
