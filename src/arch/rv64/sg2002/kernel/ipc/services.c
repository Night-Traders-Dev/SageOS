#include "ipc.h"
#include "../console/console.h"
#include "../drivers/sdio/sdio_controller.h"
#include "../drivers/usb/otg_controller.h"
#include <stdint.h>

#define WIFI_SERVICE_PID 10
#define USB_SERVICE_PID  11

void handle_ipc_message(ipc_msg_t* msg) {
    switch (msg->msg_type) {
        case IPC_USB_STATUS:
            // Invoke functional USB init
            usb_otg_init();
            break;
        case IPC_WIFI_SCAN:
            // Invoke functional SDIO command (scan command code is 0x01)
            sdio_send_command(0x01, 0);
            break;
        default:
            break;
    }
}
