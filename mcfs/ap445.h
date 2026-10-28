
#include <stdint.h>
#include <sys/types.h>

typedef unsigned char BYTE;
#ifndef BOOL
typedef int BOOL;
#endif
typedef unsigned char byte;
typedef unsigned short word;
typedef short WORD;

#define AP445 (word)0x7014

struct map445
{
  uint32_t ResetRegister;
  uint32_t LocationRegister;
  uint32_t io_port;
  uint32_t unused2[15];
  uint32_t XDAC_StatusControl;
  uint32_t XDAC_AddressReg;
  unsigned char Reserved4[0x01B0];
  uint32_t FirmwareRevision;
};

struct sblk445
{
  struct map445 *brd_ptr;
  uint32_t FPGAAdrData[10];
  int nHandle;
  uint32_t revision;
  ushort location;
  BOOL bAP;
  BOOL bInitialized;
};

#ifdef __cplusplus
extern "C" {
#endif

void psts445(struct sblk445 *);
void rsts445(struct sblk445 *);
long rpnt445(struct sblk445 *, uint32_t point);
long rprt445(struct sblk445 *);
long wpnt445(struct sblk445 *, uint32_t point, uint32_t value);
long wprt445(struct sblk445 *, uint32_t value);
long wcntl445(struct sblk445 *, uint32_t value);

#ifdef __cplusplus
}
#endif
