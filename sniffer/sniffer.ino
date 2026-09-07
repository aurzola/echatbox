// echatbox sniffer - prints chatpad reports only when their content changes,
// so key presses can be mapped to frames without flooding the console.
#include <Arduino.h>

#define CP_UART_RX 16
#define CP_UART_TX 17

static uint8_t rep[8];
static uint8_t last[8];
static bool haveLast = false;
static uint32_t same = 0;
static uint32_t changed = 0;

static void printRep() {
  Serial.printf("[%lu] ", changed);
  for (int i = 0; i < 8; i++)
    Serial.printf("%02X ", rep[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(19200, SERIAL_8N1, CP_UART_RX, CP_UART_TX);
  // wake the chatpad a few times at boot
  for (int i = 0; i < 3; i++) {
    Serial2.write(0x87); Serial2.write(0x02);
    Serial2.write(0x8C); Serial2.write(0x1F); Serial2.write(0xCC);
    delay(200);
  }
  Serial.println("sniffer: waiting for A5 reports");
}

void loop() {
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    if (b != 0xA5)
      continue; // anchor on A5
    rep[0] = 0xA5;
    if (Serial2.readBytes(&rep[1], 7) != 7)
      continue; // incomplete report; wait for the next A5 anchor

    if (!haveLast) {
      haveLast = true;
      memcpy(last, rep, 8);
      printRep();
    } else if (memcmp(rep, last, 8) != 0) {
      memcpy(last, rep, 8);
      changed++;
      same = 0;
      printRep();
    } else {
      same++;
      if (same == 500) { // heartbeat: still streaming, unchanged
        Serial.println("... (stream idle, no changes)");
        same = 0;
      }
    }
  }
  delay(2);
}
