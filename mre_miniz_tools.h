#ifndef _VRE_APP_WIZARDTEMPLATE_
#define	_VRE_APP_WIZARDTEMPLATE_

#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include "vmstdlib.h"
#include "ResID.h"
#include "vm4res.h"
#include "vmmm.h"
#include <stdio.h>
#include <stdlib.h>
#include "mre_stdlibio.h"
#include "miniz.h"

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint;

#define ZIP_FILE_PATH      "e:\\testas.zip"       // ZIP failo kelias
#define OUTPUT_FILE_PATH   "e:\\issaugotas.txt"   // Išarchyvuotas failas
#define MAX_ZIP_SIZE       (1024 * 100)           // Maks. 100 KB
#define INPUT_FILE     "e:\\testas.txt"
#define OUTPUT_ZIP     "e:\\testas.zip"

VMINT layer_hdl[1];

void handle_sysevt(VMINT message, VMINT param);
VMINT job(VMWCHAR *file_path, VMINT wlen);
int unzip_file_in_memory(VMWSTR gz_file_path, VMWSTR out_file_path);
int unzip_all_files_to_folder(VMWSTR gz_file_path);
int compress_file_to_zip(VMWSTR gz_file_path);
void remove_file_extension_wchar(VMWCHAR *filename);
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


