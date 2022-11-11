#ifndef ETHER_HELPERS_H
#define ETHER_HELPERS_H

/*
 *  Check whether Ethernet address is AppleTalk or Ethernet broadcast address
 */

static inline bool is_apple_talk_broadcast(uint8 *p) {
  return p[0] == 0x09 && p[1] == 0x00 && p[2] == 0x07 && p[3] == 0xff &&
         p[4] == 0xff && p[5] == 0xff;
}

static inline bool is_ethernet_broadcast(uint8 *p) {
  return p[0] == 0xff && p[1] == 0xff && p[2] == 0xff && p[3] == 0xff &&
         p[4] == 0xff && p[5] == 0xff;
}

static void enable_apple_talk_in_pram(uint8 *XPRAM) {
  // node ID hint for printer port (AppleTalk)
  XPRAM[0x12] = 0x01;
  // serial ports config bits: 4-7 A, 0-3 B
  // 	useFree   0 Use undefined
  // 	useATalk  1 AppleTalk
  // 	useAsync  2 Async
  // 	useExtClk 3 externally clocked
  XPRAM[0x13] = 0x20;
  // AppleTalk Zone name ("*")
  XPRAM[0xbd] = 0x01;
  XPRAM[0xbe] = 0x2a;
  // AppleTalk Network Number
  XPRAM[0xde] = 0xff;
}

#endif /* ETHER_HELPERS_H */
