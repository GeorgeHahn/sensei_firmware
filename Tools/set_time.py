#!/usr/bin/env python

import serial
import time
import datetime
import re
from os import listdir

baudRate = 115200
connected = False

# Find devices like: /dev/cu.usbserial-AI04QOBN
serial_devices = ['/dev/' + f for f in listdir('/dev') if re.match(r'cu.usbserial-AI', f)]

if len(serial_devices) == 0:
    print("No serial devices found")
    exit(0)

print "Using %s" % serial_devices[0]

ser = serial.Serial(serial_devices[0], baudRate)

while not connected:
    serIn = ser.read()
    connected = True

time = datetime.datetime.now().strftime('%m%d%y%w%H%M%S%f')[0:16]
ser.write(bytes("T" + time)) #<200 ms to write
print "Set time to %s" % time
