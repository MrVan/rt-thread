/*
 * File      : nand.h
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2011-05-25     Bernard      first version
 * 2013-07-16     Peng Fan     sep6200 implementation
 */

#include <sep6200.h>
#include "nand.h"

/*
 * NandFlash driver for hynix H27UBG8T2A
 */
#define PAGE_SIZE       8192
#define PAGE_PER_BLOCK  256
#define BLOCK_NUM       1024
#define SPARSE_SIZE     448

/* device driver debug trace */
/* #define NAND_DEBUG */
#ifdef NAND_DEBUG
#define trace_log 		rt_kprintf
#else
#define trace_log(...)
#endif

/*
 * OOB,
 * when block has been erased,  OOB is 0xff.
 * when block has been written, OOB is 0x00.
 */
struct rt_device_nand
{
	struct rt_device parent; 	/* which is inherited from rt_device */

	rt_uint16_t block_num;		/* total block number in device 	*/
	rt_uint16_t page_per_block;	/* pages in one block 				*/
	rt_uint16_t page_size;		/* page size 						*/

  rt_uint16_t ecc_steps;
  rt_uint26_t bus_type;
	/* this buffer which used as to save data before erase block 	*/
	rt_uint8_t  block_buffer[PAGE_SIZE * PAGE_PER_BLOCK];
};
static struct rt_device_nand _nand;

/* Flash operation definition */
#define NF_CMD(cmd)     {*(volatile unsigned *)(SEP6200_NAND_CMD) = (unsigned)(cmd);}
#define NF_ADDR(addr)   {*(volatile unsigned *)(SEP6200_NAND_ADDR)= (unsigned)(addr);}

/*8 bit*/
#define NF_RDDATA()     (*(volatile unsigned *)(SEP6200_NAND_SDATA))
#define NF_WRDATA(data) {*(volatile unsigned *)(SEP6200_NAND_SDATA)= (unsigned char)(data);}


int max_times = 2;
static void mdelay(rt_uint32_t ms)
{
    rt_uint32_t len;
    for (;ms > 0; ms --)
        for (len = 0; len < 100; len++ );
}

#define FORCE_MAX_ECC_MODE_24
static void calculate_ecc_mode(void)
{
	int avail_size;
		
	/* calculate available oob size per 1024 bytes */
  _nand.ecc_steps = _nand.page_size >> 10;
	avail_size = _nand.spare_size / _nand.ecc_steps;

#ifndef FORCE_MAX_ECC_MODE_24
	if (avail_size >= 56)
		_nand.ecc_mode = ECC_MODE30;
	else if (avail_size >= 46)
#else
	if (avail_size >= 46)
		_nand.ecc_mode = ECC_MODE24;
#endif
	else if (avail_size >= 32)
		_nand.ecc_mode = ECC_MODE16;
	else
		BUG();

	if (_nand.bus_type == DOUBLE_CHANS_16BITS)
		_nand.ecc_steps <<= 1;
}

#define BUS_CLK_FREQ 240000000
struct nand_features {  
	uint8_t ID[6];
	char *name;
	int ChipSize;	//unit "MBytes"
	int BlockSize;	//unit "KBytes"
	int PageSize;	//unit "Bytes"
	int SpareSize;	//unit "Bytes"
	int ColCycle;	//column address cycle
	int RowCycle;	//row address cycle
	int tCLS;	//CLE Setup Time
	int tALS;	//ALE Setup Time
	int tWC;	//Write Cycle Time
	int tWP;	//WEn Pulse Width
	int tWH;	//WEn High Hold Time 
	int tRC;	//Read Cycle Time
	int tRP;	//REn Pulse Width
	int tREH;	//REn High Hold Time
	int attribute;
};
const struct nand_features hynix = 
{{0xAD, 0xD7, 0x94, 0x9A, 0x74, 0x42},  "Hynix NAND 4G 3.3V 8-bit",  2048, 2048, 8192, 448,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT};

