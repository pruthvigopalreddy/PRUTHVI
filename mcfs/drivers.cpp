#include "drivers.h"

Drivers::Drivers(QObject *parent)
    : QObject{parent}
{

}

//AP323
double Drivers::analogIN(int ap_instance, int channel)
{
    long addr;			/* holds board address */
    int i, j;                   /* loop index */
    double s;                   /* span value */
    double z;                   /* zero value */
    struct cblk323 c_block323;  /* configuration block */
    int elements = 100;

    int cor_data[SA_CHANS][SA_SIZE];      /* allocate  corrected data storage area */
    unsigned short raw_data[SA_CHANS][SA_SIZE];    /* allocate raw data storage area */
    byte s_array[100];

    memset(&c_block323, 0, sizeof(c_block323));	/*  Initialize the Configuration Parameter Block */
    memset(s_array, 0, sizeof(s_array));	/* clear s_array */

    memset(cor_data, 0, sizeof(cor_data));	/* clear corrected sample buffer */
    memset(raw_data, 0, sizeof(raw_data));	/* clear raw sample buffer */

    for(j = 0; j < SA_CHANS; j++)
    {
        c_block323.s_cor_buf[j] = &cor_data[j][0];   /* corrected buffer start for each channel */
        c_block323.s_raw_buf[j] = &raw_data[j][0];   /* raw buffer start for each channel */
    }

    //set config params

    c_block323.range = RANGE_10TO10;		/* default +-10 to 10 V */
    c_block323.acq_mode = DI_SELECT;		/* mode */
    c_block323.scan_mode = UN_CONT;		/* scan mode */
    c_block323.data_format = TC_SELECT;		/* A/D data format */
    c_block323.timer_ps = 0xff;			/* prescaler */
    c_block323.conv_timer = 0x55;		/* counter */
    c_block323.timer_en = TIMER_ON;		/* timer on */
    c_block323.trigger = TO_SELECT;		/* trigger I/O is output */
    c_block323.int_mode = INT_DIS;		/* disable interrupt mode */
    c_block323.control = 0;			/* control register used by read only*/
    c_block323.sa_start = &s_array[0];		/* address of start of scan array */
    c_block323.sa_end = &s_array[0];		/* address of end of scan array */
    c_block323.bAP = FALSE;			/* indicate not initialized and set up yet */
    c_block323.bInitialized = FALSE;		/* indicate not ready */
    c_block323.nHandle = 0;			/* make handle to a closed board */

    /*
    Initialize the AP library
    */
    if(InitAPLib() != S_OK)
    {
        printf("\nUnable to initialize the AP library. Exiting program.\n");
        //APClose(c_block323.nHandle);
        return 99;
    }

    /*
    Open an instance of a AP device
    */
    #define DEVICE_NAME_AP323 "ap323_"	/* the name of the device */

    if(APOpen(ap_instance, &c_block323.nHandle, DEVICE_NAME_AP323 ) != S_OK)
    {
        printf("\nUnable to Open instance of %s%i.\n", DEVICE_NAME_AP323,ap_instance);
        APClose(c_block323.nHandle);
        return 98;
    }
    else
    {
        for(i = 0; i < elements; i++)
        {
            *(c_block323.sa_start+i) = channel;
        }
        c_block323.sa_end = c_block323.sa_start + (elements - 1);

        if(APInitialize(c_block323.nHandle) == S_OK)/* Initialize */
        {
            GetAPAddress(c_block323.nHandle, &addr);	/* Read back address */
            c_block323.brd_ptr = (struct map323 *)addr;
            c_block323.bInitialized = TRUE;
            c_block323.bAP = TRUE;
        }
        memset(&c_block323.IDbuf[0],0,sizeof(c_block323.IDbuf));	/* empty the buffer */
        ReadFlashID323(&c_block323, &c_block323.IDbuf[0]);

        /* Install ideal values for each reference value */
        /* Reference Values 0=9.88V, 1=4.94V, 2=2.47V, and 3=1.235V */
        strncpy(&c_block323.RefCalValues[0][0], "9.88", sizeof(c_block323.RefCalValues[0]) - 1);
        c_block323.RefCalValues[0][sizeof(c_block323.RefCalValues[0]) - 1] = '\0';

        strncpy(c_block323.RefCalValues[1], "4.94", sizeof(c_block323.RefCalValues[1]) - 1);
        c_block323.RefCalValues[1][sizeof(c_block323.RefCalValues[1]) - 1] = '\0';

        strncpy(c_block323.RefCalValues[2], "2.47", sizeof(c_block323.RefCalValues[2]) - 1);
        c_block323.RefCalValues[2][sizeof(c_block323.RefCalValues[2]) - 1] = '\0';

        strncpy(c_block323.RefCalValues[3], "1.235", sizeof(c_block323.RefCalValues[3]) - 1);
        c_block323.RefCalValues[3][sizeof(c_block323.RefCalValues[3]) - 1] = '\0';


        /* If the AP323 flash ID is found and confirmed */
        if( (strstr( (const char *)&c_block323.IDbuf[0], (const char *)FlashIDString ) == NULL) )	/* AP323 ID */
        {
            printf("\nUnable to read APBoard FLASH ID.\n");
            APClose(c_block323.nHandle);
            return 97;
        }
        else
        {
            /* Read the factory reference values from flash and overwrite the previously installed ideal values */
            if(ReadCalCoeffs323(&c_block323 ) != 0) /* read from flash into structure */
            {
                printf("\nUnable to read calibration values from Flash memory.\n");
                APClose(c_block323.nHandle);
                return 96;
            }
        }
        /* read auto zero, calibration data, raw data values and convert to voltage*/
        if(!c_block323.bInitialized)
        {
            APClose(c_block323.nHandle);
            return 95;
        }

        calibrateAP323(&c_block323, AZ_SELECT);   /* get auto-zero values */
        usleep(2000);
	
        calibrateAP323(&c_block323, CAL_SELECT);  /* get calibration values */
        usleep(2000);
	
        convertAP323(&c_block323);                /* convert the board */
        usleep(2000);

        mccdAP323(&c_block323);       /* correct input data */
        usleep(2000);

        //channel &= 0x2f;     /* limit range */
        z = -10.0000;        /* RANGE_10TO10 */
        s =  20.0000;
        
        for(i = 0; i < elements; i++)
        {
            *(c_block323.sa_start+i) = channel;
            //printf("Channel assigned for Array Index %1X:%2X\n", i, *(c_block323.sa_start+i));
        }
        
        usleep(2000);
        
        //read values
        double sum = 0.0;

        // Read values and compute the average
        for (int i = 0; i < SA_SIZE; i++)
        {
            //usleep(2000);
            double value = ((((double)c_block323.s_cor_buf[channel][i]) * s) / 65536.0) + z;
            //printf("%12.6f ", value);
            sum += value;  // Accumulate the sum
        }

        double average = sum / SA_SIZE;
        //printf("\n Average Voltage reading: %12.6f\n", average);  // Print average

        //clear buffers
        memset(cor_data, 0, sizeof(cor_data));	/* clear corrected sample buffer */
        memset(raw_data, 0, sizeof(raw_data));	/* clear raw sample buffer */

        for(j = 0; j < SA_CHANS; j++)
        {
            c_block323.s_cor_buf[j] = &cor_data[j][0];   /* corrected buffer start for each channel */
            c_block323.s_raw_buf[j] = &raw_data[j][0];   /* raw buffer start for each channel */
        }
        APClose(c_block323.nHandle);
        return average;
    }
}

