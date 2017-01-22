#!/usr/bin/env python

import serial
import time
import datetime
import re, sys
from os import listdir

def main(argv):
    baudRate = 115200
    connected = False
    dry_run = False

    if len(argv) < 2:
        print "Usage: set_id.py serial_device id"
	exit(-1)

    ser = serial.Serial(argv[0], baudRate)

    while not connected:
        serIn = ser.read()
        connected = True

    id_to_write = int(argv[1])
    if id_to_write <= 0 or id_to_write >= 64:
        print "Id must be > 0 and < 64"
        exit(-1)

    ser.write("Z%02x" % id_to_write)
    time.sleep(0.1)
    ser.write("I")
    read_id = ord(ser.read(1))
    print "Set id to %d (0x%02x)" % (read_id, read_id)

if __name__ == "__main__":
   main(sys.argv[1:])
