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

#ifndef __NAND_H__
#define __NAND_H__

#include <rtthread.h>

#define IO_NF_PFR     FM3_GPIO->PFR3
#define IO_NF_DDR     FM3_GPIO->DDR3
#define IO_NF_PDOR    FM3_GPIO->PDOR3

#define NF_EN         0x0008
#define NF_DATA_DIR   0x0004

#define EXT_BUS_BASE_ADDR    0x60000000
#define EXT_CS7_OFFSET       0x0E000000
#define EXT_CS7_SIZE         0x02000000

#define NF_FLASH_BASE_ADDR   (EXT_BUS_BASE_ADDR+EXT_CS7_OFFSET)

#define NF_ALE_OFFSET        0x00003000
#define NF_ADDR_OFFSET       0x00002000
#define NF_CMD_OFFSET        0x00001000
#define NF_DATA_OFFSET       0x00000000   

/* NAND command */
#define NAND_CMD_READ0     0x00
#define NAND_CMD_READ1     0x01
#define NAND_CMD_PAGEPROG  0x10
#define NAND_CMD_READOOB   0x50
#define NAND_CMD_ERASE1    0x60
#define NAND_CMD_STATUS    0x70
#define NAND_CMD_SEQIN     0x80
#define NAND_CMD_READID    0x90
#define NAND_CMD_READID1   0x91
#define NAND_CMD_ERASE2    0xd0
#define NAND_CMD_RESET     0xff
  
#define FLASH_OK           0
#define FLASH_NG           1

/* nand flash device initialization */
void rt_hw_nand_init(void);

#endif

#ifndef _NAND_CXG_H_
#define _NAND_CXG_H_
//nand_cfg
#define 	ECC_MASK(x)				(x<<29)
#define	ECC_MODE(x)				(x<<27)
#define	CS(x)						(x<<25)
#define	TACLS(x)					(x<<18)
#define	TWRPH0(x)				(x<<11)
#define	TWRPH1(x)				(x<<4)
#define	PageSize(x)				(x<<2)
#define	ChipNum(x)				(x<<1)
#define	FlashWidth(x)			(x<<0)
#define	DEFAULT_CFG				(ECC_MASK(0)|ECC_MODE(0)|CS(0)|TACLS(0x3f)|TWRPH0(0x3f)|TWRPH1(0x3f)|PageSize(2)|FlashWidth(0))
#define	DEFAULT_CFG_16BIT		(DEFAULT_CFG|FlashWidth(1))	
extern int num[4];
extern int ecc_bytes[4] ;
extern int error[4];
extern int NF_flashwidth[2] ;
extern int count[2];
extern char false_bytes[40];
extern char zero_bytes[40];
extern int max_times;

#define write_reg(reg,val)		\
		*(volatile unsigned long*)(reg) = val

#define 	read_reg(reg)		\
		*(volatile unsigned long*)(reg)

#define RANDOM_DATA_INPUT(x)   do { \
		write_reg(NAND_CMD,0x85);	\
		write_reg(NAND_ADDR,(x));		\
		write_reg(NAND_ADDR,((x) >> 8));	\
		} while(0)

#define RANDOM_DATA_OUTPUT(x)		do{	\
		write_reg(NAND_CMD,0x05);	\
		write_reg(NAND_ADDR,(x));		\
		write_reg(NAND_ADDR,((x) >> 8));	\
		write_reg(NAND_CMD,0xe0);		\
		}	while(0)

#define Wait_For_Ready(cs) do{	\
	while(!(read_reg(SEP6200_NAND_STATUS) & (1<<cs)));	\
	}while(0)

#define Wait_For_Decode do{	\
	while(!(read_reg(NAND_STATUS)&(1<<8)));	\
	}while(0)

#define STORE_ECC(ecc_mode) do{	\
	if(ecc_mode == 0x2){printk("mode 0x2\n");RANDOM_DATA_INPUT(1027);write_reg(NAND_SDATA,((read_reg(NAND_ECC_PARITY0) << 4) + 0xF));		\
	    for(i = 1 ;i < num[ecc_mode];i++) old_ecc_parity[i-1] = read_reg(NAND_ECC_PARITY0+4*i);}		\
	else{printk("mode %x\n", ecc_mode);for(i = 0; i < num[ecc_mode]; i++)	old_ecc_parity[i] = read_reg(NAND_ECC_PARITY0+4*i);}	\
	}while(0)

