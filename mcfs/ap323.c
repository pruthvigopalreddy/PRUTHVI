
#include "ap323.h"
#include <math.h>
#include "apcommon.h"
#include <stdio.h>
#include <string.h>

void calibrateAP323(struct cblk323 *c_blk, int mode)
{
    struct cblk323 temp_blk;
    byte temp_scn_data;
    double f_cor;
    int i;
    unsigned short *data;

    //memcpy( &temp_blk, c_blk, sizeof( temp_blk ) );

    temp_blk = *c_blk;

    if(mode == AZ_SELECT)
      data = (unsigned short*)&c_blk->s_az_buf;
    else
      data = (unsigned short*)&c_blk->s_cal_buf;

    temp_blk.int_mode = INT_DIS;
    temp_blk.acq_mode = mode;
    temp_blk.scan_mode = BU_SNGL;
    temp_blk.timer_en = TIMER_ON;
    temp_scn_data = 0;
    temp_blk.sa_start = temp_blk.sa_end = &temp_scn_data;

    cnfgAP323(&temp_blk);

    for(f_cor = 0.0, i = 0; i < AVE_SAMPLES; i++)
    {
        output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->trigFIFOclear, CLR_SAMPLE_FIFO );
        output_long(temp_blk.nHandle, (long*)&temp_blk.brd_ptr->trigFIFOclear, START_CONV);

        while(input_long(temp_blk.nHandle, (long*)&temp_blk.brd_ptr->sampleFIFOcount) == 0);

    usleep(1000);

 	f_cor += (short)input_long(temp_blk.nHandle, (long*)&temp_blk.brd_ptr->sampleFIFO);
    }
 
    f_cor /= (double)i;

    f_cor += (f_cor < 0.0) ? -0.5 : 0.5;

    *data = (unsigned short)f_cor;
}

void convertAP323(struct cblk323 *c_blk)
{
    uint32_t target_count;
    cnfgAP323(c_blk);

    if(c_blk->scan_mode == DISABLE)
      return;

    if(c_blk->int_mode == INT_DIS)
    {
      target_count = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->scn_lst_cnt);

      if(c_blk->trigger == TO_SELECT || c_blk->scan_mode == EX_TRIG)
         output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->trigFIFOclear, START_CONV);

      while((uint32_t)input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->sampleFIFOcount) < target_count);
    }
    else
    {
//printf("blocking_start_convert wait...");
fflush(stdout);

      if(c_blk->trigger == TO_SELECT || c_blk->scan_mode == EX_TRIG)
         APBlockingStartConvert(c_blk->nHandle, (long*)&c_blk->brd_ptr->trigFIFOclear, (long)START_CONV, (long) 2);

//printf(" done!\n");

    }
    move_dataAP323(c_blk);
}

void move_dataAP323(struct cblk323 *c_blk)

{
    int i, j, sample_count;
    uint32_t temp_data;

    for (int i = 0; i < 40; i++)
    {
       c_blk->s_count[i]=0;
    }

    sample_count = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->sampleFIFOcount);

    if( sample_count > SA_SIZE )
	sample_count = SA_SIZE;

    for( j = 0; j < sample_count; j++)
    {
    temp_data = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->sampleFIFO );

        i = (int)(( temp_data & 0x002F0000 ) >> 16);
        
        if(c_blk->s_count[i] >= SA_SIZE )
	     c_blk->s_count[i] = 0;

        c_blk->s_raw_buf[i][c_blk->s_count[i]++] = (unsigned short)temp_data;
     }
}

