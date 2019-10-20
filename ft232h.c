#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libftdi1/ftdi.h>
#include <libusb-1.0/libusb.h>
#include "ft232h.h"

/**
 * Log a fatal error an do some cleanup.
 */
void ft232h_fatal(struct ft232h_context *context, char *message) {
  fprintf(stderr, "%s: %s\n", message, ftdi_get_error_string(context->ftdi));
  ft232h_free(context);
  exit(-1);
}

/**
 * Set (or clear) a pin on the ACBUS register. This bus is used to set
 * addresses via shift register a well as control pins on the ROM module.
 *
 * This function does not write the value out to the register.
 */
void ft232h_set_control_pin(struct ft232h_context *context,
				   unsigned char pin, unsigned char bit) {
  // Get lsb bit value, since the bit parameter is either 0 or 1
  bit = (bit & 0x01) << pin;

  // Clear current bit
  context->control = context->control & (C_MASK - pin_to_value(pin));

  // Now we can set the bit
  context->control = context->control | bit;
}

/**
 * Update state of GPIO pins
 */
void ft232h_update(struct ft232h_context *context) {
  unsigned char bytes[6] = {
			    0x80, // DBUS
			    context->direction == FT232H_DATA_OUTPUT
			        ? context->data
			        : 0x00,
			    context->direction == FT232H_DATA_OUTPUT
			        ? 0xff
			        : 0x00,
			    0x82, // CBUS
			    context->control,
			    0xff
  };
  
  #ifdef DEBUG
  printf("ft232_update(): control = 0x%02x, data = 0x%02x\n", context->control, context->data);
  #endif
  if(ftdi_write_data(context->ftdi, bytes, 6) < 0) {
    ft232h_fatal(context, "Unable to write MPSSE bytes");
  }
}

/**
 * Toggle a clock pulse on the given pin.
 */
void ft232h_clock_control_pin(struct ft232h_context *context,
				     unsigned char pin) {
  #ifdef DEBUG
  printf("ft232h_clock_control_pin()\n");
  #endif
  ft232h_set_control_pin(context, pin, 0);
  ft232h_update(context);
  usleep(CLOCK_DELAY); // TOOO: nanosleep?
  ft232h_set_control_pin(context, pin, 1);
  ft232h_update(context);
  usleep(CLOCK_DELAY);
  ft232h_set_control_pin(context, pin, 0);
  ft232h_update(context);
}

/**
 * Clock address out into shift registers, then clock the output register.
 */
void ft232h_set_address(struct ft232h_context *context, unsigned short address) {
  #ifdef DEBUG
  printf("ft232h_set_address(0x%02x)\n", address);
  #endif

  ft232h_set_control_pin(context, PIN_SER, 0);
  ft232h_set_control_pin(context, PIN_RCLK, 0);
  ft232h_update(context);
  usleep(10);
  
  for(int i = 0; i < 16; i++) {
    #ifdef DEBUG
    printf("ft232h_set_address(0x%04x): bit = %i\n", address, address & 0x01);
    #endif
    
    ft232h_set_control_pin(context, PIN_SER, address & 0x01);
    ft232h_update(context);
    usleep(10);
    ft232h_clock_control_pin(context, PIN_SRCLK);
    
    address = address >> 1;
  }

  ft232h_set_control_pin(context, PIN_SER, 0);
  ft232h_clock_control_pin(context, PIN_RCLK);
  ft232h_set_control_pin(context, PIN_RCLK, 0);
  ft232h_set_control_pin(context, PIN_SRCLK, 0);
  usleep(10);
  ft232h_update(context);
}

/**
 * Sets data on the ADBUS, which is connected to the EEPROM data lines
 */
void ft232h_set_data(struct ft232h_context *context, unsigned char data) {
  context->data = data;
}

/**
 * Read a byte from ADBUS
 */
void ft232h_read_data(struct ft232h_context *context) {
  context->direction = FT232H_DATA_INPUT;
  ft232h_update(context);
  
  unsigned char bytes[1] = { 0x81 };
  if(ftdi_write_data(context->ftdi, &bytes[0], 1) < 0) {
    ft232h_fatal(context, "Unable to request read");
  }
  
  int ret = 0;
  while((ret = ftdi_read_data(context->ftdi, &bytes[0], 1)) <= 0) {
    if(ret < 0) {
      ft232h_fatal(context, "Unable to read ADBUS");
    }
    usleep(READ_WAIT_NS);
  }
  context->data = bytes[0];
  context->direction = FT232H_DATA_OUTPUT;
  ft232h_update(context);
}

/**
 * Public procedures
 */

void ft232h_init(struct ft232h_context *context) {
  int ret;
  unsigned int chipid;
  struct ftdi_device_list *dev;
  struct ftdi_version_info version;

  if((context->ftdi = ftdi_new()) == 0) {
    fprintf(stderr, "ftdi_new failed\n");
    exit(-1);
  }

  version = ftdi_get_library_version();
  printf("ft232-eeprom programmer with libftdi %s\n", version.version_str);
  
  // Try to list all devices
  if((ret = ftdi_usb_find_all(context->ftdi, &dev, 0, 0)) < 0) {
    ft232h_fatal(context, "Error enumerating devices");
  } else if(ret == 0) {
    ft232h_fatal(context, "No devices found");
  }
  printf("Found device at %d:%d\n",
	 libusb_get_bus_number(dev->dev),
	 libusb_get_device_address(dev->dev));

  // We'll go for the first device
  // todo: Check if it is a compatible device?
  if(ftdi_usb_open_dev(context->ftdi, dev->dev) < 0) {
    ftdi_list_free(&dev);
    ft232h_fatal(context, "Unable to open device");
  }

  if(ftdi_read_chipid(context->ftdi, &chipid) < 0) {
    ft232h_fatal(context, "Failed reading chip id");
  }
  printf("Opened chip %d\n", chipid);
  
  // Enable MPSSE
  ftdi_set_bitmode(context->ftdi, 0, BITMODE_RESET);
  ftdi_set_bitmode(context->ftdi, 0, BITMODE_MPSSE);

  // Set default states
  ft232h_set_control_pin(context, PIN_SRCLK, 0);
  ft232h_set_control_pin(context, PIN_SER, 0);
  ft232h_set_control_pin(context, PIN_RCLK, 0);
}

void ft232h_read(struct ft232h_context *context, unsigned short address) {
  ft232h_set_control_pin(context, PIN_CE, 0);
  ft232h_set_control_pin(context, PIN_OE, 0);
  ft232h_set_control_pin(context, PIN_WE, 1);
  ft232h_set_address(context, address);
  ft232h_read_data(context);
}

void ft232h_write(struct ft232h_context *context, unsigned short address, char value) {
  ft232h_set_control_pin(context, PIN_CE, 0);
  ft232h_set_control_pin(context, PIN_OE, 1);
  ft232h_set_control_pin(context, PIN_WE, 1);
  ft232h_set_address(context, address);
  ft232h_set_data(context, value);
  ft232h_update(context);
  ft232h_set_control_pin(context, PIN_WE, 0);
  ft232h_update(context);
  usleep(1);
  ft232h_set_control_pin(context, PIN_WE, 1);
  ft232h_update(context);
}

void ft232h_free(struct ft232h_context *context) {
  ftdi_usb_close(context->ftdi);
  ftdi_deinit(context->ftdi);
  free(context);
}
