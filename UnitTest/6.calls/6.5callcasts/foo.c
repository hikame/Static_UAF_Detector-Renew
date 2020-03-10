#include <stdlib.h>
#include <stdint.h>

typedef void *zipFile;  

typedef struct mz_compat_s {
    void     *stream;
    void     *handle;
    uint64_t entry_index;
    int64_t  entry_pos;
    int64_t  total_out;
} mz_compat;

typedef struct mz_zip_file_s
{
    uint16_t version_madeby;            /* version made by */
} mz_zip_file, mz_zip_entry;

typedef struct mz_zip_s
{
    mz_zip_file file_info;
    mz_zip_file local_file_info;

    void *stream;                   /* main stream */
    void *cd_stream;                /* pointer to the stream with the cd */
    void *cd_mem_stream;            /* memory stream for central directory */
    void *compress_stream;          /* compression stream */
    void *crypt_stream;             /* encryption stream */
    void *file_info_stream;         /* memory stream for storing file info */
    void *local_file_info_stream;   /* memory stream for storing local file info */

    int32_t  open_mode;
    uint8_t  recover;

    uint32_t disk_number_with_cd;   /* number of the disk with the central dir */
    int64_t  disk_offset_shift;     /* correction for zips that have wrong offset start of cd */

    int64_t  cd_start_pos;          /* pos of the first file in the central dir stream */
    int64_t  cd_current_pos;        /* pos of the current file in the central dir */
    int64_t  cd_offset;             /* offset of start of central directory */
    int64_t  cd_size;               /* size of the central directory */

    uint8_t  entry_scanned;         /* entry header information read ok */
    uint8_t  entry_opened;          /* entry is open for read/write */
    uint8_t  entry_raw;             /* entry opened with raw mode */
    uint32_t entry_crc32;           /* entry crc32  */

    uint64_t number_entry;

    uint16_t version_madeby;
    char     *comment;
} mz_zip;

int32_t mz_zip_set_comment(void *handle, const char *comment)
{
    mz_zip *zip = (mz_zip *)handle;
    free(zip->comment);
    return 0;
}

int32_t mz_zip_close(void *handle)
{
    mz_zip *zip = (mz_zip *)handle;
    if (zip->comment)
    {
        free(zip->comment);
        zip->comment = NULL;
    }
    return 0;
}

int zipClose2_MZ(zipFile file, const char *global_comment, uint16_t version_madeby)
{
    mz_compat *compat = (mz_compat *)file;
    mz_zip_set_comment(compat->handle, global_comment);
    mz_zip_close(compat->handle);

    return 0;
}

int main(){
  zipClose2_MZ(NULL, NULL, 0);
}