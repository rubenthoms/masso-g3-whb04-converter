# Converter for using XHC Mach3 WHB04B-4 as Masso MPG pendant

This is C code that can be run on a Raspberry Pi and be used for reading USB input from the XHC Mach3 WHB04B-4 pendant and simulating an MPG pendant for the MASSO G3 CNC controller.

<img src="./img0.jpeg" alt="XHC WHB04B-4 pendant" title="XHC WHB04B-4 pendant" width="500px" />

## Disclaimer

The code (and wiring) is a hobby project and might not be perfect yet. The whole system was tested by me personally and has worked reliably so far. However, I do not take any responsibility for damaged mechanic or electronic parts due to using/modifying/wrongly implementing the code and/or the electronics. All is on your own risk. If you are unsure about the electronics or the code, please ask a professional to give you a helping hand.

## About the code

The code provided in this repo is scanning for a device with the defined vendor and product id. If such a device is connected to the Raspberry Pi, a connection is established and data is read from the device. A couple of button clicks are forwarded to respective GPIO pins. You can extend this as you like. The position of the two knobs is also forwarded, each position sets a certain pin to HIGH while the OFF position keeps them all at LOW.
In order to simulate the rotary encoder of the pendant, a thread is run which reads the current rotary speed of the pendant wheel (0-5 - clockwise, 255-250 - counter-clockwise) and sets the pins associated to the MPG A and B inputs to either HIGH or LOW according to how a rotary encoder would do. There is a factor to adjust the frequency.

## Installing `libusb`

```
sudo apt-get install libudev-dev
```

## Retrieving your pendant's product id

You have to find out your pendant's product id. The vendor id should be the same as long as you are using an XHC pendant. Connect your pendant to your Raspberry Pi and run `lsusb` in order to get a list of all connected devices:

```
$ lsusb
...
Bus 001 Device 003: ID 10ce:eb93 Silicon Labs 
...
```

`10ce` (4302) is your vendor id in hex and `eb93` (60307) your product id. Open the `main.c` file and replace the product (and possibly vendor) id accordingly.

## Prepare USB

Create `udev` rules for your device. Create a text file in `/etc/udev/rules.d/` named `whb04-b.rules` and add the following lines:

```
SUBSYSTEM=="usb", ATTRS{idVendor}=="4302", MODE="0666"
SUBSYSTEM=="usb_device", ATTRS{idVendor}=="4302", MODE="0666"
```

where `4302` is your pendant's vendor id. 

NOTE: This grants read and write access to all users, including non-privileged ones, for all USB devices with the same vendor id. You might want to fine-tune it. See here for more information: https://stackoverflow.com/a/22723445

## Compiling the code

```shell
gcc -Wall -pthread -L/usr/include/libusb-1.0 main.c -o main -lusb-1.0 -lpigpio
```

(you might need to install `pigpio`)

## Electronics

Each output of the WHB04-B pendant is forwarded to a GPIO pin of the RPi. Those are defined in lines 18 - 39 in `main.c`. 

In order to forward this output (high [3.3 V] or low [0 V]) to the MASSO controller, you have to use it to switch the 24 V to the respective input (either MPG pendant input or any other programmable input on the MASSO). I used a LTV-817 optocoupler per respective input with a 100 Ohm resistor in front of the LED on the input side (GPIO pin) and 27 kOhm as a working resistor on the output side of the optocoupler.

<img src="./img1.jpeg" alt="RPi 4B with 2 custom boards with LTV-817 optocouplers" title="RPi 4B with 2 custom boards with LTV-817 optocouplers" width="500px" /><img src="./img2.jpeg" alt="Note the different connection of the ES-1 <-> ES-2 cables" title="Note the different connection of the ES-1 <-> ES-2 cables" width="500px" />

```
GPIO pin (3.3/0 V)                                   MASSO ve+ (24 V)
|                                                     |
o---- 100 Ohm -----              -------27 kOhm ------o
                  |_###########_|
                    # O       #
                    # LTV-817 #
                   _#         #_
                  | ########### |
o------------------              ---------------------o
|                                                     |
GND                                                 Input on MASSO (pendant or programmable)
```

NOTE: For the E-STOP input of the MASSO, we must not switch the input pin to 24V but switch ES-1 to ES-2.

```
GPIO pin (3.3/0 V)                                   MASSO pendant ES-1
|                                                     |
o---- 100 Ohm -----              -------27 kOhm ------o
                  |_###########_|
                    # O       #
                    # LTV-817 #
                   _#         #_
                  | ########### |
o------------------              ---------------------o
|                                                     |
GND                                                 Masso pendant ES-2
```

You are free to connect as many of MASSO's programmable inputs to the WHB04-B pendant as you like. Some button functions of the pendant are not supported (yet) by MASSO while others are. But of course you can also use buttons for other than their intended functions.