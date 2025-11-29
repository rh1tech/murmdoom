#include "ff.h"
#include "w_file.h"
#include "z_zone.h"
#include "m_misc.h"

typedef struct
{
    wad_file_t wad;
    FIL file;
} fatfs_wad_file_t;

extern wad_file_class_t stdc_wad_file; // We implement this one

static wad_file_t *W_FatFs_OpenFile(char *path)
{
    fatfs_wad_file_t *result;
    FRESULT fr;

    result = Z_Malloc(sizeof(fatfs_wad_file_t), PU_STATIC, 0);
    
    fr = f_open(&result->file, path, FA_READ);

    if (fr != FR_OK)
    {
        Z_Free(result);
        return NULL;
    }

    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = f_size(&result->file);

    return &result->wad;
}

static void W_FatFs_CloseFile(wad_file_t *wad)
{
    fatfs_wad_file_t *fatfs_wad;

    fatfs_wad = (fatfs_wad_file_t *) wad;

    f_close(&fatfs_wad->file);
    Z_Free(fatfs_wad);
}

size_t W_FatFs_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    fatfs_wad_file_t *fatfs_wad;
    UINT br;
    FRESULT fr;

    fatfs_wad = (fatfs_wad_file_t *) wad;

    f_lseek(&fatfs_wad->file, offset);
    fr = f_read(&fatfs_wad->file, buffer, buffer_len, &br);

    if (fr != FR_OK) return 0;
    return br;
}

wad_file_class_t stdc_wad_file = 
{
    W_FatFs_OpenFile,
    W_FatFs_CloseFile,
    W_FatFs_Read,
};
