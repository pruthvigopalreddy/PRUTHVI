
#include <stdint.h>
#include <sys/types.h>

typedef unsigned char BYTE;
#ifndef BOOL
typedef int BOOL;
#endif
typedef unsigned char byte;
typedef unsigned short word;
typedef short WORD;

#define AP323 (word)0x7017
#define FlashIDString "AP323"


#define SA_CHANS	40
#define SA_SIZE         100
#define AVE_SAMPLES	200


#define RANGE_0TO5	0
#define RANGE_5TO5	1
#define RANGE_10TO10	2
#define RANGE_0TO10	3

#define TC_SELECT	0
#define SB_SELECT	1

#define T_DISABLED	0
#define TI_SELECT	1
#define TO_SELECT	2

#define DI_SELECT	0
#define SE_SELECT	1
#define AZ_SELECT	2
#define CAL_SELECT	3

#define DISABLE		0
#define UN_CONT		1
#define UN_SNGL		2
#define BU_CONT		3
#define BU_SNGL		4
#define EX_TRIG		5

#define TIMER_OFF	0
#define TIMER_ON	1

#define INT_DIS		0
#define INT_AEC		1
#define INT_AGC		2
#define INT_RELEASE	0x8000

#define START_CONV		1
#define CLR_FIFO_SCN_LIST	2
#define CLR_SAMPLE_FIFO		4
#define CLR_SAMPLE_OVERFLAG	8
#define SL_FIFO_EMPTY		1
#define SL_FIFO_FULL		2
#define SAMPLE_FIFO_EMPTY	4
#define SAMPLE_FIFO_FULL	8
#define SAMPLE_FIFO_OVRFLOW	16

#define FlashCoefficientMemoryAddress	0x3FE000
#define FlashCoefficientIDString	0x3FEFF0


/*///////////////////////////// M25P10 definitions /////////////////////////////////////////*/
#define PageProgramM25P10     0x02
#define ReadM25P10            0x03
#define ReadStatusM25P10      0x05
#define SectorEraseM25P10     0x20
#define WrenM25P10            0x06

#define WIP                   0x01
#define FMAX_TRIES            250
/*///////////////////////////////////////////////////////////////////////////////////////////*/

struct map323
{
    uint32_t InterruptRegister;
    uint32_t LocationRegister;
    uint32_t cntl_reg;
    uint32_t tpc_reg;
    uint32_t ctc_reg;
    uint32_t scn_lst_FIFO;
    uint32_t scn_lst_cnt;
    uint32_t scn_status;
    uint32_t sampleFIFO;
    int sampleFIFOcount;
    uint32_t trigFIFOclear;
    unsigned char unused[0x34];
    uint32_t XDAC_StatusControl;
    uint32_t XDAC_AddressReg;
    unsigned char Reserved2[0x198];
    uint32_t FirmwareRevision;
    uint32_t FLASHData;
    uint32_t FlashChipSelect;
};

struct cblk323
{
    struct map323 *brd_ptr;
    uint32_t FPGAAdrData[10];
    int nHandle;
    BOOL bAP;
    BOOL bInitialized;
    int *s_cor_buf[SA_CHANS];
    unsigned short *s_raw_buf[SA_CHANS];
    unsigned short s_count[SA_CHANS];
    unsigned short s_az_buf;
    unsigned short s_cal_buf;
    byte *sa_start;
    byte *sa_end;
    char RefCalValues[4][8];
    unsigned char IDbuf[32];
    uint32_t conv_timer;
    uint32_t data_format;
    uint32_t trigger;
    uint32_t acq_mode;
    uint32_t scan_mode;
    uint32_t timer_ps;
    uint32_t timer_en;
    uint32_t int_mode;
    uint32_t range;
    uint32_t control;
    uint32_t scan_status;
    uint32_t scan_count;
    uint32_t FIFO_count;
    uint32_t location;
    uint32_t revision;
};

#ifdef __cplusplus
extern "C" {
#endif

void rstsAP323(struct cblk323 *c_blk);
void cnfgAP323(struct cblk323 *c_blk);
void setconfAP323(struct cblk323 *c_blk);
void readstatAP323(struct cblk323 *c_blk);
int mccdAP323(struct cblk323 *c_blk);
void calibrateAP323(struct cblk323 *c_blk, int mode);
void convertAP323(struct cblk323 *c_blk);
void move_dataAP323(struct cblk323 *c_blk);
int ReadCalCoeffs323(struct cblk323 *c_blk);
int ReadFlashID323(struct cblk323 *c_blk, unsigned char *p );

int I0_M25P10(struct cblk323 *c_blk, unsigned char *command_buf, unsigned char *response_buf, unsigned int size);
int ReadStatus_M25P10(struct cblk323 *c_blk );
int SectorErase_M25P10(struct cblk323 *c_blk );
int WriteFlashBlock(struct cblk323 *c_blk, uint32_t address, void *pdata, uint32_t length );
int ReadByte_M25P10(struct cblk323 *c_blk, unsigned long address, unsigned char *p );
int BlankCheckFlash( struct cblk323 *c_blk );

#ifdef __cplusplus
}
#endif



