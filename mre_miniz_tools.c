#include "mre_miniz_tools.h"

VMBOOL trigeris = VM_FALSE;

void _sbrk(){}
void _write(){}
void _close(){}
void _lseek(){}
void _open(){}
void _read(){}
void _exit(int status) {
	//LOG_M("Exit");
	vm_exit_app();
	//asm("ldr SP, %0" : "=m" (return_sp));
	//asm("ldr PC, %0" : "=m" (return_address));
}
void _getpid(){}
void _kill(){}
void _fstat(){}
void _isatty(){}

void vm_main(void) {
	vm_reg_sysevt_callback(handle_sysevt);
}

void handle_sysevt(VMINT message, VMINT param) {

    switch (message) {
        case VM_MSG_CREATE:
        case VM_MSG_ACTIVE:
            break;

        case VM_MSG_PAINT:
            if (trigeris == VM_TRUE) {vm_exit_app();}
            if (vm_selector_run(0, 0, job) == 0) {trigeris = VM_TRUE;}
            break;

        case VM_MSG_INACTIVE:
            break;

        case VM_MSG_QUIT:
            break;
    }
}

VMINT job(VMWCHAR *file_path, VMINT wlen) {

    VMWCHAR autoFullPathName[100];
    VMWCHAR wfile_extension[8];

    vm_ascii_to_ucs2(wfile_extension, 8, "bin");
    vm_wstrncpy(autoFullPathName, file_path, vm_wstrlen(file_path) - 3); 
    vm_wstrcat(autoFullPathName, wfile_extension);

    int unzip_res = unzip_all_files_to_folder(file_path);
    //int unzip_res = compress_file_to_zip(file_path);

    //if (unzip_res == ZT_SUCCESS) {
       //sprintf(text, "Download and unzip successful, size: %d bytes", file_size);
    //} else {
       //vm_audio_play_beep();
       //vm_vibrator_once();
       //sprintf(text, "Unzip failed with code: %d", unzip_res);
       //vm_file_delete(autoFullPathName);
    //}

    return 0;
}

int unzip_all_files_to_folder(VMWSTR gz_file_path) {

    VMWCHAR wpath[260];
    VMWCHAR folder_name[100];
    char folder_ascii[260];
    char zip_filename[128];
    char *zip_data = NULL;
    mz_zip_archive zip_archive;
    VMFILE file_handle;
    VMUINT zip_size, read_size;
    VMINT i, num_files;

    // 1. Atidaryti ZIP failą
    file_handle = vm_file_open(gz_file_path, MODE_READ, FALSE);
    if (file_handle < 0) return -1;

    if (vm_file_getfilesize(file_handle, &zip_size) != 0 || zip_size == 0 || zip_size > 1024 * 1024 * 5) {
        vm_file_close(file_handle);
        return -2;
    }

    zip_data = (char *)vm_malloc(zip_size);
    if (!zip_data) {
        vm_file_close(file_handle);
        return -3;
    }

    vm_file_read(file_handle, zip_data, zip_size, &read_size);
    vm_file_close(file_handle);
    if (read_size != zip_size) {
        vm_free(zip_data);
        return -4;
    }

    // 2. Gauti ZIP failo vardą (be kelio)
    vm_get_filename(gz_file_path, folder_name);       // pvz. "myfile.zip"
    remove_file_extension_wchar(folder_name);         // pvz. "myfile"

    // Sukurti tikslinį katalogą
    vm_get_path(gz_file_path, wpath);                 // pvz. "e:\\downloads\\"
    vm_wstrcat(wpath, folder_name);                   // pvz. "e:\\downloads\\myfile"
    vm_file_mkdir(wpath);                             // sukuriame katalogą

    // Konvertuojame katalogą į ASCII
    vm_ucs2_to_ascii(folder_ascii, sizeof(folder_ascii), wpath);

    // 3. Inicializuoti ZIP
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_mem(&zip_archive, zip_data, zip_size, 0)) {
        vm_free(zip_data);
        return -5;
    }

    num_files = (VMINT)mz_zip_reader_get_num_files(&zip_archive);

    for (i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue;

        size_t file_size;
        void *file_data = mz_zip_reader_extract_to_heap(&zip_archive, i, &file_size, 0);
        if (!file_data) continue;

        // Sudarom pilną kelią į failą
        char full_ascii_path[260];
        vm_sprintf(full_ascii_path, "%s\\%s", folder_ascii, file_stat.m_filename);

        // Sukuriam poaplankius jei reikia
        char temp_ascii_dir[260];
        strcpy(temp_ascii_dir, full_ascii_path);
        char *last_slash = strrchr(temp_ascii_dir, '\\');
        if (last_slash) {
            *last_slash = '\0';
            vm_ascii_to_ucs2(wpath, sizeof(wpath), temp_ascii_dir);
            vm_file_mkdir(wpath);
        }

        // Įrašom failą
        vm_ascii_to_ucs2(wpath, sizeof(wpath), full_ascii_path);
        VMFILE out = vm_file_open(wpath, MODE_CREATE_ALWAYS_WRITE, FALSE);
        if (out >= 0) {
            VMUINT written;
            vm_file_write(out, file_data, (VMUINT)file_size, &written);
            vm_file_close(out);
        }

        mz_free(file_data);
    }

    mz_zip_reader_end(&zip_archive);
    vm_free(zip_data);
    return 0;
}

