// echatbox - Xbox 360 Chatpad reader. BLE output optional.
//
// BLE off (default): solo lee el chatpad por UART2 y vuelca las teclas por
// consola (Serial, 115200). BLE on: además se anuncia como teclado Bluetooth.
#define ECHATBOX_BLE 0

#include <Arduino.h>

#include "src/chatpad.h"
#include "src/keymap.h"

#if ECHATBOX_BLE
#include <BleKeyboard.h>
#endif

// Chatpad UART on Serial2. ESP32 RX pin receives chatpad TX (TP9), ESP32 TX
// pin drives the chatpad RX line.
#define CP_UART_RX 16
#define CP_UART_TX 17

static Chatpad pad;

#if ECHATBOX_BLE
static BleKeyboard ble("Xbox 360 Chatpad", "Microsoft", 100);
static uint8_t lastOut[8];
static uint8_t lastOutCount = 0;
#endif

static uint8_t held[8];
static uint8_t heldCount = 0;
static bool wasSynced = false;

static bool pushUnique(uint8_t *arr, uint8_t &n, uint8_t v);
static void dropHeld(uint8_t kc);
static void syncReport();

// Emit a Unicode code point as UTF-8 so non-ASCII legends (ñ, é, €, ...)
// print correctly on the console.
static void printUtf8(uint16_t cp) {
  if (cp <= 0x7F) {
    Serial.write((uint8_t)cp);
  } else if (cp <= 0x7FF) {
    Serial.write((uint8_t)(0xC0 | (cp >> 6)));
    Serial.write((uint8_t)(0x80 | (cp & 0x3F)));
  } else {
    Serial.write((uint8_t)(0xE0 | (cp >> 12)));
    Serial.write((uint8_t)(0x80 | ((cp >> 6) & 0x3F)));
    Serial.write((uint8_t)(0x80 | (cp & 0x3F)));
  }
}

// Human-readable list of the layer modifiers currently held, e.g. "shift+green".
static const char *modsLabel(uint8_t m) {
  static char b[32];
  b[0] = '\0';
  if (m & CHAT_MOD_SHIFT)  strcat(b, "shift");
  if (m & CHAT_MOD_GREEN)  strcat(b, b[0] ? "+green" : "green");
  if (m & CHAT_MOD_ORANGE) strcat(b, b[0] ? "+orange" : "orange");
  if (m & CHAT_MOD_PEOPLE) strcat(b, b[0] ? "+people" : "people");
  return b;
}

void onKey(uint8_t kc, bool down) {
  uint8_t m = pad.modifiers();
  Serial.printf("[chatpad] %s key 0x%02X", down ? "dn" : "up", kc);
  if (!chatIsLayerKey(kc)) {
    HostKey h = chatToHost(kc, m);
    if (h.kind == HK_ASCII) {
      Serial.print(" -> '");
      printUtf8(h.ch);
      Serial.print("'");
    } else if (h.kind != HK_NONE) {
      Serial.printf(" -> special(%d)", h.kind);
    }
  }
  if (m)
    Serial.printf("  [%s]", modsLabel(m));
  Serial.println();

  if (chatIsLayerKey(kc))
    return;
  if (down) {
    if (heldCount < sizeof(held))
      pushUnique(held, heldCount, kc);
  } else {
    dropHeld(kc);
  }
}

bool pushUnique(uint8_t *arr, uint8_t &n, uint8_t v) {
  for (uint8_t i = 0; i < n; i++)
    if (arr[i] == v)
      return false;
  arr[n++] = v;
  return true;
}

void dropHeld(uint8_t kc) {
  for (uint8_t i = 0; i < heldCount; i++) {
    if (held[i] == kc) {
      held[i] = held[--heldCount];
      return;
    }
  }
}

#if ECHATBOX_BLE
uint8_t bleCodeFor(HostKey k) {
  switch (k.kind) {
    case HK_ASCII:
      // HID usages only exist for ASCII; non-ASCII legends (ñ, é, €, ...)
      // need a HID composite this sketch does not build, so skip them.
      return k.ch <= 0x7E ? (uint8_t)k.ch : 0;
    case HK_ENTER:
      return KEY_RETURN;
    case HK_BACKSPACE:
      return KEY_BACKSPACE;
    case HK_LEFT:
      return KEY_LEFT_ARROW;
    case HK_RIGHT:
      return KEY_RIGHT_ARROW;
    default:
      return 0;
  }
}

void syncReport() {
  uint8_t want[8];
  uint8_t wantN = 0;
  uint8_t layers = pad.modifiers();

  for (uint8_t i = 0; i < heldCount; i++) {
    HostKey k = chatToHost(held[i], layers);
    if (k.kind == HK_NONE)
      continue;
    uint8_t code = bleCodeFor(k);
    if (code)
      pushUnique(want, wantN, code);
  }

  for (uint8_t i = 0; i < lastOutCount; i++) {
    uint8_t c = lastOut[i];
    bool keep = false;
    for (uint8_t j = 0; j < wantN; j++)
      if (want[j] == c)
        keep = true;
    if (!keep)
      ble.release(c);
  }
  for (uint8_t i = 0; i < wantN; i++) {
    uint8_t c = want[i];
    bool heldOut = false;
    for (uint8_t j = 0; j < lastOutCount; j++)
      if (lastOut[j] == c)
        heldOut = true;
    if (!heldOut)
      ble.press(c);
  }

  lastOutCount = wantN;
  for (uint8_t i = 0; i < wantN; i++)
    lastOut[i] = want[i];
}
#endif // ECHATBOX_BLE

void setup() {
  Serial.begin(115200);

  Serial2.begin(19200, SERIAL_8N1, CP_UART_RX, CP_UART_TX);
  pad.begin(Serial2, onKey);

#if ECHATBOX_BLE
  ble.begin();
  Serial.println("echatbox: BLE on, pairing as 'Xbox 360 Chatpad'");
#else
  Serial.println("echatbox: BLE off, dumping chatpad keys to console");
#endif
}

void loop() {
  pad.poll();

  if (pad.synced() && !wasSynced) {
    wasSynced = true;
    Serial.println("[chatpad] synced (frames OK)");
  }
  if (!pad.synced() && wasSynced)
    wasSynced = false;

#if ECHATBOX_BLE
  if (ble.isConnected()) {
    syncReport();
  } else if (lastOutCount > 0) {
    ble.releaseAll();
    lastOutCount = 0;
  }
#endif

  delay(5);
}