static void calculate_timing_sequence(void)
{
	int HCLK, tacls, twrph0, twrph1, twr;

	HCLK = 1000000000 / BUS_CLK_FREQ;	//unit "ns"

	tacls = hynix.tCLS - hynix.tWP;
	if (tacls < hynix.tALS - hynix.tWP)
		tacls = hynix.tALS - hynix.tWP;	

	_nand.TACLS = tacls / HCLK;
	if (_nand.TACLS * HCLK < tacls)
		_nand.TACLS += 1;	

	twrph0 = (hynix.tWP > hynix.tRP)? hynix.tWP : hynix.tRP;
	twrph1 = (hynix.tWH > hynix.tREH)? hynix.tWH : hynix.tREH;
	twr = (hynix.tWC > hynix.tRC)? hynix.tWC : hynix.tRC;

	_nand.TWRPH0 = twrph0 / HCLK;
	if (_nand.TWRPH0 * HCLK < twrph0)	
		_nand.TWRPH0 += 1; 
	
	nfc.TWRPH1 = twrph1 / HCLK;
	if (nfc.TWRPH1 * HCLK < twrph1)
		nfc.TWRPH1 += 1;
	
	if (_nand.TWRPH0 * HCLK + _nand.TWRPH1 * HCLK < twr) {
		int i = 0;

		while (1) {
			if (!(i & 0x1)) //even
				_nand.TWRPH0++;
			else	//odd
				_nand.TWRPH1++;

			if (_nand.TWRPH0 * HCLK + _nand.TWRPH1 * HCLK >= twr)	
				break;
			
			i++;
		}		
	}

	_nand.TWRPH0 -= 1;
	_nand.TWRPH1 -= 1;

}

static inline void clear_nfc_register(void)
{
    write_reg(0, SEP6200_NAND_CFG);
}

static void nfc_init(void)
{
  clear_nfc_register();

  write_reg(SEP6200_NAND_CFG, ECC_MASK(1) | CS(0) | TACLS(_nand.TACLS + 1) | TWRPH0(_nand.TWRPH0 + 1) |
            TWPPH1(_nand.TWRPH1 + 1));

  switch (_nand.page_size) {
    case 2048:  ps = 0; break; 
    case 4096:  ps = 1; break; 
    default: ps = 2; /*page_size = 8192*/
  }

  write_reg(SEP6200_NAND_CFG, PageSize(ps) | read_reg(SEP6200_NAND_CFG));

  switch (_nand.ecc_mode) {
    case ECC_MODE16:
      write_reg(SEP6200_NAND_CFG, ECC_MODE(0) | read_reg(SEP6200_NAND_CFG));
      break;
    case ECC_MODE24:
      write_reg(SEP6200_NAND_CFG, ECC_MODE(1) | read_reg(SEP6200_NAND_CFG));
      break;
    case ECC_MODE30:
      write_reg(SEP6200_NAND_CFG, ECC_MODE(2) | read_reg(SEP6200_NAND_CFG));
      break;
    default:
      rt_kprintf("Wrong ECC mode\n");
      while(1);
  }

  switch (_nand.bus_type) {
    case SINGLE_CHAN_8BITS:
      write_reg(SEP6200_NAND_CFG, CHIP_NUM(0) | FlashWidth(0) | read_reg(SEP6200_NAND_CFG));
      break;
    case DOUBLE_CHAN_16BITS:
      write_reg(SEP6200_NAND_CFG, CHIP_NUM(0) | FlashWidth(1) | read_reg(SEP6200_NAND_CFG));
      break;
    default: /*SINGLE_CHAN_16BITS*/
      write_reg(SEP6200_NAND_CFG, CHIP_NUM(1) | FlashWidth(1) | read_reg(SEP6200_NAND_CFG));
  }
}

