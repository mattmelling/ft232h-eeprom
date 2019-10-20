#ifndef FT232H_H

#define FT232H_H

// Control word mask
#define C_MASK 0xff

// Shift register clock pin
#define PIN_SRCLK 0

// Shift register data pin
#define PIN_SER 1

// Internal register clock pin
#define PIN_RCLK 2

// Chip Enable
#define PIN_CE 3

// Output Enable
#define PIN_OE 6

// Write Enable
#define PIN_WE 5

// Microsecond delay for clock
#define CLOCK_DELAY 1

// How long to wait between data bus reads
#define READ_WAIT_NS 10

// Convert a pin number to a value which we can use for bitwise masks
#define pin_to_value(pin) (0x01 << pin)

#define FT232H_DATA_OUTPUT 0
#define FT232H_DATA_INPUT 1

/**
 * Represents the current state of the device.
 */
struct ft232h_context {
  struct ftdi_context *ftdi; // Handle to FTDI library
  unsigned char control; // Control bits - address, clock, etc
  unsigned char data; // Data bits
  unsigned char direction; // direction for data bits, 1 = output, 0 = input
};

/**
 * Discover an available FTDI device and open it in the provided context.
 */
void ft232h_init(struct ft232h_context *context);

/**
 * Read value from ROM address and return in context->data
 */
void ft232h_read(struct ft232h_context *context, unsigned short address);

/**
 * Write a byte to the given address.
 */
void ft232h_write(struct ft232h_context *context, unsigned short address, char value);

/**
 * Close underlying libftdi layer and free given context.
 */
void ft232h_free(struct ft232h_context *context);

#endif
