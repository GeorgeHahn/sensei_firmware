#!/usr/bin/env python

import serial
import time
import re
import getopt, sys
from os import listdir
from sensei import format_time_str

def usage():
    print 'set_time.py [options] device'
    print '  options:'
    print '    -d, --dry-run               Do not actually set time, but print out example time format'
    print '    -o hours, --offset=hours    Instead of current time, use current time - offset hours'

def main(argv):
    baudRate = 115200
    connected = False
    dry_run = False
    offset_hours = 0

    try:
        opts, args = getopt.getopt(argv,"do:",["dry-run", "offset="])
    except getopt.GetoptError:
        usage()
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-d", "--dry-run"):
            dry_run = True
        if opt in ("-o", "--offset"):
            offset_hours = int(arg)

    if len(args) != 1:
        usage()
        sys.exit(2)

    # /dev/cu.usbserial-AI04QOBN
    serial_device = args[0]

    if not dry_run:
        ser = serial.Serial(serial_device, baudRate)

        while not connected:
            serIn = ser.read()
            connected = True

        time_str = format_time_str(offset_hours)
        ser.write(bytes("T" + time_str)) #<200 ms to write
        print "Set time to %s" % time_str
    else:
        print "Time string is: T%s" % format_time_str(offset_hours)

if __name__ == "__main__":
   main(sys.argv[1:])