//AP441
double Drivers::optoDI(int ap_instance, int port, int point, int mode)
{
    long addr; // For address storage
    long status=-1; // For status storage

    cblk441 c_blk;
    sblk441 s_blk;

    memset(&c_blk, 0, sizeof(cblk441));
    memset(&s_blk, 0, sizeof(sblk441));

    c_blk.bAP = FALSE; // Indicate no AP initialized
    c_blk.bInitialized = FALSE; // Indicate not ready to talk to AP
    c_blk.sblk_ptr = (struct sblk441*)&s_blk; // Set address of status structure

    if (InitAPLib() != S_OK) {
        printf("\nUnable to initialize the AP library. Exiting program.\n");
        return 99;
    }

    #define DEVICE_NAME_AP441 "ap441_"       /* name of device */

    if (APOpen(ap_instance, &c_blk.nHandle, DEVICE_NAME_AP441) != S_OK) {
        printf("\nUnable to Open instance of %s%i.\n", DEVICE_NAME_AP441,ap_instance);
        return 98;
    }

    if (APInitialize(c_blk.nHandle) == S_OK)
    { // Initialize
        GetAPAddress(c_blk.nHandle, &addr); // Read back address
        c_blk.brd_ptr = (struct map441 *)addr;
        c_blk.bInitialized = TRUE;
        c_blk.bAP = TRUE; // Set AP initialized flag

        switch (mode)
        {
        case 1: // Read Digital Input Point
            if (!c_blk.bInitialized)
            {
                printf("Board Setup Failed. Exiting program.\n");
                return 96;
            }
            else
            {
                //Input port number   (0 - 3)
                //Input point number  (7 - 0)
                status = (long)rpnt441(&c_blk,(uint8_t)port, (uint8_t)point);
                //printf("\nValue of port %d point %d: %lx\n", port, point, status);
            }
            break;

        case 2: // Read Digital Input Port
            if (!c_blk.bInitialized)
            {
                printf("Board Setup Failed. Exiting program.\n");
                return 96;
            }
            else
            {
                //Input port number  (0 - 3)
                status = (long)rprt441(&c_blk,(uint8_t)port);
                //printf("\nValue of port %d: %lX\n", port, status);
            }
            break;
        } // end of switch
    }
    else
    {
        printf("APInitialize Failed. Exiting program.\n");
        return 97;
    }

    APClose(c_blk.nHandle); // Close the AP instance

    return status;
}

