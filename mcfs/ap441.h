
#include <stdint.h>
#include <sys/types.h>

typedef unsigned char BYTE;
#ifndef BOOL
typedef int BOOL;
#endif
typedef unsigned char byte;
typedef unsigned short word;
typedef short WORD;

#define AP441_1	1

#ifdef AP441_1
#define APBOARD (word)0x7031
#endif

#define EV_ENABLE      1
#define EV_TYPE        2
#define EV_POLARITY    4
#define DEBCONTROL     8
#define DEBDURATION    0x10
#define SW_RESET       0x20


struct sblk441
{
  uint32_t EventEnableStat[4];
  uint32_t EventTypeStat[4];
  uint32_t EventPolarityStat[4];
  uint32_t EventPendingClrStat[4];
  uint32_t DebounceControlStat[4];
  uint32_t DebounceDurationStat[4];
  uint32_t BoardIntEnableStat;
};

struct AP441_isr_info
{
  uint32_t Status;
  uint32_t EventPending[4];
};


struct map441
{
  uint32_t InterruptRegister;
  uint32_t LocationRegister;
  uint32_t IORegister[4];
  uint32_t EventEnableReg[4];
  uint32_t EventTypeReg[4];
  uint32_t EventPolarityReg[4];
  uint32_t EventPendingClrReg[4];
  uint32_t DebounceControl[4];
  uint32_t DebounceDuration[4];
  uint32_t SoftwareReset;
  uint32_t XDAC_StatusControl;
  uint32_t XDAC_AddressReg;
  unsigned char Reserved1[0x017C];
  uint32_t FirmwareRevision;
};

struct cblk441
{
  struct map441 *brd_ptr;
  struct sblk441 *sblk_ptr;
  uint32_t FPGAAdrData[10];
  int nHandle;
  uint16_t location;
  uint16_t param;
  BOOL bAP;
  BOOL bInitialized;
  uint32_t revision;
  uint32_t EventEnable[4];
  uint32_t EventType[4];
  uint32_t EventPolarity[4];
  uint32_t deb_control[4];
  uint32_t deb_duration[4];
};

#ifdef __cplusplus
extern "C" {
#endif

void scfg441(struct cblk441 *c_blk);
void psts441(struct cblk441 *c_blk);
void cnfg441(struct cblk441 *c_blk);
void rsts441(struct cblk441 *c_blk);
uint8_t rpnt441(struct cblk441 *c_blk, uint8_t port, uint8_t point);
uint8_t rprt441(struct cblk441 *c_blk, uint8_t port);
int AP441Blocking_io(int nHandle, struct AP441_isr_info *isr_info, long *addr, long value, long cmd);

#ifdef __cplusplus
}
#endif

