#!/usr/bin/env python

import serial
import time
import datetime
import re, sys
from os import listdir
from sensei import format_time_str

def main(argv):
    baudRate = 115200
    connected = False
    dry_run = False

    if len(argv) < 2:
        print "Usage: fetch_pages.py serial_device id"
	exit(-1)

    ser = serial.Serial(argv[0], baudRate)

    while not connected:
        serIn = ser.read()
        connected = True

    sensor_id = int(argv[1])

    time_str = format_time_str(0)
    ser.write(bytes("T" + time_str)) #<200 ms to write
    print ser.readline()
    print ser.readline()

    time.sleep(1)
    ser.write("O")
    print ser.readline()

    ser.write("D%02x" % sensor_id)
    pageCount = ord(ser.read())
    print "pageCount = %d" % pageCount

    for expectedPageNum in xrange(1, pageCount+1):
        data = ser.read(3)
        pageNum = ord(data[0])
        pageSize = (ord(data[1]) << 8) + ord(data[2])
        print "Header: pageNum = %d, pageSize = %d" % (pageNum, pageSize)
        data = ser.read(pageSize)
        print "read %d bytes" % len(data)
        print data.encode('hex')

if __name__ == "__main__":
   main(sys.argv[1:])