//AP445
double Drivers::optoDO(int ap_instance, int point, int value, long mode)
{
    long addr; // For address storage
    long status=-1; // For status storage

    sblk445 c_blk;
    memset(&c_blk, 0, sizeof(sblk445));

    c_blk.bAP = false;         // indicate no carrier initialized and set up yet
    c_blk.bInitialized = false; // indicate not ready to talk to IP
    c_blk.nHandle = 0;         // make handle to a closed carrier board

    if (InitAPLib() != S_OK) {
        printf("\nUnable to initialize the AP library. Exiting program.\n");
        return 99;
    }

    #define DEVICE_NAME_AP445 "ap445_"      /* name of device */

    if (APOpen(ap_instance, &c_blk.nHandle, DEVICE_NAME_AP445) != S_OK) {
        printf("\nUnable to Open instance of %s%i.\n", DEVICE_NAME_AP445,ap_instance);
        return 98;
    }
    else
    {
        if (APInitialize(c_blk.nHandle) == S_OK)
        { // Initialize
            GetAPAddress(c_blk.nHandle, &addr);  // Read back address
            c_blk.brd_ptr = (struct map445 *)addr;
            c_blk.bInitialized = true;
            c_blk.bAP = true;
        }
        else
        {
            printf("APInitialize Failed. Exiting program.\n");
            return 97;
        }
    }

    switch (mode)
    {
        case 1: { // Read Digital I/O Point
            if (!c_blk.bInitialized) {
                printf("Board Setup Failed. Exiting program.\n");
                return 96;
            }
            else
            {
                long status = rpnt445(&c_blk,point);
                if (status == -1) {
                    printf("Parameter Outof Range. Exiting program.\n");
                    return 95;
                }
                else {
                    //printf("\nValue of point %d: %lx\n", point, status);
                }
            }
            break;
        }

        case 2: { // Read Digital Port
            if (!c_blk.bInitialized) {
                printf("Board Setup Failed. Exiting program.\n");
                return 96;
            }
            else {
                long status = rprt445(&c_blk);
                if (status == -1) {
                    printf("Parameter Outof Range. Exiting program.\n");
                    return 95;
                }
                else {
                    //printf("\nValue %lx\n", status);
                }
            }
            break;
        }

        case 3: { // Write Digital Point
            if (!c_blk.bInitialized) {
                printf("Board Setup Failed. Exiting program.\n");
                return 96;
            }
            else {
                long status = wpnt445(&c_blk,point,value);
                if (status == -1) {
                    printf("Parameter Outof Range. Exiting program.\n");
                    return 95;
                }
                else
                {
                    //printf("\nWritten Value at point %d : %d\n", point,value);
                }
            }
            break;
        }

        case 4: { // Write Digital Port
            if (!c_blk.bInitialized) {
                printf("Board Setup Failed. Exiting program.\n");
                return 96;
            }
            else {
                //value in hex (0 - 0xFFFFFFFF)
                status = wprt445(&c_blk,value);
                if (status == -1) {
                    printf("Parameter Outof Range. Exiting program.\n");
                    return 95;
                }
                else
                {
                    //printf("\nWritten Value at port is %0x\n", value);
                }
            }
            break;
        }
    } // end of switch

    APClose(c_blk.nHandle);
    return status;
}

