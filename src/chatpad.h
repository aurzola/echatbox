/*
 * chatpad.h - UART reader for the Xbox 360 Chatpad.
 *
 * Protocol derived from Cliff L. Biffle's "Chatpad" Arduino library
 * (BSD-3-Clause) and the reversing notes of fdufnews and belse-de.
 *
 * Bus: 3.3V UART, 19200 8N1. Frames start with a marker byte whose high
 * nibble maps back to itself (0xA5, 0xB4, ...). The second header byte
 * carries the payload length in its low nibble; a checksum byte follows the
 * payload. 0x41 version frames are 12 bytes.
 *
 * Checksum: two's complement of the sum of the frame bytes that precede it
 * (the payload-only convention used by belse-de is also accepted).
 *
 * The scanner is resynchronising: a frame is only committed once its full
 * length and checksum validate; otherwise the candidate start byte is
 * dropped and scanning resumes one byte later. This tolerates joining the
 * chatpad stream mid-frame and data bytes (0x0F, 0xF0) that look like frame
 * markers inside idle reports.
 *
 * Set CP_DEBUG_RAW to 1 to dump every RX byte (and TX command) as hex on the
 * default Serial so the protocol can be validated on the bench.
 */
#ifndef ECHATBOX_CHATPAD_H
#define ECHATBOX_CHATPAD_H

#include <stdint.h>
#include <HardwareSerial.h>

#ifndef CP_DEBUG_RAW
#define CP_DEBUG_RAW 0
#endif

enum {
  CHAT_KEY_7 = 0x11, CHAT_KEY_6 = 0x12, CHAT_KEY_5 = 0x13,
  CHAT_KEY_4 = 0x14, CHAT_KEY_3 = 0x15, CHAT_KEY_2 = 0x16,
  CHAT_KEY_1 = 0x17,
  CHAT_KEY_U = 0x21, CHAT_KEY_Y = 0x22, CHAT_KEY_T = 0x23,
  CHAT_KEY_R = 0x24, CHAT_KEY_E = 0x25, CHAT_KEY_W = 0x26,
  CHAT_KEY_Q = 0x27,
  CHAT_KEY_J = 0x31, CHAT_KEY_H = 0x32, CHAT_KEY_G = 0x33,
  CHAT_KEY_F = 0x34, CHAT_KEY_D = 0x35, CHAT_KEY_S = 0x36,
  CHAT_KEY_A = 0x37,
  CHAT_KEY_N = 0x41, CHAT_KEY_B = 0x42, CHAT_KEY_V = 0x43,
  CHAT_KEY_C = 0x44, CHAT_KEY_X = 0x45, CHAT_KEY_Z = 0x46,
  CHAT_KEY_RIGHT = 0x51, CHAT_KEY_M = 0x52, CHAT_KEY_PERIOD = 0x53,
  CHAT_KEY_SPACE = 0x54, CHAT_KEY_LEFT = 0x55,
  CHAT_KEY_COMMA = 0x62, CHAT_KEY_ENTER = 0x63, CHAT_KEY_P = 0x64,
  CHAT_KEY_0 = 0x65, CHAT_KEY_9 = 0x66, CHAT_KEY_8 = 0x67,
  CHAT_KEY_BACKSPACE = 0x71, CHAT_KEY_L = 0x72, CHAT_KEY_O = 0x75,
  CHAT_KEY_I = 0x76, CHAT_KEY_K = 0x77,

  CHAT_KEY_SHIFT = 0x81,
  CHAT_KEY_GREEN = 0x82,
  CHAT_KEY_PEOPLE = 0x83,
  CHAT_KEY_ORANGE = 0x84,
};

enum {
  CHAT_MOD_SHIFT = 1 << 0,
  CHAT_MOD_GREEN = 1 << 1,
  CHAT_MOD_ORANGE = 1 << 2,
  CHAT_MOD_PEOPLE = 1 << 3,
};

inline bool chatIsLayerKey(uint8_t k) {
  return k >= CHAT_KEY_SHIFT;
}

class Chatpad {
public:
  typedef void (*key_event_fn)(uint8_t keycode, bool down);

  // serial must already be configured (19200 8N1, 3.3V)
  void begin(HardwareSerial &serial, key_event_fn cb);
  void poll();
  uint8_t modifiers() const { return _mods; }
  bool synced() const { return _synced; }

private:
  HardwareSerial *_serial = nullptr;
  key_event_fn _cb = nullptr;
  uint8_t _buf[20];
  uint8_t _buflen = 0;
  bool _synced = false;
  uint8_t _mods = 0;
  uint8_t _last_mods = 0;
  uint8_t _last_k0 = 0;
  uint8_t _last_k1 = 0;
  uint32_t _last_rx = 0;

  bool valid_start(uint8_t b) const;
  void feed(uint8_t b);
  void drain();
  void emit_diff(uint8_t mods, uint8_t k0, uint8_t k1);
  void send_awake();
  void send_init_awake();
};

#endif
