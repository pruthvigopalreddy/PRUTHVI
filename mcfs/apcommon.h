
#define VENDOR_ID (word)0x16D5
#define MAX_APS 15

#ifndef BUILDING_FOR_KERNEL
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include<pthread.h>
#include <stdlib.h>
#include <stdint.h>
#endif

typedef unsigned char BYTE;
#ifndef BOOL
typedef int BOOL;
#endif

typedef unsigned char byte;
typedef unsigned short word;
typedef short WORD;

typedef int APSTATUS;

#define TRUE	1
#define FALSE	0

#define AP_RESET	0x8000
#define AP_INT_ENABLE	0x0001
#define AP_INT_PENDING	0x0002

#define ERROR			0x8000
#define E_OUT_OF_MEMORY 	0x8001
#define E_OUT_OF_APS	   	0x8002
#define E_INVALID_HANDLE	0x8003
#define E_NOT_INITIALIZED	0x8006
#define E_NOT_IMPLEMENTED	0x8007
#define E_NO_INTERRUPTS 	0x8008
#define S_OK			0x0000

typedef struct
{
    int nHandle;
    int nAPDeviceHandle;
    long lBaseAddress;
    int nDevInstance;
    int nInteruptID;
    int nIntLevel;
    char devname[64];
    BOOL bInitialized;
    BOOL bIntEnabled;
}APDATA_STRUCT;

typedef struct
{
    uint32_t InterruptRegister;
}AP_BOARD_MEMORY_MAP;

#ifdef __cplusplus
extern "C" {
#endif

APSTATUS GetAPAddress(int nHandle, long* pAddress);
APSTATUS SetAPAddress(int nHandle, long lAddress);
APSTATUS EnableAPInterrupts(int nHandle);
APSTATUS DisableAPInterrupts(int nHandle);
APSTATUS InitAPLib(void);
APSTATUS APOpen(int nDevInstance, int* pHandle, const char *devname);
APSTATUS APClose(int nHandle);
APSTATUS APInitialize(int nHandle);

word SwapBytes( word v );
long SwapLong( long v );

void AddAP(APDATA_STRUCT* pAP);
void DeleteAP(int nHandle);
APDATA_STRUCT* GetAP(int nHandle);
byte input_byte(int nHandle, byte*);
word input_word(int nHandle, word*);
void output_byte(int nHandle, byte*, byte);
void output_word(int nHandle, word*, word);
long input_long(int nHandle, long*);
void output_long(int nHandle, long*, long);
uint32_t APBlockingStartConvert(int nHandle, long *p, long v, long parameter);
void APTerminateBlockedStart(int nHandle);
long get_param(void);

#ifdef __cplusplus
}
#endif
