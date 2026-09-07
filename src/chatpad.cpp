#include "chatpad.h"
#include <Arduino.h>

static const uint8_t kInitMsg[] = { 0x87, 0x02, 0x8C, 0x1F, 0xCC };
static const uint8_t kAwakeMsg[] = { 0x87, 0x02, 0x8C, 0x1B, 0xD0 };

static const uint8_t kStartLut[16] = {
  0x0F, 0x1E, 0x2D, 0x3C,
  0x4B, 0x5A, 0x69, 0x78,
  0x87, 0x96, 0xA5, 0xB4,
  0xC3, 0xD2, 0xE1, 0xF0,
};

bool Chatpad::valid_start(uint8_t b) const {
  if (b == 0x41)
    return true;
  return kStartLut[(b >> 4) & 0x0F] == b;
}

void Chatpad::send_awake() {
#if CP_DEBUG_RAW
  Serial.print("\nTX> ");
  for (uint8_t i = 0; i < sizeof(kAwakeMsg); i++) {
    Serial.printf("%02X ", kAwakeMsg[i]);
  }
  Serial.println();
#endif
  _last_rx = millis();
  _serial->write(kAwakeMsg, sizeof(kAwakeMsg));
}

void Chatpad::send_init_awake() {
#if CP_DEBUG_RAW
  Serial.print("\nTX> ");
  for (uint8_t i = 0; i < sizeof(kInitMsg); i++) {
    Serial.printf("%02X ", kInitMsg[i]);
  }
  Serial.print("| ");
  for (uint8_t i = 0; i < sizeof(kAwakeMsg); i++) {
    Serial.printf("%02X ", kAwakeMsg[i]);
  }
  Serial.println();
#endif
  _last_rx = millis();
  _serial->write(kInitMsg, sizeof(kInitMsg));
  _serial->write(kAwakeMsg, sizeof(kAwakeMsg));
}

void Chatpad::begin(HardwareSerial &serial, key_event_fn cb) {
  _serial = &serial;
  _cb = cb;
  _buflen = 0;
  _synced = false;
  _mods = 0;
  _last_mods = 0;
  _last_k0 = 0;
  _last_k1 = 0;
  _last_rx = millis();

  _serial->write(kInitMsg, sizeof(kInitMsg));
  _serial->write(kAwakeMsg, sizeof(kAwakeMsg));
}

void Chatpad::feed(uint8_t b) {
  if (_buflen == sizeof(_buf)) {
    // no resolvable frame in the window: drop the oldest byte
    memmove(_buf, _buf + 1, _buflen - 1);
    _buflen--;
  }
  _buf[_buflen++] = b;
  drain();
}

// Resynchronising scanner. Frames are only consumed once their claimed
// length and checksum validate; a candidate that fails is retried from its
// second byte. This recovers from mid-stream joins and from data bytes that
// happen to look like frame markers (0x0F/0xF0 inside idle reports).
void Chatpad::drain() {
  for (;;) {
    if (_buflen == 0)
      return;

    // scan to the earliest byte that can start a frame
    uint8_t k = 0;
    while (k < _buflen && !valid_start(_buf[k]))
      k++;
    if (k == _buflen) {
      _buflen = 0;
      return;
    }
    if (k > 0) {
      memmove(_buf, _buf + k, _buflen - k);
      _buflen -= k;
    }

    if (_buf[0] == 0x41) {
      // version/serial frame: 12 bytes, no checksum; comms are live
      if (_buflen < 12)
        return;
      _synced = true;
      memmove(_buf, _buf + 12, _buflen - 12);
      _buflen -= 12;
      continue;
    }

    if (_buflen < 2)
      return; // need the second header byte to know the length
    uint8_t n = _buf[1] & 0x0F; // payload bytes
    if (_buflen < (uint16_t)2 + n + 1)
      return; // need the full frame plus its checksum

    uint8_t cs = _buf[2 + n];

    // belse-de computed the checksum over the payload only; Biffle's working
    // parser summed the whole frame. Accept either.
    uint8_t sum = 0;
    for (uint8_t i = 0; i < n; i++)
      sum += _buf[2 + i];
    bool ok = cs == (uint8_t)(-sum);
    if (!ok) {
      uint8_t sumAll = 0;
      for (uint8_t i = 0; i < 2 + n; i++)
        sumAll += _buf[i];
      ok = cs == (uint8_t)(-sumAll);
    }

    if (!ok) {
      // not a real frame: drop just this start byte and keep scanning
      memmove(_buf, _buf + 1, _buflen - 1);
      _buflen--;
      continue;
    }

    _synced = true;
    if (_buf[0] == 0xB4 && n == 5)
      emit_diff(_buf[3], _buf[4], _buf[5]);

    memmove(_buf, _buf + (2 + n + 1), _buflen - (2 + n + 1));
    _buflen -= 2 + n + 1;
  }
}

void Chatpad::emit_diff(uint8_t mods, uint8_t k0, uint8_t k1) {
  uint8_t changed = mods ^ _last_mods;
  if (changed & CHAT_MOD_SHIFT)
    _cb(CHAT_KEY_SHIFT, mods & CHAT_MOD_SHIFT);
  if (changed & CHAT_MOD_GREEN)
    _cb(CHAT_KEY_GREEN, mods & CHAT_MOD_GREEN);
  if (changed & CHAT_MOD_ORANGE)
    _cb(CHAT_KEY_ORANGE, mods & CHAT_MOD_ORANGE);
  if (changed & CHAT_MOD_PEOPLE)
    _cb(CHAT_KEY_PEOPLE, mods & CHAT_MOD_PEOPLE);
  _last_mods = mods;
  _mods = mods;

  if (k0 && k0 != _last_k0 && k0 != _last_k1) _cb(k0, true);
  if (k1 && k1 != _last_k0 && k1 != _last_k1) _cb(k1, true);
  if (_last_k0 && _last_k0 != k0 && _last_k0 != k1) _cb(_last_k0, false);
  if (_last_k1 && _last_k1 != k0 && _last_k1 != k1) _cb(_last_k1, false);
  _last_k0 = k0;
  _last_k1 = k1;
}

void Chatpad::poll() {
  while (_serial->available()) {
    uint8_t b = _serial->read();
#if CP_DEBUG_RAW
    Serial.printf("%02X ", b);
#endif
    _last_rx = millis();
    feed(b);
  }
#if CP_DEBUG_RAW
  Serial.flush();
#endif

  uint32_t now = millis();
  uint32_t idle = now - _last_rx;
  if (idle > 2500)
    send_init_awake();
  else if (idle > 700)
    send_awake();
}