unsigned int Read_Status (void)
{
	unsigned int status;
	write_reg(SEP6200_NAND_CMD,70);
	status = read_reg(SEP6200_NAND_SDATA);
	return status;
}
void Select_Chip(unsigned int chip)
{
	unsigned int old ,new;
	old = read_reg(SEP6200_NAND_CFG);
	new = (old & (~(CS(3)))) + CS(chip);
	write_reg(SEP6200_NAND_CFG,new);
	rt_kprintf("fasfdasfafdsf, cfg = 0x%x, status = 0x%x\n", read_reg(SEP6200_NAND_CFG), read_reg(SEP6200_NAND_STATUS));
	Wait_For_Ready(chip);
	return;
}
void Reset_NF(unsigned int cs)
{
	int i;
	Select_Chip(cs);
	write_reg(SEP6200_NAND_CMD,0xff);//Reset  NandFlash  command
	mdelay(150);
	for(i = 0;i < max_times;i++){
		if(Read_Status()& NAND_STATUS_READY );
			return ;
		mdelay(150);
	}
	rt_kprintf("%s:Reset NANDFLASH timeout \n",__func__);
}

int NF_ReadID(void)
{
	int i ,err = 0;
	char id[6];
	write_reg(SEP6200_NAND_CFG,DEFAULT_CFG);////0001 0010 0010 0100:width=8bit page size=4K 
	rt_kprintf("Reset NANDFlash \n");
	Reset_NF(0);
	write_reg(SEP6200_NAND_CMD,0x90);//ID read command
	write_reg(SEP6200_NAND_ADDR,0x00);//Address is 0x00
	for(i = 0;i < 6;i++){      
		id[i] = (char)read_reg(SEP6200_NAND_SDATA);
		}
	rt_kprintf("Read ID:0x%x ,0x%x ,0x%x ,0x%x,0x%x\n",id[0],id[1],id[2],id[3],id[4],id[5]);
	for( i = 0;i < 5;i++)
		if(id[i] == 0xc0){
			err = 1;
			goto error_out;
		}
	return err;
error_out:
		Reset_NF(0);
	return err;
}

static rt_err_t rt_nand_init (rt_device_t dev)
{
	/* empty implementation */
  Reset_NF(0)
	return RT_EOK;
}

static rt_err_t rt_nand_open(rt_device_t dev, rt_uint16_t oflag)
{
	/* empty implementation */
	return RT_EOK;
}

static rt_err_t rt_nand_close(rt_device_t dev)
{
	/* empty implementation */
	return RT_EOK;
}

/*
 * @ Funciton: NF_ReadPage
 *   Parameter: block (max: 1024)
 *              page  (max:256)
 *              buffer: pointer to data buffer
 *   Return: 0: Flash Operation OK
 *           1: Flash Operation NG
 */
int NF_ReadPage(unsigned int block, unsigned int page, unsigned char *buffer, 
                unsigned char *oob)
{
    unsigned int blockPage,i;

    NF_Init();
    blockPage=(block<<5)+page;					    /* 1 block=32 page */
    NF_OE_L();
    NF_DATA_OUT();
	if (buffer != RT_NULL)
	{
		volatile unsigned char ch;

		NF_CMD(NAND_CMD_READ0);						/* send read data */

		NF_ADDR(0);
		NF_ADDR(blockPage & 0xff);
		NF_ADDR((blockPage>>8) & 0xff);             /* send 3 byte address */
		NF_CLR_ALE();
		NF_DATA_IN();

		Wait(500);

		for(i=0;i<512;i++)							/* read 512 bytes data */
			buffer[i] = NF_RDDATA();
		for(i=0;i<16;i++)							/* read 16 bytes oob */
			if (oob != RT_NULL)
				oob[i] = NF_RDDATA();
			else
				ch = NF_RDDATA();
	}
	else
	{
		NF_CMD(NAND_CMD_READOOB);					/* send read data */

		NF_ADDR(0);
		NF_ADDR(blockPage & 0xff);
		NF_ADDR((blockPage>>8) & 0xff);				/* send 3 byte address */
		NF_CLR_ALE();
		NF_DATA_IN();

		Wait(500);

		for (i=0; i<16; i++)						/* read 16 bytes oob */
		   oob[i] = NF_RDDATA();
	}

    NF_OE_H();
    NF_UnInit();
    return 0;
}
static rt_size_t rt_nand_read (rt_device_t dev, rt_off_t pos, void* buffer, 
                               rt_size_t size)
{
	rt_ubase_t block;			/* block of position */
	rt_ubase_t page, index;		/* page in block of position */
	rt_uint8_t *page_ptr, oob[16];
	struct rt_device_nand *nand;

	/* get nand device */
	nand = (struct rt_device_nand*) dev;
	RT_ASSERT(nand != RT_NULL);

	/* get block and page */
	block = pos / nand->page_per_block;
	page  = pos % nand->page_per_block;

	trace_log("nand read: position %d, block %d, page %d, size %d\n", 
		pos, block, page, size);

	/* set page buffer pointer */
	page_ptr = (rt_uint8_t*) buffer;
	for (index = 0; index < size; index ++)
	{
		NF_ReadPage(block, page + index, page_ptr, oob);
		page_ptr += nand->page_size;
		
		if (page + index > nand->page_per_block)
		{
			block += 1;
			page = 0;
		}
	}

	/* return read size (count of block) */
	return size;
}

