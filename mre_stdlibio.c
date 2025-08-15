#include "mre_stdlibio.h"
#include <string.h>
#include <stdio.h>

/* ---- Failo handle wrapper ---- */
typedef struct {
    VMFILE fh;
    VMBOOL from_malloc;  // VM_TRUE jei alokuota mz_fopen_mre()
} MRE_FILE_WRAPPER;

/* Skaičiavimui/debugui */
int file_opened = 0;
int file_closed = 0;

/* ---- Malloc wrappers ---- */
void *malloc_mre(size_t size) {
    return vm_malloc((int)size);
}

void free_mre(void *memory) {
    vm_free(memory);
}

void *realloc_mre(void *memory, size_t new_size) {
    if (memory == NULL) {
        return vm_malloc((int)new_size);
    }
    return vm_realloc(memory, (int)new_size);
}

/* ---- Helper ---- */
static int mode_is(const char *mode, const char *pattern) {
    if (!mode || !pattern) return 0;
    return (strcmp(mode, pattern) == 0);
}

/* ---- Open file ---- */
MZ_FILE *mz_fopen_mre(const char *pFilename, const char *pMode) {
    if (!pFilename || !pMode) return NULL;

    VMUINT mode = MODE_READ;
    VMUINT binary = FALSE;

    if (mode_is(pMode, "wb")) { mode = MODE_CREATE_ALWAYS_WRITE; binary = TRUE; }
    else if (mode_is(pMode, "w")) { mode = MODE_CREATE_ALWAYS_WRITE; binary = FALSE; }
    else if (mode_is(pMode, "r")) { mode = MODE_READ; binary = FALSE; }
    else if (mode_is(pMode, "rb")) { mode = MODE_READ; binary = TRUE; }
    else if (mode_is(pMode, "r+b") || mode_is(pMode, "rb+")) { mode = MODE_WRITE; binary = TRUE; }
    else if (mode_is(pMode, "a")) { mode = MODE_APPEND; binary = FALSE; }
    else if (mode_is(pMode, "ab")) { mode = MODE_APPEND; binary = TRUE; }

    MRE_FILE_WRAPPER *wrap = (MRE_FILE_WRAPPER *)vm_malloc(sizeof(MRE_FILE_WRAPPER));
    if (!wrap) return NULL;

    wrap->from_malloc = VM_TRUE;
    wrap->fh = mre_open_file((VMSTR)pFilename, mode, binary);

    if (wrap->fh < 0) {
        vm_free(wrap);
        return NULL;
    }

    file_opened++;
    return (MZ_FILE *)wrap;
}

/* ---- Read ---- */
size_t fread_mre(void *pBuf, int elementSize, size_t n, MZ_FILE *pFile) {
    if (!pBuf || !pFile) return 0;
    MRE_FILE_WRAPPER *wrap = (MRE_FILE_WRAPPER *)pFile;
    VMUINT read_bytes = 0;
    size_t total_bytes = (size_t)elementSize * n;

    VMINT file_read_result = vm_file_read(wrap->fh, pBuf, (VMUINT)total_bytes, &read_bytes);
    if (file_read_result < 0) return 0;

    return (size_t)(read_bytes / (VMUINT)elementSize);
}

/* ---- Write ---- */
size_t fwrite_mre(const void *pBuf, int element_size, size_t n, MZ_FILE *pFile) {
    if (!pBuf || !pFile) return 0;
    MRE_FILE_WRAPPER *wrap = (MRE_FILE_WRAPPER *)pFile;
    VMUINT written_bytes = 0;

    VMINT res = vm_file_write(wrap->fh, (void *)pBuf,
                              (VMUINT)(element_size * n),
                              &written_bytes);
    if (res < 0) return 0;

    //vm_file_commit(wrap->fh);
    return written_bytes / element_size;
}

