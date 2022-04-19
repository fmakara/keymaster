/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <string.h>
#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#define DISK_BLOCK_NUM 130
#define DISK_BLOCK_SIZE 512
uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE];

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (BYTE pdrv){
	if(pdrv==0) return RES_OK;
	return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (BYTE pdrv){
	if(pdrv==0) return RES_OK;
	return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if(pdrv!=0) return STA_NOINIT;
	if(sector+count>DISK_BLOCK_NUM) return RES_ERROR;
	memcpy(buff, msc_disk[sector], count*DISK_BLOCK_SIZE);
	return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if(pdrv!=0) return STA_NOINIT;
	if(sector+count>DISK_BLOCK_NUM) return RES_ERROR;
	memcpy(msc_disk[sector], buff, count*DISK_BLOCK_SIZE);
	return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if(pdrv!=0) return STA_NOINIT;
	switch(cmd){
		case CTRL_SYNC: break;
		case GET_SECTOR_COUNT: *((DWORD*)buff) = DISK_BLOCK_NUM; break;
		case GET_SECTOR_SIZE: *((WORD*)buff) = DISK_BLOCK_SIZE; break;
		case GET_BLOCK_SIZE: *((DWORD*)buff) = 1; break;
		case CTRL_TRIM: break;
		case CTRL_FORMAT: memset(msc_disk[0], 0, sizeof(msc_disk)); break;
	}
	return RES_OK;
}