void rt_hw_nand_init(void)
{
	/* initialize nand flash structure */
	_nand.block_num = BLOCK_NUM;
	_nand.page_per_block = PAGE_PER_BLOCK;
	_nand.page_size = PAGE_SIZE;

	rt_memset(_nand.block_buffer, 0, sizeof(_nand.block_buffer));

	_nand.parent.type 		= RT_Device_Class_MTD;
	_nand.parent.rx_indicate = RT_NULL;
	_nand.parent.tx_complete = RT_NULL;
	_nand.parent.init 		= rt_nand_init;
	_nand.parent.open		= rt_nand_open;
	_nand.parent.close		= rt_nand_close;
	_nand.parent.read 		= rt_nand_read;
	_nand.parent.write 		= rt_nand_write;
	_nand.parent.control 	= rt_nand_control;

	/* register a MTD device */
	rt_device_register(&(_nand.parent), "nand", RT_DEVICE_FLAG_RDWR);
}
#if 0
static unsigned char NF_ReadStatus(void);
static void Wait(unsigned int cnt);
static void NF_Reset(void);

static void Wait(unsigned int cnt)
{
    while(cnt--);
}

static void NF_Reset(void)
{
    NF_OE_L();
    NF_DATA_OUT();
    NF_CMD(NAND_CMD_RESET);
    NF_OE_H();

    Wait(10000);  /* wait for Trst */
}

static unsigned char NF_ReadStatus(void)
{
    unsigned int timeout=0;
    NF_DATA_OUT();
    NF_CMD(NAND_CMD_STATUS);
    NF_DATA_IN();

    while(!(NF_RDDATA() & 0x40))
    {
       timeout++;
       if(timeout == 0x00080000)
         return FLASH_NG;
    }
    if(NF_RDDATA() & 0x01)return FLASH_NG;

    return FLASH_OK;
}

/*
 * @ Funciton: NF_Init
 *   Parameter: None
 *   Return: None
 */
static void NF_Init(void)
{
    FM3_GPIO->PFR5 |= (0x7ff);		/*  D0-D5, CS7, ALE, CLE, WEX, REX */
    FM3_GPIO->PFR3 |= (0x3);		/* D6-D7 */
    FM3_GPIO->EPFR10 |= (1<<13		/* CS enable */
                       |1<<6		/* ALE, CLE, WEX, REX enable */
                       |1<<0);		/* D0-D7 enable */
    FM3_EXBUS->AREA7 = 0x001f00e0;  /* Select CS7 area, 32Mbyte size */
    FM3_EXBUS->MODE7 |= (1<<4);     /* Nand Flash mode turn on, set 8 bit width */

    IO_NF_PFR = IO_NF_PFR & ~(NF_EN|NF_DATA_DIR);
    IO_NF_DDR = IO_NF_DDR | (NF_EN|NF_DATA_DIR);
    IO_NF_PDOR = IO_NF_PDOR | (NF_EN | NF_DATA_DIR); /* disable Flash operation */

    /*Reset NAND*/
    NF_Reset();
}

