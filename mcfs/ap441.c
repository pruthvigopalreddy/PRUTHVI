#include "apcommon.h"
#include "ap441.h"
#include <string.h>

void cnfg441(struct cblk441 *c_blk)
{

/* If the Software Reset Register parameter is specified write a 1 to the  Software Reset Register */
   if(c_blk->param & SW_RESET)
   {
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->SoftwareReset, 1);
   }

/* If the Debounce Control Registers are to be updated */
   if(c_blk->param & DEBCONTROL)
   {
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[0], c_blk->deb_control[0]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[1], c_blk->deb_control[1]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[2], c_blk->deb_control[2]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[3], c_blk->deb_control[3]);
   }

/* If the Debounce Duration Registers are to be updated */
   if(c_blk->param & DEBDURATION)
   {
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[0], c_blk->deb_duration[0]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[1], c_blk->deb_duration[1]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[2], c_blk->deb_duration[2]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[3], c_blk->deb_duration[3]);
   }

/* If the Event Polarity Registers are to be updated */
   if(c_blk->param & EV_POLARITY)
   {
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[0], c_blk->EventPolarity[0]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[1], c_blk->EventPolarity[1]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[2], c_blk->EventPolarity[2]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[3], c_blk->EventPolarity[3]);
   }

/* If the Event Type Registers are to be updated */
   if(c_blk->param & EV_TYPE)
   {
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[0], c_blk->EventType[0]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[1], c_blk->EventType[1]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[2], c_blk->EventType[2]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[3], c_blk->EventType[3]);
   }

/* If the Event Enable Registers are to be updated */
   if(c_blk->param & EV_ENABLE)
   {
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[0], c_blk->EventEnable[0]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[1], c_blk->EventEnable[1]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[2], c_blk->EventEnable[2]);
      output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[3], c_blk->EventEnable[3]);
   }
}

int AP441Blocking_io(int nHandle, struct AP441_isr_info *isr_info, long *addr, long value, long cmd)
{
    APDATA_STRUCT* pAP;	/*  local */
    unsigned long data[5];

    pAP = GetAP(nHandle);
    if(pAP == NULL)
	return(-1);

    /* place address to write to in data[0] */
    data[0] = (unsigned long) addr;

    /* place data value to write at above address in data[1] */
    data[1] = (unsigned long) value;

    /* place transfer size flag in data[2]... flag=0=byte write, flag=1=word write... */
    /*... flag=2=long write, flag=10=no write just wait for a input event to wake up */
    data[2] = (unsigned long)cmd;

    /* place board instance index in data[3] */
    data[3] = (unsigned long) pAP->nDevInstance;	/* Device Instance */

    data[4] = 0;

    /* pram3 = function: 8=blocking_start_convert */
    write( pAP->nAPDeviceHandle, &data[0], 8 );

    /* Parse data and place in structure */
    isr_info->Status = (uint32_t)data[0];              /* exit status */
    isr_info->EventPending[0] = (uint32_t)data[1];     /* individual channel interrupt cause info */
    isr_info->EventPending[1] = (uint32_t)data[2];
    isr_info->EventPending[2] = (uint32_t)data[3];
    isr_info->EventPending[3] = (uint32_t)data[4];
    return((int)0);
}

uint8_t rpnt441(struct cblk441 *c_blk, uint8_t port, uint8_t point)

{

    uint8_t temp;

   temp = (uint8_t)input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->IORegister[(port & 3)]);

   if(temp & (1 << (point & 7)))
      return((uint8_t)1);
   else 
      return((uint8_t)0);
}

uint8_t rprt441(struct cblk441 *c_blk, uint8_t port)

{

   return((uint8_t)input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->IORegister[(port & 3)]));
}

void rsts441(struct cblk441 *c_blk)

{

   long addr, index;

    c_blk->location = (word)input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->LocationRegister);/* AP location */
    c_blk->revision = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->FirmwareRevision);	 /* AP Revision */

    c_blk->sblk_ptr->BoardIntEnableStat = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->InterruptRegister);

    c_blk->sblk_ptr->DebounceDurationStat[0] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[0]);
    c_blk->sblk_ptr->DebounceDurationStat[1] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[1]);
    c_blk->sblk_ptr->DebounceDurationStat[2] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[2]);
    c_blk->sblk_ptr->DebounceDurationStat[3] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceDuration[3]);

    c_blk->sblk_ptr->DebounceControlStat[0] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[0]);
    c_blk->sblk_ptr->DebounceControlStat[1] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[1]);
    c_blk->sblk_ptr->DebounceControlStat[2] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[2]);
    c_blk->sblk_ptr->DebounceControlStat[3] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->DebounceControl[3]);

    c_blk->sblk_ptr->EventTypeStat[0] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[0]);
    c_blk->sblk_ptr->EventTypeStat[1] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[1]);
    c_blk->sblk_ptr->EventTypeStat[2] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[2]);
    c_blk->sblk_ptr->EventTypeStat[3] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventTypeReg[3]);

    c_blk->sblk_ptr->EventPolarityStat[0] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[0]);
    c_blk->sblk_ptr->EventPolarityStat[1] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[1]);
    c_blk->sblk_ptr->EventPolarityStat[2] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[2]);
    c_blk->sblk_ptr->EventPolarityStat[3] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPolarityReg[3]);

    c_blk->sblk_ptr->EventEnableStat[0] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[0]);
    c_blk->sblk_ptr->EventEnableStat[1] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[1]);
    c_blk->sblk_ptr->EventEnableStat[2] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[2]);
    c_blk->sblk_ptr->EventEnableStat[3] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventEnableReg[3]);

    c_blk->sblk_ptr->EventPendingClrStat[0] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPendingClrReg[0]);
    c_blk->sblk_ptr->EventPendingClrStat[1] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPendingClrReg[1]);
    c_blk->sblk_ptr->EventPendingClrStat[2] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPendingClrReg[2]);
    c_blk->sblk_ptr->EventPendingClrStat[3] = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->EventPendingClrReg[3]);

   /* read temp & VCC info from FPGA */
   for( addr = 0, index = 0; index < 3; index++, addr++)
   {
     output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->XDAC_AddressReg, (long)addr); /* FPGA[addr] */
     c_blk->FPGAAdrData[index] = input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->XDAC_StatusControl); /* addr & data [index] */
   }

   for( addr = 0x20, index = 3; index < 6; index++, addr++)
   {
     output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->XDAC_AddressReg, (long)addr); /* FPGA[addr] */
     c_blk->FPGAAdrData[index] = input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->XDAC_StatusControl); /* addr & data [index] */
   }

   for( addr = 0x24, index = 6; index < 9; index++, addr++)
   {
     output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->XDAC_AddressReg, (long)addr); /* FPGA[addr] */
     c_blk->FPGAAdrData[index] = input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->XDAC_StatusControl); /* addr & data [index] */
   }
}

