
#include "ap445.h"
#include "apcommon.h"

long rpnt445(struct sblk445 *c_blk, uint32_t point)
/* the I/O point of a port */
{

	if ((input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->io_port)) & (1 << point))
		return(1);
	else 
		return(0);
}

long rprt445(struct sblk445 *c_blk)
{
	return ( input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->io_port));
}

void rsts445(struct sblk445 *s_blk)
{

   long addr, index;

   s_blk->location = (word)input_byte(s_blk->nHandle, (byte*)&s_blk->brd_ptr->LocationRegister);/* AP location */

   /* read temp & VCC info from FPGA */
   for( addr = 0, index = 0; index < 3; index++, addr++)
   {
     output_long( s_blk->nHandle, (long*)&s_blk->brd_ptr->XDAC_AddressReg, (long)addr); /* FPGA[addr] */
     s_blk->FPGAAdrData[index] = input_long( s_blk->nHandle, (long*)&s_blk->brd_ptr->XDAC_StatusControl); /* addr & data [index] */
   }

   for( addr = 0x20, index = 3; index < 6; index++, addr++)
   {
     output_long( s_blk->nHandle, (long*)&s_blk->brd_ptr->XDAC_AddressReg, (long)addr); /* FPGA[addr] */
     s_blk->FPGAAdrData[index] = input_long( s_blk->nHandle, (long*)&s_blk->brd_ptr->XDAC_StatusControl); /* addr & data [index] */
   }

   for( addr = 0x24, index = 6; index < 9; index++, addr++)
   {
     output_long( s_blk->nHandle, (long*)&s_blk->brd_ptr->XDAC_AddressReg, (long)addr); /* FPGA[addr] */
     s_blk->FPGAAdrData[index] = input_long( s_blk->nHandle, (long*)&s_blk->brd_ptr->XDAC_StatusControl); /* addr & data [index] */
   }
}

long wcntl445(struct sblk445 *c_blk, uint32_t value)
/* value to write to control register */
{
    output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->ResetRegister, value);
    return(0);
}

long wprt445(struct sblk445 *c_blk, uint32_t value)
/* the output value */
{
	output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->io_port, value);
	return(0);
}
	
long wpnt445(struct sblk445 *c_blk, uint32_t point, uint32_t value)
/* the I/O point of a port */
/* the output value */
{
	uint32_t bpos;              /* bit position */

	bpos = 1 << point;
	value <<= point;
	output_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->io_port, 
		((input_long( c_blk->nHandle, (long*)&c_blk->brd_ptr->io_port ) & ~bpos) | value));
	return(0);
}
