#include "sysdeps.h"

#include <emscripten.h>
#include <map>

#include "cpu_emulation.h"
#include "ether.h"
#include "ether_defs.h"
#include "macos_util.h"
#include "main.h"

#define DEBUG 0
#include "debug.h"

static const uint8 GENERATED_ETHER_ADDR_PREFIX = 0xb2;

// Attached network protocols, maps protocol type to MacOS handler address
static std::map<uint16, uint32> net_protocols;

// Use stringified form of MAC address on the JS side, so that it's easier to
// debug.
static void ether_addr_to_str(uint8 *ether_addr, char *ether_addr_str) {
  sprintf(ether_addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", ether_addr[0],
          ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4],
          ether_addr[5]);
}

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
        ReadMacInt16(ether_data + ed_RHA + 4),
        ReadMacInt32(ether_data + ed_RHA + 6),
        ReadMacInt16(ether_data + ed_RHA + 10),
        ReadMacInt16(ether_data + ed_RHA + 12)));

  // Call protocol handler
  M68kRegisters r;
  r.d[0] = type; // Packet type
  r.d[1] =
      length - 14; // Remaining packet length (without header, for ReadPacket)
  r.a[0] = p + 14; // Pointer to packet (Mac address, for ReadPacket)
  r.a[3] = ether_data + ed_RHA + 14; // Pointer behind header in RHA
  r.a[4] =
      ether_data + ed_ReadPacket; // Pointer to ReadPacket/ReadRest routines
  D(bug(" calling protocol handler %08x, type %08x, length %08x, data %08x, "
        "rha %08x, read_packet %08x\n",
        handler, r.d[0], r.d[1], r.a[0], r.a[3], r.a[4]));
  Execute68k(handler, &r);
}

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
  } else if (is_apple_talk_broadcast(packet) || is_ethernet_broadcast(packet)) {
    dest_ether_addr_str[0] = '*';
    dest_ether_addr_str[1] = '\0';
  } else {
    D(bug("Ignoring ether_write with unexpected address"));
    return eMultiErr;
  }

  EM_ASM_({ workerApi.etherWrite(UTF8ToString($0), $1, $2); },
          dest_ether_addr_str, packet, len);

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
    int length = EM_ASM_INT({ return workerApi.etherRead($0, 1514); },
                            Mac2HostAddr(packet));
    if (length < 14) {
      return;
    }

    ether_dispatch_packet(packet, length);
  }
}
