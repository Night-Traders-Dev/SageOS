#include "usb_device.h"
#include "otg_controller.h"

// Device Descriptor
static usb_device_descriptor_t device_desc = {
    .bLength = 18,
    .bDescriptorType = 1,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x02, // CDC
    .bMaxPacketSize0 = 64,
    .idVendor = 0x1234,
    .idProduct = 0x5678,
    .bcdDevice = 0x0100,
    .bNumConfigurations = 1
};

void usb_device_handle_setup(uint8_t* setup_packet) {
    uint8_t bmRequestType = setup_packet[0];
    uint8_t bRequest = setup_packet[1];
    uint16_t wValue = setup_packet[2] | (setup_packet[3] << 8);

    // GET_DESCRIPTOR request
    if (bmRequestType == 0x80 && bRequest == 0x06) {
        uint8_t descriptor_type = (wValue >> 8);
        if (descriptor_type == 0x01) { // Device
            // TODO: Write device_desc to FIFO
        }
    } 
    // CDC-ACM Class Request (bmRequestType 0x21)
    else if (bmRequestType == 0x21) {
        // SET_LINE_CODING or SET_CONTROL_LINE_STATE
        // CDC_SET_LINE_CODING = 0x20
        // CDC_SET_CONTROL_LINE_STATE = 0x22
        if (bRequest == 0x20 || bRequest == 0x22) {
            // Acknowledge request
        }
    }
}