/* ---- Tell ---- */
__int64 _ftelli64_mre(MZ_FILE *pFile) {
    if (!pFile) return -1;
    MRE_FILE_WRAPPER *wrap = (MRE_FILE_WRAPPER *)pFile;
    VMINT pointer_position = vm_file_tell(wrap->fh);
    return (__int64) pointer_position;
}

/* ---- Seek ---- */
int _fseeki64_mre(MZ_FILE *file_handle, long offset, int origin) {
    if (!file_handle) return -1;
    MRE_FILE_WRAPPER *wrap = (MRE_FILE_WRAPPER *)file_handle;
    int base;
    switch(origin) {
        case SEEK_CUR: base = BASE_CURR; break;
        case SEEK_END: base = BASE_END; break;
        case SEEK_SET:
        default: base = BASE_BEGIN; break;
    }
    return vm_file_seek(wrap->fh, (VMINT)offset, (VMINT)base);
}

/* ---- Stat ---- */
int _stat_mre(const char* file_path, void *stat_object) {
    (void)stat_object;
    VMBOOL file_exists = does_this_file_exist((VMSTR)file_path);
    return (file_exists == TRUE) ? 0 : -1;
}

VMBOOL does_this_file_exist(VMSTR filename) {
    if (!filename) return FALSE;
    VMINT wfilename_size = (int)((strlen(filename) + 1) * 2);
    VMWSTR wfilename = (VMWSTR)vm_malloc(wfilename_size);
    if (!wfilename) return FALSE;
    vm_ascii_to_ucs2(wfilename, MRE_STR_SIZE_MAX, filename);
    VMBOOL res = does_this_wfile_exist(wfilename);
    vm_free(wfilename);
    return res;
}

VMBOOL does_this_wfile_exist(VMWSTR wfilename) {
    if (!wfilename) return FALSE;
    VMFILE test_handle = vm_file_open(wfilename, MODE_READ, FALSE);
    if(test_handle < 0) {
        return FALSE;
    } else {
        vm_file_close(test_handle);
        return TRUE;
    }
}

/* ---- Close ---- */
int fclose_mre(MZ_FILE *file_handle) {
    if (!file_handle) return EOF;
    MRE_FILE_WRAPPER *wrap = (MRE_FILE_WRAPPER *)file_handle;

    //vm_file_commit(wrap->fh);
    vm_file_close(wrap->fh);

    if (wrap->from_malloc) {
        vm_free(wrap);
    }

    file_closed++;
    return 0;
}

/* ---- Remove ---- */
int remove_mre(const char *file_path) {
    if (!file_path) return -1;
    VMWCHAR wfile_path[MRE_STR_SIZE_MAX + 1];
    vm_ascii_to_ucs2 (wfile_path, MRE_STR_SIZE_MAX, file_path);
    return vm_file_delete(wfile_path);
}

/* ---- Freopen ---- */
MZ_FILE *mz_freopen_mre(const char *pPath, const char *pMode, MZ_FILE *old_file_handle) {
    if (old_file_handle) {
        fclose_mre(old_file_handle);
    }
    return mz_fopen_mre(pPath, pMode);
}

/* ---- Flush ---- */
int fflush_mre(MZ_FILE *file_handle) {
    if (!file_handle) return -1;
    MRE_FILE_WRAPPER *wrap = (MRE_FILE_WRAPPER *)file_handle;
    return vm_fs_async_commit(wrap->fh);
}

/* ---- Open helper ---- */
VMFILE mre_open_file(VMSTR file_path, VMUINT mode, VMBOOL binary) {
    VMINT wfile_size = (strlen(file_path) + 1) * sizeof(VMWCHAR);
    VMWSTR wfile_path = (VMWSTR)vm_malloc(wfile_size);
    if (!wfile_path) return -1;
    vm_ascii_to_ucs2(wfile_path, MRE_STR_SIZE_MAX, file_path);
    VMFILE fh = vm_file_open(wfile_path, mode, binary);
    vm_free(wfile_path);
    return fh;
}
