#include "sysdeps.h"

#include <emscripten.h>

#include <map>

#include "cpu_emulation.h"
#include "ether.h"
#include "ether_defs.h"
#include "ether_helpers.h"
#include "macos_util.h"
#include "main.h"
#include "prefs.h"

#define DEBUG 0
#include "debug.h"

#ifdef SHEEPSHAVER
static bool net_open = false;  // Flag: initialization succeeded, network device open
static uint8 ether_addr[6];    // Our Ethernet address

// Error codes
enum { eMultiErr = -91, eLenErr = -92, lapProtErr = -94, excessCollsns = -95 };

#define ether_wds_to_buffer ether_msgb_to_buffer

#endif

static const uint8 GENERATED_ETHER_ADDR_PREFIX = 0xb2;

// Attached network protocols, maps protocol type to MacOS handler address
static std::map<uint16, uint32> net_protocols;

// Use stringified form of MAC address on the JS side, so that it's easier to
// debug.
static void ether_addr_to_str(uint8* ether_addr, char* ether_addr_str) {
  sprintf(ether_addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", ether_addr[0], ether_addr[1],
          ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]);
}

#ifndef SHEEPSHAVER

// Dispatch packet to protocol handler
static void ether_dispatch_packet(uint32 p, uint32 length) {
  // Get packet type
  uint16 type = ReadMacInt16(p + 12);

  // Look for protocol
  uint16 search_type = (type <= 1500 ? 0 : type);
  if (net_protocols.find(search_type) == net_protocols.end()) {
    D(bug("Could not dispatch packet: no protocol\n"));
    return;
  }
  uint32 handler = net_protocols[search_type];

  // No default handler
  if (handler == 0) {
    D(bug("Could not dispatch packet: no protocol handler\n"));
    return;
  }

  // Copy header to RHA
  Mac2Mac_memcpy(ether_data + ed_RHA, p, 14);
  D(bug(" header %08x%04x %08x%04x %04x\n", ReadMacInt32(ether_data + ed_RHA),
        ReadMacInt16(ether_data + ed_RHA + 4), ReadMacInt32(ether_data + ed_RHA + 6),
        ReadMacInt16(ether_data + ed_RHA + 10), ReadMacInt16(ether_data + ed_RHA + 12)));

  // Call protocol handler
  M68kRegisters r;
  r.d[0] = type;                        // Packet type
  r.d[1] = length - 14;                 // Remaining packet length (without header, for ReadPacket)
  r.a[0] = p + 14;                      // Pointer to packet (Mac address, for ReadPacket)
  r.a[3] = ether_data + ed_RHA + 14;    // Pointer behind header in RHA
  r.a[4] = ether_data + ed_ReadPacket;  // Pointer to ReadPacket/ReadRest routines
  D(
      bug(" calling protocol handler %08x, type %08x, length %08x, data %08x, "
          "rha %08x, read_packet %08x\n",
          handler, r.d[0], r.d[1], r.a[0], r.a[3], r.a[4]));
  Execute68k(handler, &r);
}

#endif

bool ether_init(void) {
  // Construct random Ethernet address. JS has access to a better random
  // number generator, use that to seed.
  int seed = EM_ASM_INT({ return workerApi.etherSeed(); });
  D(bug("ether_init seeded with  %d\n", seed));
  srand(seed);
  ether_addr[0] = GENERATED_ETHER_ADDR_PREFIX;
  ether_addr[1] = rand() % 0xff;
  ether_addr[2] = rand() % 0xff;
  ether_addr[3] = rand() % 0xff;
  ether_addr[4] = rand() % 0xff;
  ether_addr[5] = rand() % 0xff;

  char ether_addr_str[18];
  ether_addr_to_str(ether_addr, ether_addr_str);
  D(bug("ether_init with MAC address %s\n", ether_addr_str));
  EM_ASM_({ workerApi.etherInit(UTF8ToString($0)); }, ether_addr_str);

  return true;
}

void ether_exit(void) {
  // No-op
  D(bug("ether_exit\n"));
}