static uint32_t build_control(struct cblk323 *c_blk)
{

    uint32_t control;

    control = DISABLE;

    if(c_blk->data_format == SB_SELECT)
       control |= 1;

    if(c_blk->trigger == TI_SELECT)
       control |= 2;

    if(c_blk->trigger == TO_SELECT)
       control |= 4;

    switch(c_blk->acq_mode)
    {
      case SE_SELECT:
     control |= (1 << 3);
      break;

      case AZ_SELECT:
	 switch(c_blk->range)
	 {
       case RANGE_0TO5:
       case RANGE_0TO10:
          control |= (6 << 3);
	   break;

	   default:
        control |= (7 << 3);
	   break;
	 }
      break;

      case CAL_SELECT:
	 switch(c_blk->range)
	 {
       case RANGE_5TO5:
       case RANGE_0TO5:
           control |= (4 << 3);
	   break;

       case RANGE_10TO10:
       case RANGE_0TO10:
           control |= (3 << 3);
	   break;
	 }
      break;

      default:
      break;
    }

    control |= (c_blk->scan_mode << 8);

    if(c_blk->timer_en == TIMER_ON)
       control |= 0x0800;

    control |= (c_blk->int_mode << 12);

    return(control);
}

void cnfgAP323(struct cblk323 *c_blk)
{
    byte *wsa_start;
    int elements;

    output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->cntl_reg, DISABLE | 0x38);

    output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->cntl_reg, build_control(c_blk));

    output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->trigFIFOclear, CLR_FIFO_SCN_LIST | CLR_SAMPLE_FIFO | CLR_SAMPLE_OVERFLAG );

    wsa_start = c_blk->sa_start;

    elements = (c_blk->sa_end - c_blk->sa_start) + 1;
    if(elements > 1024)
       elements = 1024;
 
    for(; elements; elements--, wsa_start++)
      output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->scn_lst_FIFO, *wsa_start);

    output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->tpc_reg, c_blk->timer_ps);
    output_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->ctc_reg, c_blk->conv_timer);
    usleep(500);
}

int mccdAP323(struct cblk323 *c_blk)
{

    int i, j;
    int *cp;
    int UnipolarFlag;
    unsigned short *rp;
    union
    {
      short s;
      unsigned short u;
    }w;

    double temp, seed, seed1;
    double i_span;
    double i_zero;
    double calhi;
    double callo;
    double slope;
    double az, as;
    char *endptr;

    if( c_blk->s_cal_buf - c_blk->s_az_buf == 0 )
        return(-1);

    switch(c_blk->range)
    {
	case RANGE_0TO5:
        callo = strtod(c_blk->RefCalValues[3], &endptr);
        calhi = strtod(c_blk->RefCalValues[1], &endptr);
        i_zero = 0.0000;
        i_span = 5.0000;
        UnipolarFlag = 1;
	break;

	case RANGE_5TO5:
        calhi = strtod(c_blk->RefCalValues[1], &endptr);
        callo = 0.0;
        i_zero = -5.0000;
        i_span = 10.0000;
        UnipolarFlag = 0;
	break;

	case RANGE_0TO10:
        callo = strtod(c_blk->RefCalValues[3], &endptr);
        calhi = strtod(c_blk->RefCalValues[0], &endptr);
        i_zero =  0.0;
        i_span = 10.0000;
        UnipolarFlag = 1;
	break;

    default:
        calhi = strtod(c_blk->RefCalValues[0], &endptr);
        callo = 0.0;
        i_zero = -10.0000;
        i_span = 20.0000;
        UnipolarFlag = 0;
	break;
    }

    if(( c_blk->data_format == TC_SELECT && UnipolarFlag == 1) || ( c_blk->data_format == SB_SELECT && UnipolarFlag == 0 ))
    {
      az = (double) (c_blk->s_az_buf  ^ 0x8000);
      as = (double) (c_blk->s_cal_buf ^ 0x8000);
    }
    else
    {
      az = (double) c_blk->s_az_buf;
      as = (double) c_blk->s_cal_buf;
    }

    slope = ( calhi - callo ) / ( as - az );
    seed = (double)65536.0 * slope / i_span;
    seed1 = (callo - i_zero) / slope - az;

    for( j = 0; j < SA_CHANS; j++)
    {
      rp = c_blk->s_raw_buf[j];
      cp = c_blk->s_cor_buf[j];

      for( i = 0; i < SA_SIZE; i++ )
      {

        if(( c_blk->data_format == TC_SELECT && UnipolarFlag == 1) || ( c_blk->data_format == SB_SELECT && UnipolarFlag == 0 ))
           w.u = (rp[i] ^ 0x8000);
        else
           w.u = rp[i];

        if( UnipolarFlag == 1)
            temp = seed * ((double)w.u + seed1);
        else
            temp = seed * ((double)w.s + seed1);

        temp += (temp < 0.0) ? -0.5 : 0.5;
        temp = fmin(temp, 65535.0);
        temp = fmax(temp, 0.0);
        cp[i] = (int)temp;
     }
    }
    return(0);
}

