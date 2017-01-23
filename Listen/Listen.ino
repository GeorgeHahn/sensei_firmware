/*
 * Copyright (c) 2015 RF Digital Corp. All Rights Reserved.
 *
 * The source code contained in this file and all intellectual property embodied in
 * or covering the source code is the property of RF Digital Corp. or its licensors.
 * Your right to use this source code and intellectual property is non-transferable,
 * non-sub licensable, revocable, and subject to terms and conditions of the
 * SIMBLEE SOFTWARE LICENSE AGREEMENT.
 * http://www.simblee.com/licenses/SimbleeSoftwareLicenseAgreement.txt
 *
 * THE SOURCE CODE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND.
 *
 * This heading must NOT be removed from this file.
 */

#include "SimbleeCOM.h"

typedef struct packet {
  unsigned long millis;
  unsigned int esn;
  unsigned char data[15];
  int len;
  int rssi;
} packet;

#define BUFFER_SIZE 256

packet packets[BUFFER_SIZE];
unsigned int head;
unsigned int tail;
unsigned int count;

void setup() {
  Serial.begin(115200);
  SimbleeCOM.mode = LOW_LATENCY;
  SimbleeCOM.begin();
  tail = 0;
  head = 0;
  count = 0;
}

void loop() {
  if (count > 0) {
    packet *p = &packets[tail];
    printf("%08d %d %0x08x ", p->millis, p->rssi, p->esn);
    
    for (int i = 0; i < p->len; i++) {
      printf("%02x ", p->data[i]);
    } 
    printf("\r\n");  
    count--;
    tail++;
    if (tail >= BUFFER_SIZE) {
      tail = 0;
    }
  }
}

void SimbleeCOM_onReceive(unsigned int esn, const char *payload, int len, int rssi)
{
  packet *p = &packets[head];
  p->millis = millis();
  p->esn = esn;
  memcpy(p->data, payload, len);
  p->len = len;
  p-> rssi = rssi;
  head++;
  count++;
  if (head >= BUFFER_SIZE) {
    head = 0;
  }
}