static void NF_UnInit(void)
{
    FM3_GPIO->PFR5 &= ~(0x7ff);		/*  disable D0-D5, CS7, ALE, CLE, WEX, REX */
    FM3_GPIO->PFR3 &= ~(0x3);		/* disable D6-D7 */
    FM3_GPIO->EPFR10 &= ~(1<<13		/* disable CS enable */
                       |1<<6        /* disable ALE, CLE, WEX, REX enable */
                       |1<<0);      /* disable D0-D7 enable */
    FM3_EXBUS->MODE7 &= ~(1<<4);
    IO_NF_PFR = IO_NF_PFR & ~(NF_EN|NF_DATA_DIR);
    IO_NF_DDR = IO_NF_DDR | (NF_EN|NF_DATA_DIR);
    IO_NF_PDOR = IO_NF_PDOR | (NF_EN | NF_DATA_DIR); /* disable Flash operation */
}

/*
 * @ Funciton: NF_ReadPage
 *   Parameter: block (max: 2048)
 *              page  (max:32)
 *              buffer: pointer to data buffer
 *   Return: 0: Flash Operation OK
 *           1: Flash Operation NG
 */
int NF_ReadPage(unsigned int block, unsigned int page, unsigned char *buffer, 
                unsigned char *oob)
{
    unsigned int blockPage,i;

    NF_Init();
    blockPage=(block<<5)+page;					    /* 1 block=32 page */
    NF_OE_L();
    NF_DATA_OUT();
	if (buffer != RT_NULL)
	{
		volatile unsigned char ch;

		NF_CMD(NAND_CMD_READ0);						/* send read data */

		NF_ADDR(0);
		NF_ADDR(blockPage & 0xff);
		NF_ADDR((blockPage>>8) & 0xff);             /* send 3 byte address */
		NF_CLR_ALE();
		NF_DATA_IN();

		Wait(500);

		for(i=0;i<512;i++)							/* read 512 bytes data */
			buffer[i] = NF_RDDATA();
		for(i=0;i<16;i++)							/* read 16 bytes oob */
			if (oob != RT_NULL)
				oob[i] = NF_RDDATA();
			else
				ch = NF_RDDATA();
	}
	else
	{
		NF_CMD(NAND_CMD_READOOB);					/* send read data */

		NF_ADDR(0);
		NF_ADDR(blockPage & 0xff);
		NF_ADDR((blockPage>>8) & 0xff);				/* send 3 byte address */
		NF_CLR_ALE();
		NF_DATA_IN();

		Wait(500);

		for (i=0; i<16; i++)						/* read 16 bytes oob */
		   oob[i] = NF_RDDATA();
	}

    NF_OE_H();
    NF_UnInit();
    return 0;
}

/*
 * @ Funciton: NF_EraseBlock
 *   Parameter: block (max: 2048)
 *   Return: 0: Flash Operation OK
 *           1: Flash Operation NG
 */
int NF_EraseBlock(unsigned int block)
{
    rt_uint32_t blockPage;

	trace_log("Erase block %d: ", block);

    NF_Init();
    blockPage = (block << 5);
    NF_OE_L();
    NF_DATA_OUT();
    NF_CMD(NAND_CMD_ERASE1);                        /* send erase command */
    NF_ADDR(blockPage & 0xff);
    NF_ADDR((blockPage >> 8) & 0xff);
    NF_CMD(NAND_CMD_ERASE2);                        /* start erase */

    if(NF_ReadStatus())
    {
		NF_Reset();
		NF_OE_H();
		NF_UnInit();
		trace_log("Failed\n");
		rt_kprintf("erase block failed\n");

		return FLASH_NG;
    }

    NF_OE_H();
    NF_UnInit();

	trace_log("OK\n");

    return FLASH_OK;
}

/*
 * @ Funciton: NF_WritePage
 *   Parameter: block (max: 2048)
 *              page  (max:32)
 *              buffer: pointer to data buffer
 *   Return: 0: Flash Operation OK
 *           1: Flash Operation NG
 */
