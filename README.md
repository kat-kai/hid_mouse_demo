## HID Mouse Demo for ESP32

- BTstack implementation for Bluetooth HID Mouse

*Note: Currently, you can use an official BTstack example code:* [hid_mouse_demo.c](https://github.com/bluekitchen/btstack/blob/develop/example/hid_mouse_demo.c)

### Requirements

- ESP-IDF
- [BTstack](https://github.com/bluekitchen/btstack) examples for ESP32

```
$ cd ~/esp
$ git clone https://github.com/bluekitchen/btstack.git
$ cd btstack/port/esp32
$ python ./create_example.py
```

### Preparation
```
$ cd ~/esp
$ git clone https://github.com/kat-kai/hid_mouse_demo.git
$ cp -r btstack/port/esp32/hid_keyboard_demo/ btstack/port/esp32/hid_mouse_demo
$ cp -r kat-kai/hid_mouse_demo btstack/port/esp32/
$ rm btstack/port/esp32/hid_mouse_demo/main/hid_keyboard_demo.c
$ cd btstack/port/esp32/hid_mouse_demo/
$ make flash
```

### Usage
1. Turn on the ESP32
2. Connect ESP32 by a terminal client (e.g. PuTTY) via serial line.
3. Connect ESP32 from your PC via Bluetooth. (Device name: "HID Mouse Demo")
4. Type the following characters from the terminal client to move your mouse pointer.
- q: left click
- e: right click
- w: move upward
- s: move downward
- a: move left
- d: move right