void rstsAP323(struct cblk323 *c_blk)
{
    long addr, index;

   c_blk->control = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->cntl_reg);
   c_blk->location = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->LocationRegister);
   c_blk->revision = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->FirmwareRevision);
   c_blk->scan_status = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->scn_status);
   c_blk->FIFO_count = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->sampleFIFOcount);
   c_blk->scan_count = input_long(c_blk->nHandle, (long*)&c_blk->brd_ptr->scn_lst_cnt);

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

int ReadStatus_M25P10(struct cblk323 *c_blk )
{
    unsigned char cmd_buf[2];
    unsigned char rsp_buf[2];

    for (int i = 0; i < 2; i++)
    {
       cmd_buf[i] = 0;
       rsp_buf[i]=0;
    }

    cmd_buf[1] = 0;
    cmd_buf[0] = ReadStatusM25P10;		/* read status command */

    I0_M25P10(c_blk, &cmd_buf[0], &rsp_buf[0], sizeof(rsp_buf));/* Issue command */

    /* recover the response */
    return(( int)rsp_buf[1]);
}

int SectorErase_M25P10(struct cblk323 *c_blk )
{
    unsigned char cmd_buf[4];
    unsigned char rsp_buf[4];
    int i;
    int status = 0;
    unsigned long address;

    /* Send the WREN command (write enable) */
    cmd_buf[0] = WrenM25P10;			/* write enable command */
    I0_M25P10(c_blk, &cmd_buf[0], &rsp_buf[0], 1);	/* Issue command */

    address = FlashCoefficientMemoryAddress;
    cmd_buf[0] = SectorEraseM25P10;			/* sector erase command */
    cmd_buf[1] = (unsigned char)(address >> 16);	/* form A23-A16 address byte */
    cmd_buf[2] = (unsigned char)(address >> 8);	/* form A15-A8 address byte */
    cmd_buf[3] = (unsigned char)(address);		/* form lower address byte */

    I0_M25P10(c_blk, &cmd_buf[0], &rsp_buf[0], sizeof(cmd_buf));/* Issue command */
	
    /* Poll write in progress bit until it reads as a logic low */
    for(i = 0; i < FMAX_TRIES; i++ )
    {
      usleep(1000);		/* Linux */
      status = ReadStatus_M25P10(c_blk );
      if( (status & WIP) == 0 )  /* zero upon write complete */
        break;
    }

    if( i >= FMAX_TRIES )
       return((int) -1);         	/* write error */

    return((int)0);
}

int BlankCheckFlash( struct cblk323 *c_blk )
{

unsigned char verify_data, read_data;
uint32_t j,k,length;

 length = 4096;	/* get length of data array in bytes */

 for(k = 0, j = 0; j < length; j++ )
 {
   ReadByte_M25P10(c_blk, FlashCoefficientMemoryAddress + j, &read_data );
 
   verify_data = 0xFF;
   if( verify_data != read_data )
   {
    k++;
    //printf("ErrCnt=%06d Adr=%06X Expected=%02X Read=%02X\n",k, FlashCoefficientMemoryAddress + j,verify_data,read_data);
   }
 }

return((int)k);		/* return error count */
}

