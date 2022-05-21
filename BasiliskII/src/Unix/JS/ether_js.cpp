#include "sysdeps.h"

#include "cpu_emulation.h"
#include "ether.h"
#include "ether_defs.h"
#include "macos_util.h"

bool ether_init(void) {
  printf("ether_init\n");
  return true;
}

void ether_exit(void) { printf("ether_exit\n"); }

void ether_reset(void) { printf("ether_reset\n"); }

int16 ether_add_multicast(uint32 pb) {
  printf("ether_add_multicast\n");
  return noErr;
}

int16 ether_del_multicast(uint32 pb) {
  printf("ether_del_multicast\n");
  return noErr;
}

int16 ether_attach_ph(uint16 type, uint32 handler) {
  printf("ether_attach_ph\n");
  return noErr;
}

int16 ether_detach_ph(uint16 type) {
  printf("ether_detach_ph\n");
  return noErr;
}

int16 ether_write(uint32 wds) {
  printf("ether_write\n");
  // Set source address
  uint32 hdr = ReadMacInt32(wds + 2);
  memcpy(Mac2HostAddr(hdr + 6), ether_addr, 6);
  return noErr;
}

bool ether_start_udp_thread(int socket_fd) {
  printf("ether_start_udp_thread\n");
  return false;
}

void ether_stop_udp_thread(void) { printf("ether_stop_udp_thread\n"); }

void EtherInterrupt(void) { printf("EtherInterrupt\n"); }