void ether_reset(void) {
  D(bug("ether_reset\n"));
  net_protocols.clear();
}

int16 ether_add_multicast(uint32 pb) {
  // No-op
  return noErr;
}

int16 ether_del_multicast(uint32 pb) {
  // No-op
  return noErr;
}

int16 ether_attach_ph(uint16 type, uint32 handler) {
  if (net_protocols.find(type) != net_protocols.end()) {
    D(bug("Ignoring ether_attach_ph with with already-attached handler\n"));
    return lapProtErr;
  }
  net_protocols[type] = handler;
  return noErr;
}

int16 ether_detach_ph(uint16 type) {
  if (net_protocols.erase(type) == 0) {
    D(bug("Ignoring ether_detach_ph with no handler\n"));
    return lapProtErr;
  }
  return noErr;
}

int16 ether_write(uint32 wds) {
  uint8 packet[1514];
  int len = ether_wds_to_buffer(wds, packet);

  // Extract destination address
  char dest_ether_addr_str[18];
  if (len >= 6 && packet[0] == GENERATED_ETHER_ADDR_PREFIX) {
    ether_addr_to_str(packet, dest_ether_addr_str);
  } else if (is_apple_talk_broadcast(packet)) {
    dest_ether_addr_str[0] = 'A';
    dest_ether_addr_str[1] = 'T';
    dest_ether_addr_str[2] = '\0';
  } else if (is_ethernet_broadcast(packet)) {
    dest_ether_addr_str[0] = '*';
    dest_ether_addr_str[1] = '\0';
  } else {
    D(bug("Ignoring ether_write with unexpected address"));
    return eMultiErr;
  }

  EM_ASM_({ workerApi.etherWrite(UTF8ToString($0), $1, $2); }, dest_ether_addr_str, packet, len);

  return noErr;
}

bool ether_start_udp_thread(int socket_fd) {
  // No-op
  return false;
}

void ether_stop_udp_thread(void) {
  // No-op
}

void EtherInterrupt(void) {
  EthernetPacket ether_packet;
  uint32 packet = ether_packet.addr();

  while (true) {
    int length = EM_ASM_INT({ return workerApi.etherRead($0, 1514); }, Mac2HostAddr(packet));
    if (length < 14) {
      return;
    }
#ifdef SHEEPSHAVER
    if (!ether_driver_opened) {
      // We can't dispatch packets before the ethernet driver is initialized
      // (ether_dispatch_packet_tvect is not initialized yet), don't do anything
      // for now.
      D(bug("Dropping Ethernet packet of %d bytes, driver has not been opened yet\n", length));
      continue;
    }
#endif
    ether_dispatch_packet(packet, length);
  }
}

// SheepShaver glue
#ifdef SHEEPSHAVER

void EtherInit(void) {
  net_open = false;

  if (PrefsFindBool("nonet")) {
    return;
  }

  net_open = ether_init();
}

void EtherExit(void) {
  ether_exit();
  net_open = false;
}

void AO_get_ethernet_address(uint32 arg) {
  uint8* addr = Mac2HostAddr(arg);
  if (net_open)
    OTCopy48BitAddress(ether_addr, addr);
  else {
    addr[0] = 0x12;
    addr[1] = 0x34;
    addr[2] = 0x56;
    addr[3] = 0x78;
    addr[4] = 0x9a;
    addr[5] = 0xbc;
  }
  D(bug("AO_get_ethernet_address: got address %02x%02x%02x%02x%02x%02x\n", addr[0], addr[1],
        addr[2], addr[3], addr[4], addr[5]));
}

void AO_enable_multicast(uint32 addr) {
  // No-op
}

void AO_disable_multicast(uint32 addr) {
  // No-op
}

void AO_transmit_packet(uint32 mp) {
  if (net_open) {
    switch (ether_write(mp)) {
      case noErr:
        num_tx_packets++;
        break;
      case excessCollsns:
        num_tx_buffer_full++;
        break;
    }
  }
}

void EtherIRQ(void) {
  num_ether_irq++;
  EtherInterrupt();
}

#endif  // SHEEPSHAVER