static int Write_FLASH(struct cblk323 *c_blk, unsigned char *command_buf, unsigned char *response_buf, unsigned int size)
{
#ifdef DBG_SPI
unsigned char *cmd_buf = command_buf;
unsigned char *rsp_buf = response_buf;
#endif /* DBG_SPI */

    unsigned int sz;

    /* transfer data over the FLASH port */
    for( sz = 1; sz <= size; sz++)
    {
      output_byte(c_blk->nHandle, (byte*)&c_blk->brd_ptr->FLASHData, (byte)*command_buf++ );   /* LSByte */
      *response_buf++ = (unsigned char) input_byte(c_blk->nHandle, (byte*)&c_blk->brd_ptr->FLASHData);
    }

#if 0
int i;
printf("\n");
for(i = 0; i < size; i++)
printf("%02X ",cmd_buf[i]);
printf("\n");
for(i = 0; i < size; i++)
printf("%02X ",rsp_buf[i]);
#endif /* DBG_SPI */

    return(0);
}

int I0_M25P10(struct cblk323 *c_blk, unsigned char *command_buf, unsigned char *response_buf, unsigned int size)
{
    /* drive the chip select active for the M25P10 device */
    output_byte(c_blk->nHandle, (byte*)&c_blk->brd_ptr->FlashChipSelect, 0 );
    
    Write_FLASH( c_blk, command_buf, response_buf, size);

    /* drive the chip select inactive for the M25P10 device */
    output_byte(c_blk->nHandle, (byte*)&c_blk->brd_ptr->FlashChipSelect, 1 );
    return(0);
}

int ReadByte_M25P10(struct cblk323 *c_blk, unsigned long address, unsigned char *p )
{
    unsigned char cmd_buf[5];
    unsigned char rsp_buf[5];
    int status = 0;

    for (int i = 0; i < 5; i++)
    {
       cmd_buf[i] = 0;
       rsp_buf[i]=0;
    }

    cmd_buf[0] = ReadM25P10;				/* read command */
    cmd_buf[1] = (unsigned char)(address >> 16);	/* form A23-A16 address byte */
    cmd_buf[2] = (unsigned char)(address >> 8);		/* form A15-A8 address byte */
    cmd_buf[3] = (unsigned char)(address);		/* form lower address byte */

    I0_M25P10(c_blk, &cmd_buf[0], &rsp_buf[0], sizeof(rsp_buf));/* Issue command */

    /* recover the response */
    if(p)		 /* if pointer is non-zero update */
    *p = rsp_buf[4]; /* update callers data */
    else
    status = -1;	 /* error */

    return(status);
}

int WriteFlashBlock(struct cblk323 *c_blk, uint32_t address, void *pdata, uint32_t length )
{
	unsigned char cmd_buf[264];
	unsigned char rsp_buf[264];			/* worst case length */
	int i;
	int status = 0;

	if(length > 256)
	   return((int) -1);				/* error */

	/* Send the WREN command (write enable) */
	cmd_buf[0] = WrenM25P10;			/* write enable command */
	I0_M25P10(c_blk, &cmd_buf[0], &rsp_buf[0], 1);	/* Issue command */
	
	/* Build up the write command */
	cmd_buf[0] = PageProgramM25P10;			/* Page Program command */	
	cmd_buf[1] = (unsigned char)(address >> 16);	/* form A23-A16 address byte */
	cmd_buf[2] = (unsigned char)(address >> 8);	/* form A15-A8 address byte */
	cmd_buf[3] = (unsigned char)(address);		/* form lower address byte */

        //memcpy(&cmd_buf[4], pdata, length);		/* copy the byte data into local buffer */

        for (uint32_t i = 0; i < length; i++)
        {
            cmd_buf[4 + i] = ((unsigned char *)pdata)[i];
        }

	I0_M25P10(c_blk, &cmd_buf[0], &rsp_buf[0], (length + 4));/* Issue command */

	/* Poll write in progress bit until it reads as a logic low.*/
	for(i = 0; i < FMAX_TRIES; i++ )
	{
      usleep(1000);
	  status = ReadStatus_M25P10(c_blk );
	  if( (status & WIP) == 0 )  /* zero upon write complete */
	    break;	
	}

	if( i >= FMAX_TRIES )
	   return((int) -2);         	/* write error */

	return((int)0);
}

