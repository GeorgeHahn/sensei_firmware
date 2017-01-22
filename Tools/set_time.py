#!/usr/bin/env python

import serial
import time
from datetime import datetime, timedelta
import re
import getopt, sys
from os import listdir

def get_time_str(offset_hours):
    t = datetime.now() - timedelta(hours=offset_hours)
    return t.strftime('%m%d%y%w%H%M%S%f')[0:16]

def main(argv):
    baudRate = 115200
    connected = False
    dry_run = False
    offset_hours = 0

    try:
        opts, args = getopt.getopt(argv,"do:",["dry-run", "offset="])
    except getopt.GetoptError:
        print 'set_time.py [options]'
        print '  options:'
        print '    -d, --dry-run               Do not actually set time, but print out example time format'
        print '    -o hours, --offset=hours    Instead of current time, use current time - offset hours'
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-d", "--dry-run"):
            dry_run = True
        if opt in ("-o", "--offset"):
            offset_hours = int(arg)

    if not dry_run:
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

        time_str = get_time_str(offset_hours)
        ser.write(bytes("T" + time_str)) #<200 ms to write
        print "Set time to %s" % time_str
    else:
        print "Time string is: T%s" % get_time_str(offset_hours)

if __name__ == "__main__":
   main(sys.argv[1:])