int compress_file_to_zip(VMWSTR gz_file_path) {

    VMWCHAR wpath[260];
    char *file_data = NULL;
    mz_zip_archive zip;
    void *zip_buffer = NULL;
    size_t zip_size = 0;
    VMFILE file;
    VMUINT file_size, read_size, written;
    VMWCHAR w_my_file_name[128];
    VMCHAR my_file_name[128];
    VMWCHAR autoFullPathName[260];
    VMWCHAR wfile_extension[8];

    // 1. Atidaryti originalų failą
    file = vm_file_open(gz_file_path, MODE_READ, FALSE);
    if (file < 0) return -1;

    if (vm_file_getfilesize(file, &file_size) != 0 || file_size == 0 || file_size > 1024 * 1024 * 2) {  // max 2MB
        vm_file_close(file);
        return -2;
    }

    file_data = (char *)vm_malloc(file_size);
    if (!file_data) {
        vm_file_close(file);
        return -3;
    }

    vm_file_read(file, file_data, file_size, &read_size);
    vm_file_close(file);
    if (read_size != file_size) {
        vm_free(file_data);
        return -4;
    }

    // 2. Inicializuoti ZIP rašymą
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        vm_free(file_data);
        return -5;
    }

    // 3. Gauti failo pavadinimą ZIP viduje
    vm_get_filename(gz_file_path, w_my_file_name);
    vm_ucs2_to_ascii(my_file_name, sizeof(my_file_name), w_my_file_name);

    if (!mz_zip_writer_add_mem(&zip, my_file_name, file_data, file_size, MZ_BEST_COMPRESSION)) {
        mz_zip_writer_end(&zip);
        vm_free(file_data);
        return -6;
    }

    // 4. Užbaigti archyvą ir gauti ZIP duomenis
    //zip_buffer = mz_zip_writer_finalize_heap_archive(&zip, &zip_size);
    //zip_buffer = mz_zip_writer_finalize_heap_archive(&zip, &zip_size, NULL);

    if (!mz_zip_writer_finalize_heap_archive(&zip, &zip_buffer, &zip_size)) {
       mz_zip_writer_end(&zip);
       vm_free(file_data);
       return -7;
    }

    if (!zip_buffer || zip_size == 0) {
        mz_zip_writer_end(&zip);
        vm_free(file_data);
        return -7;
    }

    // 5. Sugeneruoti ZIP failo vardą (pakeičiant plėtinį į .zip)
    vm_ascii_to_ucs2(wfile_extension, sizeof(wfile_extension), "zip");
    VMINT len = vm_wstrlen(gz_file_path);
    vm_wstrncpy(autoFullPathName, gz_file_path, len - 3);  // pašalinam pvz. ".txt"
    autoFullPathName[len - 3] = 0;
    vm_wstrcat(autoFullPathName, wfile_extension);         // pridedam ".zip"

    // 6. Įrašyti ZIP į failą
    file = vm_file_open(autoFullPathName, MODE_CREATE_ALWAYS_WRITE, FALSE);
    if (file < 0) {
        mz_zip_writer_end(&zip);
        mz_free(zip_buffer);
        vm_free(file_data);
        return -8;
    }

    vm_file_write(file, zip_buffer, (VMUINT)zip_size, &written);
    vm_file_close(file);

    // 7. Atlaisvinti atmintį
    mz_zip_writer_end(&zip);
    mz_free(zip_buffer);
    vm_free(file_data);

    return 0;  // Sėkmingai suspausta
}



void remove_file_extension_wchar(VMWCHAR *filename) {

    int len = 0;
    int dot_pos = -1;

    // Surasti paskutinio taško (.) poziciją
    while (filename[len] != 0) {
        if (filename[len] == (VMWCHAR)'.') {
            dot_pos = len;
        }
        len++;
    }

    // Jei rasta, nutraukti eilutę ties tašku
    if (dot_pos != -1) {
        filename[dot_pos] = 0;
    }
}