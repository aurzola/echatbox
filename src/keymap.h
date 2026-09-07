/*
 * keymap.h - Chatpad keycode -> host key resolution.
 *
 * Layer tables describe the printed legends of the chatpad unit being used.
 * Values are Unicode code points (UTF-16 scope). Layer precedence is:
 * shift (white) > green (top-left symbols) > orange (bottom-right symbols).
 * This layout matches the European/multilingual Xbox 360 Chatpad (the one
 * whose keys carry accented characters such as ñ and ¡¿ on the orange
 * layer and !@#... on the green layer).
 */
#ifndef ECHATBOX_KEYMAP_H
#define ECHATBOX_KEYMAP_H

#include <stdint.h>

enum HostKeyKind {
  HK_NONE = 0,
  HK_ASCII = 1,    /* hostKey.ch holds a Unicode code point */
  HK_ENTER = 2,
  HK_BACKSPACE = 3,
  HK_LEFT = 4,
  HK_RIGHT = 5,
};

struct HostKey {
  uint8_t kind;
  uint16_t ch;
};

/*
 * Resolves a chatpad keycode to the host output given the active layer
 * bits (CHAT_MOD_*). Keys without a legend on that layer yield HK_NONE.
 * The People layer is intentionally ignored.
 */
HostKey chatToHost(uint8_t keycode, uint8_t layers);

#endif