int NF_WritePage(unsigned block, unsigned page, const rt_uint8_t *buffer)
{
    unsigned int blockPage,i;
    unsigned char se[16] = {0};
    unsigned char data;

    blockPage = (block<<5)+page;
    NF_Init();
    NF_OE_L();
    NF_DATA_OUT();
    NF_CMD(0x00);                                    /* set programming area */
    NF_CMD(NAND_CMD_SEQIN);                          /* send write command */
    NF_ADDR(0);
    NF_ADDR(blockPage & 0xff);
    NF_ADDR((blockPage>>8) & 0xff);
    NF_CLR_ALE();

    for(i=0;i<512;i++) NF_WRDATA(buffer[i]);	/* write data */
    for(i=0;i<16;i++) NF_WRDATA(se[i]);			/* dummy write */

    NF_CMD(NAND_CMD_PAGEPROG);					/* start programming */

    if(NF_ReadStatus())
    {
		NF_Reset();
		NF_OE_H();
		NF_UnInit();
		
		trace_log("write failed\n");
		return FLASH_NG;
    }

    /* verify the write data */
    NF_DATA_OUT();
    NF_CMD(NAND_CMD_READ0);						/* send read command */
    NF_ADDR(0);
    NF_ADDR(blockPage & 0xff);
    NF_ADDR((blockPage>>8) & 0xff);
    NF_CLR_ALE();
    NF_DATA_IN();

    Wait(500);
    for(i=0; i<512; i++)
    {
        data=NF_RDDATA();						/* verify 1-512 byte */
        if(data != buffer[i])
        {
			trace_log("block %d, page %d\n", block , page);
			trace_log("write data failed[%d]: %02x %02x\n", i, data, buffer[i]);

            NF_Reset();
            NF_OE_H();
            NF_UnInit();
            return FLASH_NG;
        }
    }

    for(i=0; i<16; i++)
    {
       data=NF_RDDATA();						/* verify 16 byte dummy data */
        if(data != se[i])
        {
			trace_log("block %d, page %d\n", block , page);
			trace_log("write oob failed[%d]: %02x %02x\n", i, data, se[i]);
            NF_Reset();
            NF_OE_H();
            NF_UnInit();
            return FLASH_NG;
        }
    }

    NF_OE_H();
    NF_UnInit();
    return FLASH_OK;
}

/*
 * @ Funciton: NF_ReadID
 *   Parameter: id: pointer to device ID
 *   Return: None
 */
void NF_ReadID(unsigned char *id)
{
	unsigned char maker_code;
    NF_Init();
    NF_OE_L();
    NF_DATA_OUT();
    NF_CMD(NAND_CMD_READID);
    NF_ADDR(0x00);
    NF_CLR_ALE();
    Wait(10);
    NF_DATA_IN();
    maker_code = NF_RDDATA();
	maker_code = maker_code;
    *id = NF_RDDATA();
    NF_OE_H();
    NF_UnInit();
}

static rt_err_t rt_nand_init (rt_device_t dev)
{
	/* empty implementation */
	return RT_EOK;
}

static rt_err_t rt_nand_open(rt_device_t dev, rt_uint16_t oflag)
{
	/* empty implementation */
	return RT_EOK;
}

static rt_err_t rt_nand_close(rt_device_t dev)
{
	/* empty implementation */
	return RT_EOK;
}

/* nand device read */
static rt_size_t rt_nand_read (rt_device_t dev, rt_off_t pos, void* buffer, 
                               rt_size_t size)
{
	rt_ubase_t block;			/* block of position */
	rt_ubase_t page, index;		/* page in block of position */
	rt_uint8_t *page_ptr, oob[16];
	struct rt_device_nand *nand;

	/* get nand device */
	nand = (struct rt_device_nand*) dev;
	RT_ASSERT(nand != RT_NULL);

	/* get block and page */
	block = pos / nand->page_per_block;
	page  = pos % nand->page_per_block;

	trace_log("nand read: position %d, block %d, page %d, size %d\n", 
		pos, block, page, size);

	/* set page buffer pointer */
	page_ptr = (rt_uint8_t*) buffer;
	for (index = 0; index < size; index ++)
	{
		NF_ReadPage(block, page + index, page_ptr, oob);
		page_ptr += nand->page_size;
		
		if (page + index > nand->page_per_block)
		{
			block += 1;
			page = 0;
		}
	}

	/* return read size (count of block) */
	return size;
}

