# ESP32 Pinhole Camera

## Overview

This is an Arduino framework-based firmware for a pinhole camera using the Thinker AI ESP32 CAM board. The idea is to use the ESP32 CAM board and camera as a "point and shoot" camera that saves the pictures it takes to an SD card inserted into the SD card slot built into the ESP32 CAM board. The firmware will work "out of the box" with the lens and camera unmodified. It's called a "pinhole camera" because the impetus for building it was to make a simple digital pinhole camera by substituting a pinhole for the camera lens. Just because.

![The camera](./doc/ESP32%20Pinhole%20Camera.jpg)

The only controls are a switch that acts as the shutter release and the reset button. To use the camera, power up the ESP32 and wait for the built-in red LED to flash five times. (There are two built-in LEDS. The red one is marked LED1 and is on the side of the board  with the processor. The other is the white "flash" LED on the camera side.) Once the red LED flashes, the camera is ready. If it repeatedly flashes, pauses and flashes again, count the number of flashes in a group to figure out what went wrong:

    Flashes  Problem
    =======  ====================================
          2  Camera initialization failed
          3  SD card file system mount failed
          4  No SD Card found in the card reader

To use the camera, click its shutter. The red LED will flash once to indicate that the image was captured and saved. If the red LED doesn't flash, something went wrong. Maybe I'll add more error indicator LED flashing if this turns out to be a problem, but so far it hasn't.

Activity on the SD card occurs at two only points. First, during initialization. And, second, after the shutter is pressed and before the red LED flashes to indicate the image was captured. So, it should be okay to pull the power on the camera at other times.

If the shutter isn't clicked for five minutes the camera will flash the red LED five times and go into deep sleep mode. To get it going again, press the reset button on the board.

## Camera Construction

The body of the camera is laser cut from 3mm Baltic birch plywood. Here are the parts:

![Camera parts](./doc/Pinhole%20Camera%20Parts.svg)

The drawings are not kerf-compensated. 

The pinhole is made from thin kitchen aluminum foil stretched over a 6mm length of thin-walled brass tubing 10.33mm o.d. (It's 3/8-inch i.d. stuff I had around the shop.) The brass alignment pins are 2.9mm diameter driver pins for pin-tumbler locks. The shutter switch is a microswitch from my stock. The camera sensor and pinhole assembly is attached to the body of the camera with a M-3 screw that goes into a heat-stake nut pressed in from the back of the plywood. That lets me swap out different focal length pinhole assemblies.

## The Pinhole

For a pinhole camera to work at all well, the ratio of the distance from the hole to the sensor (the focal length) to the diameter of the pinhole (the entrance pupil) — called the f-stop — need to be at least 32, and preferably higher. We can make the focal length whatever we want. A long focal length means the pinhole diameter can be bigger, and so easier to make. But it also narrows the field of view that the camera sees.

![FoV](./doc/Pinhole%20Geometry.jpg)

Since the width of the sensor in the ESP-CAM is about 4mm, the field of view quickly narrows as the focal length increases. I wanted a relatively wide field of view, which means a hole smaller than 0.125mm. Fortunately, there's an easy way to make quite tiny pinholes: Put the piece of foil you like to make the hole in on a plastic-topped desk, and carefully poke it gently straight down from above with a sharp needle. The plastic is just soft enough to let the needle make a tiny hole.