#define ECC_DECODE	do{	\
	Wait_For_Decode;	\
	if(read_reg(NAND_STATUS)& 0x400){printk("ECC Fail \n");return ;	}	\
	else{printk("ECC pass: %d data wrong\n",(unsigned int)((read_reg(NAND_STATUS)>>12)&(0x1F) + ((read_reg(NAND_STATUS)>>26)&0x1)<<5));	\
	for( i = 0 ; i <((read_reg(NAND_STATUS)>>12)& 0x1F + ((read_reg(NAND_STATUS)>>26)&0x1)<<5);i++){tmp = read_reg((unsigned int)NAND_ERR_ADDR0+4*i);if(!tmp) return ;printk("0x%x \n",tmp);}}	\
	}while(0)

/*trans_done*/
#define ENABLE_TRANS_DONE do{	\
	write_reg(NAND_CTRL,read_reg(NAND_CTRL)|(1<<3));	\
	}while(0)

#define DISABLE_TRANS_DONE do{	\
	write_reg(NAND_CTRL,read_reg(NAND_CTRL)&~(1<<3));	\
	}while(0)	

#define CLEAR_TRANS_DONE	do{	\
	write_reg(NAND_STATUS,read_reg(NAND_STATUS)|(1<<9));

/*decoder done*/
#define ENABLE_DEC_DONE do{	\
	write_reg(NAND_CTRL,read_reg(NAND_CTRL)|(1<<2));	\
	}while(0)

#define DISABLE_DEC_DONE do{	\
	write_reg(NAND_CTRL,read_reg(NAND_CTRL)&~(1<<2));	\
	}while(0)	

#define CLEAR_DEC_DONE do{		\
	write_reg(NAND_STATUS,read_reg(NAND_STATUS)|(1<<8));	\
	}while(0)

/*RB_DONE*/
#define ENABLE_RB_DONE do{	\
	write_reg(NAND_CTRL,read_reg(NAND_CTRL)|(1<<1));	\
	}while(0)

#define DISABLE_RB_DONE do{	\
	write_reg(NAND_CTRL,read_reg(NAND_CTRL)&~(1<<1));	\
	}while(0)

/*DMA interrupt*/
#define CLEAR_DMA_INT	do{	\
	write_reg(DMAC1_CLRTFR,0x1);	\
	write_reg(DMAC1_CLRBLK,0x1);	\
	write_reg(DMAC1_CLRSRCTR,0x1);	\
	write_reg(DMAC1_CLRDSTTR,0x1);	\
	write_reg(DMAC1_CLRERR,0x1); 	\
	}while(0)	

#define UNMASK_DMA_INT	do{	\
	write_reg(DMAC1_MASKTRF,0x101) ; 	\
	write_reg(DMAC1_MASKBLK,0x101); 	\
 	write_reg(DMAC1_MASKSRCTR,0x101); 	\
 	write_reg(DMAC1_MASKDSTTR,0x101); 	\
	write_reg(DMAC1_MASKERR,0x101); 	\
	}while(0)

#define MASK_DMA_INT	do{	\
	write_reg(DMAC1_MASKTRF,0x100) ; 	\
	write_reg(DMAC1_MASKBLK,0x100); 	\
 	write_reg(DMAC1_MASKSRCTR,0x100); 	\
 	write_reg(DMAC1_MASKDSTTR,0x100); 	\
	write_reg(DMAC1_MASKERR,0x100); 	\
	}while(0)

#define 	NAND_STATUS_READY   0x40
#define	ROW_CYCLE 	3
#define	COL_CYCLE	2
void Delay(unsigned int cnt);
int Read_Id(void);
unsigned int Read_Status (void);
void Reset_NF(unsigned int cs);
void Select_Chip(unsigned int chip);
void Dma_Write(void*phyaddr,int len);
void Dma_Read(void* phyaddr,int len);
void Generate_ColRow_Address(int page ,int offset);
void Write_page_4096(char*buf,unsigned int cs,unsigned int page,unsigned int dma);
void Read_page_4096(char*buf,unsigned int cs,unsigned int page,unsigned int dma);
void Write_page_8192(char*write_buf,unsigned int page,unsigned int dma);
void Read_page_8192(char*read_buf,unsigned int page,unsigned int dma);
void Block_Erase (int block,unsigned int cs);
/*offset of nandflash controller*/
#endif