/*
 * write pages by erase block first
 * @param nand the nand device driver
 * @param block the block of page
 * @param page the page
 * @param buffer the data buffer to be written
 * @param pages the number of pages to be written
 */
static int rt_nand_eraseblock_writepage(struct rt_device_nand* nand, 
                                        rt_ubase_t block, rt_ubase_t page, 
	const rt_uint8_t *buffer, rt_ubase_t pages)
{
	rt_ubase_t index;
	rt_uint32_t page_status;
	rt_uint8_t *page_ptr, oob[16];

	/* set page status */
	page_status = 0;

	/* read each page in block */
	page_ptr = nand->block_buffer;
	for (index = 0; index < nand->page_per_block; index ++)
	{
		NF_ReadPage(block, index, page_ptr, oob);
		if (!oob[0])
			page_status |= (1 << index);
		page_ptr += nand->page_size;
	}

	/* erase block */
	NF_EraseBlock(block);

	page_ptr = &(nand->block_buffer[page * nand->page_size]);
	/* merge buffer to page buffer */
	for (index = 0; index < pages; index ++)
	{
		rt_memcpy(page_ptr, buffer, nand->page_size);

		/* set page status */
		page_status |= (1 << (page + index));

		/* move to next page */
		page_ptr += nand->page_size;
		buffer += nand->page_size;
	}

	/* write to flash */
	page_ptr = nand->block_buffer;
	for (index = 0; index < nand->page_per_block; index ++)
	{
		if (page_status & (1 << index))
			NF_WritePage(block, index, page_ptr);

		/* move to next page */
		page_ptr += nand->page_size;
	}

	return 0;
}

/* nand device write */
static rt_size_t rt_nand_write (rt_device_t dev, rt_off_t pos, 
                                const void* buffer, rt_size_t size)
{
	rt_ubase_t block, page;
	rt_uint8_t oob[16];
	struct rt_device_nand *nand;

	nand = (struct rt_device_nand*) dev;
	RT_ASSERT(nand != RT_NULL);

	/* get block and page */
	block = pos / nand->page_per_block;
	page  = pos % nand->page_per_block;

	trace_log("nand write: position %d, block %d, page %d, size %d\n", 
		pos, block, page, size);

	if (size == 1)
	{
		/* write one page */

		/* read oob to get page status */
		NF_ReadPage(block, page, RT_NULL, oob);
		if (oob[0])
			NF_WritePage(block, page, buffer);
		else
			/* erase block and then write page */
			rt_nand_eraseblock_writepage(nand, block, page, buffer, 1);
	}
	else if (size > 1)
	{
		rt_ubase_t index;
		rt_ubase_t need_erase_block;
		const rt_uint8_t *page_ptr;
		rt_ubase_t chunk_pages, pages;

		pages = size;
		page_ptr = (const rt_uint8_t*) buffer;
		do
		{
			need_erase_block = 0;
			/* calculate pages in current chunk */
			if (pages > nand->page_per_block - page)
				chunk_pages = nand->page_per_block - page;
			else 
				chunk_pages = pages;

			/* get page status in current block */
			for (index = page; index < page + chunk_pages; index ++)
			{
				NF_ReadPage(block, index, RT_NULL, oob);
				if (!oob[0])
				{
					/* this page has data, need erase this block firstly */
					need_erase_block = 1;
					break;
				}
			}

			if (need_erase_block)
			{
				/* erase block and then write it */
				rt_nand_eraseblock_writepage(nand, block, page, page_ptr, chunk_pages);
				page_ptr += chunk_pages * nand->page_size;
			}
			else
			{
				/* write pages directly */
				for (index = page; index < page + chunk_pages; index ++)
				{
					NF_WritePage(block, index, page_ptr);
					page_ptr += nand->page_size;
				}
			}

			pages -= chunk_pages;
			page = 0; block ++; /* move to next block */
		}
		while (pages);
	}

	return size;
}

