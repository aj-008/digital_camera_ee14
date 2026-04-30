// diskio.c
#include "ff.h"
#include "diskio.h"
#include "stm32_adafruit_sd.h"

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    if (BSP_SD_Init() == BSP_SD_OK)
        return 0;           // RES_OK
    return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    if (BSP_SD_GetCardState() == BSP_SD_OK)
        return 0;
    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    if (BSP_SD_ReadBlocks((uint32_t*)buff, sector, count, SD_DATATIMEOUT) == BSP_SD_OK)
        return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    if (BSP_SD_WriteBlocks((uint32_t*)buff, sector, count, SD_DATATIMEOUT) == BSP_SD_OK)
        return RES_OK;
    return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    SD_CardInfo info;
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT:
            BSP_SD_GetCardInfo(&info);
            *(LBA_t*)buff = info.LogBlockNbr;
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            return RES_OK;
    }
    return RES_PARERR;
}

DWORD get_fattime(void) {
    return ((DWORD)(2024 - 1980) << 25)
         | ((DWORD)1 << 21)
         | ((DWORD)1 << 16)
         | ((DWORD)0 << 11)
         | ((DWORD)0 << 5)
         | ((DWORD)0 >> 1);
}
