#ifndef _VRE_APP_WIZARDTEMPLATE_
#define	_VRE_APP_WIZARDTEMPLATE_

#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include <stdio.h>
#include "mre_stdlibio.h"
#include "miniz.h"

#include "string.h"
#include "stdint.h"
#include "ctype.h"

void handle_sysevt(VMINT message, VMINT param);
VMINT job(VMWCHAR *file_path, VMINT wlen);
int unzip_all_files_to_folder(VMWSTR input_path);
void normalize_path_slashes(char *path);
void mkdir_recursive_ascii(const char *path);
int compress_file_to_zip(VMWSTR zip_path);
void remove_file_extension_wchar(VMWCHAR *filename);
void get_relative_zip_path(VMWSTR base_path, VMWSTR full_path, char *relative_ascii);
int add_files_to_zip_mem_recursive(mz_zip_archive *zip, VMWSTR base_path, VMWSTR current_path, VMUINT *total_size_ptr);
int compress_directory_to_zip(VMWSTR dir_path);
void create_search_path(VMWSTR result, VMWSTR source, VMSTR text);
void append_backslash_to_path(VMWSTR dest);
int decompress_gzip_file_stream(VMWSTR input_path);
void _sbrk();
void _write();
void _close();
void _lseek();
void _open();
void _read();
void _exit(int status);
void _getpid();
void _kill();
void _fstat();
void _isatty();

#endif


