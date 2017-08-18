/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "hid_device_demo.c"

// *****************************************************************************
/* EXAMPLE_START(hid_device_demo): HID Device (Server) Demo
 *
 * Status: Modified implementation, based on "hid_keyboard_demo" of BTstack, by kat-kai.
 * HID Request from Host are not answered yet too.
 *
 * @text This HID Device example demonstrates how to implement a HID mouse.
 * You can type from the terminal to move the mouse pointer.
 * q: left click
 * e: right click
 *
 * w: move upward
 * s: move downward
 * a: move left
 * d: move right
 */
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack.h"
#include "btstack_stdin.h"

static uint8_t hid_service_buffer[250];
static const char hid_device_name[] = "HID Mouse Demo";
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t hid_cid;

// from USB HID Specification 1.1, Appendix B.1
const uint8_t hid_descriptor_mouse_boot_mode[] = {

    0x05, 0x01,              // Usage Page (Generic Desktop)
    0x09, 0x02,              // Usage (Mouse)
    0xA1, 0x01,              // Collection (Application)

    0x09, 0x01,              // Usage (Pointer)
    0xA1, 0x00,              // Collection (Physical)

    0x05, 0x09,              // Usage Page (Buttons)
    0x19, 0x01,              // Usage Minimum (01)
    0x29, 0x03,              // Usage Maximun (03)
    0x15, 0x00,              // Logical Minimum (0)
    0x25, 0x01,              // Logical Maximum (1)
    0x95, 0x03,              // Report Count (3)
    0x75, 0x01,              // Report Size (1)
    0x81, 0x02,              // Input (Data, Variable, Absolute), ;3 button bit
    0x95, 0x01,              // Report Count (1)
    0x75, 0x05,              // Report Size (5)
    0x81, 0x01,              // Input (Constant), ;5 bit paddin
    0x05, 0x01,              // Usage Page (Generic Desktop)
    0x09, 0x30,              // Usage (X)
    0x09, 0x31,              // Usage (Y)
    0x15, 0x81,              // Logical Minimum (-127)
    0x25, 0x7F,              // Logical Maximum (127)
    0x75, 0x08,              // Report Size (8)
    0x95, 0x02,              // Report Count (2)
    0x81, 0x06,              // Input (Data, Variable, Relative), ;2 position bytes (X & Y)

    0xC0,              // End Collection

    0xC0,              //End Collectio
};

static int lookup_move_keycode(char character, uint8_t * click_flag, int8_t * x_move, int8_t *  y_move) {
  int move_val = 10;

  *click_flag = 0;
  *x_move = 0;
  *y_move = 0;

  switch(character) {
    case 'q': //left click
      *click_flag |= 0x01;
      return 1;
    case 'e': //right click
      *click_flag |= 0x02;
      return 1;
    case 'w': //up
      *y_move = -move_val;
      return 1;
    case 'a': //left
    *x_move = -move_val;
      return 1;
    case 's': //down
      *y_move = move_val;
      return 1;
    case 'd': //right
      *x_move = move_val;
      return 1;
    default:
      return 0;
  }
}

// HID Report sending
static int send_click_flag;
static int send_x_move;
static int send_y_move;

static void send_key(int click_flag, int x_move, int y_move){
    send_click_flag = click_flag;
    send_x_move = x_move;
    send_y_move = y_move;
    hid_device_request_can_send_now_event(hid_cid);
}

static void send_report(int click_flag, int x_move, int y_move){
    //uint8_t click_flag = 0; bit7~3: zero, bit2: wheel button, bit1: right button, bit0: left button
    uint8_t wheel_scroll = 0;
    uint8_t report[] = { 0xa1, click_flag, x_move, y_move, wheel_scroll};
    hid_device_send_interrupt_message(hid_cid, &report[0], sizeof(report));
}

// Demo Application
static void stdin_process(char character){
    uint8_t click_flag;
    int8_t x_move;
    int8_t y_move;
    int found = lookup_move_keycode(character, &click_flag, &x_move, &y_move);
    if (found){
        send_key(click_flag, x_move, y_move);
        return;
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (packet[0]){
                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    log_info("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", hci_event_user_confirmation_request_get_numeric_value(packet));
                    log_info("SSP User Confirmation Auto accept\n");
                    break;

                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)){
                        case HID_SUBEVENT_CONNECTION_OPENED:
                            if (hid_subevent_connection_opened_get_status(packet)) return;
                            hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            printf("HID Connected, please start typing...\n");
                            break;
                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            printf("HID Disconnected\n");
                            hid_cid = 0;
                            break;
                        case HID_SUBEVENT_CAN_SEND_NOW:
                            if ((send_x_move != 0) || (send_y_move != 0) || (send_click_flag != 0)){
                                send_report(send_click_flag, send_x_move, send_y_move);
                                send_click_flag = 0;
                                send_x_move = 0;
                                send_y_move = 0;
                                hid_device_request_can_send_now_event(hid_cid);
                            } else {
                                send_report(0, 0, 0);
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * To run a HID Device service you need to initialize the SDP, and to create and register HID Device record with it.
 * At the end the Bluetooth stack is started.
 */

/* LISTING_START(MainConfiguration): Setup HID Device */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    hci_register_sco_packet_handler(&packet_handler);

    gap_discoverable_control(1);
    gap_set_class_of_device(0x2540);
    gap_set_local_name(hid_device_name);

    // L2CAP
    l2cap_init();

    // SDP Server
    sdp_init();
    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));
    // hid sevice subclass 2540 Keyboard, hid counntry code 33 US, hid virtual cable off, hid reconnect initiate off, hid boot device off
    // hid service subclass of Mouse may also be 0x2540.
    hid_create_sdp_record(hid_service_buffer, 0x10001, 0x2540, 33, 0, 0, 0, hid_descriptor_mouse_boot_mode, sizeof(hid_descriptor_mouse_boot_mode), hid_device_name);
    printf("SDP service record size: %u\n", de_get_len( hid_service_buffer));
    sdp_register_service(hid_service_buffer);

    // HID Device
    hid_device_init();
    hid_device_register_packet_handler(&packet_handler);

    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
