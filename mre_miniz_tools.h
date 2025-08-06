#ifndef _VRE_APP_WIZARDTEMPLATE_
#define	_VRE_APP_WIZARDTEMPLATE_

#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmchset.h"
#include <stdio.h>
#include "mre_stdlibio.h"
#include "miniz.h"

void handle_sysevt(VMINT message, VMINT param);
VMINT job(VMWCHAR *file_path, VMINT wlen);
int unzip_all_files_to_folder(VMWSTR input_path);
int compress_file_to_zip(VMWSTR zip_path);
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