int ReadFlashID323(struct cblk323 *c_blk, unsigned char *p )
{
    unsigned char cmd_buf[10];
    unsigned char rsp_buf[10];
    unsigned long address;
    int status = 0;

    for (int i = 0; i < 10; i++)
    {
       cmd_buf[i] = 0;
       rsp_buf[i]=0;
    }

    address = FlashCoefficientIDString;
    cmd_buf[0] = ReadM25P10;			/* read command */
    cmd_buf[1] = (unsigned char)(address >> 16);/* form A23-A16 address byte */
    cmd_buf[2] = (unsigned char)(address >> 8);	/* form A15-A8 address byte */
    cmd_buf[3] = (unsigned char)(address);	/* form lower address byte */

    I0_M25P10(c_blk, &cmd_buf[0], &rsp_buf[0], sizeof(rsp_buf));/* Issue command */

    /* recover the response */
    *p++ = rsp_buf[4];
    *p++ = rsp_buf[5];
    *p++ = rsp_buf[6];
    *p++ = rsp_buf[7];
    *p++ = rsp_buf[8];
    *p = rsp_buf[9];

    return(status);
}

int ReadCalCoeffs323(struct cblk323 *c_blk)
{

uint32_t calv, j, calb;
int status;

 /* 4 reference cal values 0=9.88V, 1=4.94V, 2=2.47V, and 3=1.235V */
 for( calv = 0; calv < 4; calv++ )
 {
    j = (FlashCoefficientMemoryAddress + (calv * 8));/* Flash memory addressing */

    for( calb = 0; calb < 8; calb++ )		/* 8 bytes for each calibration value */
    {
       /* read bytes */
       status = ReadByte_M25P10(c_blk, (j+calb), ((unsigned char *)&c_blk->RefCalValues[calv][calb]));
       if( status )
	   return(status);
    }
 }
 return(0);
}

int WriteCalCoeffs323(struct cblk323 *c_blk)
{
   unsigned char ogc_buf[256];
   int status;

   status = SectorErase_M25P10( c_blk );
   if(status )		/* errors */
      return( status );	/* flash error */

   status = BlankCheckFlash( c_blk );
   if(status )		/* errors */
      return( status );	/* flash error */

    /* set buffer to 0xFF */
    for (size_t i = 0; i < sizeof(ogc_buf); i++)
    {
      ogc_buf[i] = 0xFF;
    }

   memcpy(&ogc_buf[0],&c_blk->RefCalValues[0][0],32);	/* copy ASCII cal values to buffer */
    // for (size_t i = 0; i < 32; i++)
    // {
    //   ogc_buf[i] = c_blk->RefCalValues[0][i];
    // }

   /* write calibration vlaues to Flash - 64 bytes */
   status = WriteFlashBlock(c_blk, FlashCoefficientMemoryAddress, &ogc_buf[0], 64);

   printf("%d",status);

   /* write the ID string to flash */
   //memset(&ogc_buf[0],0xFF,sizeof(ogc_buf));	/* set buffer to 0xFF */

  for (size_t i = 0; i < sizeof(ogc_buf); i++)
  {
      ogc_buf[i] = 0xFF;
  }

   /* Add the model ID string */
   //strcpy((char*)&ogc_buf[0], (const char *)FlashIDString);

    size_t i = 0;
    while (i < sizeof(ogc_buf) - 1 && FlashIDString[i] != '\0') {
      ogc_buf[i] = FlashIDString[i];
      i++;
    }
ogc_buf[i] = '\0';  // Null-terminate the buffer

   /* write model ID string to Flash - 64 bytes */
   status = WriteFlashBlock(c_blk, FlashCoefficientIDString, &ogc_buf[0], 64);

   return( status );
}