static rt_err_t rt_nand_control (rt_device_t dev, rt_uint8_t cmd, void *args)
{
	struct rt_device_nand *nand;

	nand = (struct rt_device_nand*) dev;
    RT_ASSERT(dev != RT_NULL);

    switch (cmd)
	{
	case RT_DEVICE_CTRL_BLK_GETGEOME:
		{
			struct rt_device_blk_geometry *geometry;

			geometry = (struct rt_device_blk_geometry *)args;
			if (geometry == RT_NULL) return -RT_ERROR;

			geometry->bytes_per_sector = nand->page_size;
			geometry->block_size = nand->page_size * nand->page_per_block;
			geometry->sector_count = nand->block_num * nand->page_per_block;
		}
		break;
	}

	return RT_EOK;
}

void rt_hw_nand_init(void)
{
	/* initialize nand flash structure */
	_nand.block_num = BLOCK_NUM;
	_nand.page_per_block = PAGE_PER_BLOCK;
	_nand.page_size = PAGE_SIZE;

	rt_memset(_nand.block_buffer, 0, sizeof(_nand.block_buffer));

	_nand.parent.type 		= RT_Device_Class_MTD;
	_nand.parent.rx_indicate = RT_NULL;
	_nand.parent.tx_complete = RT_NULL;
	_nand.parent.init 		= rt_nand_init;
	_nand.parent.open		= rt_nand_open;
	_nand.parent.close		= rt_nand_close;
	_nand.parent.read 		= rt_nand_read;
	_nand.parent.write 		= rt_nand_write;
	_nand.parent.control 	= rt_nand_control;

	/* register a MTD device */
	rt_device_register(&(_nand.parent), "nand", RT_DEVICE_FLAG_RDWR);
}

#ifdef NAND_DEBUG
#include <finsh.h>
unsigned char nand_buffer[512];
unsigned char nand_oob[16];

void dump_mem(unsigned char* buffer, int length)
{
	int i;

	if (length > 64) length = 64;	
	for (i = 0; i < length; i ++)
	{
		rt_kprintf("%02x ", *buffer++);
		if (((i+1) % 16) == 0)
		rt_kprintf("\n");
	}
	rt_kprintf("\n");
}

void nand_read(int block, int page)
{
	rt_kprintf("read block %d, page %d\n", block, page);

	NF_ReadPage(block, page, nand_buffer, nand_oob);
	rt_kprintf("page data:\n");
	dump_mem(nand_buffer, 512);
	rt_kprintf("oob data:\n");
	dump_mem(nand_oob, 16);
}
FINSH_FUNCTION_EXPORT_ALIAS(nand_read, read_page, read page[block/page]);

void nand_write(int block, int page)
{
	int i;
	for (i = 0; i < 512; i ++)
		nand_buffer[i] = i;

	NF_WritePage(block, page, nand_buffer);
}
FINSH_FUNCTION_EXPORT_ALIAS(nand_write, write_page, write page[block/page]);

void nand_erase(int block)
{
	NF_EraseBlock(block);
}
FINSH_FUNCTION_EXPORT_ALIAS(nand_erase, erase_block, erase block[block]);

void nand_readoob(int block, int page)
{
	rt_kprintf("read oob on block %d, page %d\n", block, page);

	NF_ReadPage(block, page, RT_NULL, (unsigned char*)nand_oob);
	rt_kprintf("oob data:\n");
	dump_mem(nand_oob, 16);
}
FINSH_FUNCTION_EXPORT_ALIAS(nand_readoob, readoob, read oob[block/page]);

void nand_erase_chip()
{
	int i;
	unsigned char id;
	
	NF_ReadID(&id);
	rt_kprintf("id: %02x\n", id);

	for (i = 0; i < 2048; i ++)
	{
		NF_EraseBlock(i);
	}
}
FINSH_FUNCTION_EXPORT_ALIAS(nand_erase_chip, erase_chip, erase whole chip);
#endif
#endif
