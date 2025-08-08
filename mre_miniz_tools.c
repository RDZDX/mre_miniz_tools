#include "mre_miniz_tools.h"

#define MAX_TOTAL_SIZE (2 * 1024 * 1024) // 2 MB
#define MAX_UNCOMPRESS_SIZE (2 * 1024 * 1024) // 2 MB

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

    //VMWCHAR dir_pathx[260];
    //vm_ascii_to_ucs2(dir_pathx, sizeof(dir_pathx), "E:\\testz");

    //int unzip_res = unzip_all_files_to_folder(file_path);
    //int unzip_res = compress_file_to_zip(file_path);
    int unzip_res = decompress_gzip_file(file_path);
    //int unzip_res = compress_directory_to_zip(dir_pathx);

    return 0;
}

int unzip_all_files_to_folder(VMWSTR input_path) {

    VMWCHAR wpath[260];
    VMWCHAR folder_name[100];
    char folder_ascii[260];
    char *zip_data = NULL;
    mz_zip_archive zip_archive;
    VMFILE file_handle;
    VMUINT zip_size, read_size;
    VMINT i, num_files;

    // Atidaryti ZIP failą
    file_handle = vm_file_open(input_path, MODE_READ, FALSE);
    if (file_handle < 0) return -1;

    if (vm_file_getfilesize(file_handle, &zip_size) != 0 || zip_size == 0) {
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

    // Gauti ZIP failo vardą
    vm_get_filename(input_path, folder_name);      // pvz. "archive.zip"
    remove_file_extension_wchar(folder_name);      // tampa "archive"

    // Sukurti tikslinį katalogą
    vm_get_path(input_path, wpath);                // pvz. "e:\\downloads\\"
    vm_wstrcat(wpath, folder_name);                // "e:\\downloads\\archive"
    vm_file_mkdir(wpath);                          // Sukuriamas katalogas

    // Konvertuoti UCS2 → ASCII
    vm_ucs2_to_ascii(folder_ascii, sizeof(folder_ascii), wpath);

    // Inicializuoti ZIP skaitymą
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_mem(&zip_archive, zip_data, zip_size, 0)) {
        vm_free(zip_data);
        return -5;
    }

    num_files = (VMINT)mz_zip_reader_get_num_files(&zip_archive);

    //
    // 1. Sukuriam visus katalogus, įskaitant tuščius
    //
    for (i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;

        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            char dir_path[260];
            vm_sprintf(dir_path, "%s\\%s", folder_ascii, file_stat.m_filename);
            normalize_path_slashes(dir_path);

            // Pašalinti galinį backslash
            int len = strlen(dir_path);
            if (len > 0 && (dir_path[len - 1] == '\\' || dir_path[len - 1] == '/')) {
                dir_path[len - 1] = '\0';
            }

            mkdir_recursive_ascii(dir_path);
        }
    }

    //
    // 2. Išskleisti visus failus
    //
    for (i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;

        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue;

        size_t file_size;
        void *file_data = mz_zip_reader_extract_to_heap(&zip_archive, i, &file_size, 0);
        if (!file_data) continue;

        // Pilnas ASCII kelias
        char full_ascii_path[260];
        vm_sprintf(full_ascii_path, "%s\\%s", folder_ascii, file_stat.m_filename);
        normalize_path_slashes(full_ascii_path);

        // Sukuriam poaplankius jei reikia
        char temp_ascii_dir[260];
        strcpy(temp_ascii_dir, full_ascii_path);
        char *last_slash = strrchr(temp_ascii_dir, '\\');
        if (last_slash) {
            *last_slash = '\0';
            mkdir_recursive_ascii(temp_ascii_dir);
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

void normalize_path_slashes(char *path) {

    int i;

    for (i = 0; path[i]; i++) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }
}

void mkdir_recursive_ascii(const char *path) {

    char partial[260];
    int len = strlen(path);
    int i, j = 0;

    for (i = 0; i <= len; i++) {
        if (path[i] == '\\' || path[i] == '/' || path[i] == '\0') {
            if (j > 0) {
                partial[j] = '\0';
                VMWCHAR wtemp[260];
                vm_ascii_to_ucs2(wtemp, sizeof(wtemp), partial);
                vm_file_mkdir(wtemp);
            }
            partial[j++] = path[i];
        } else {
            partial[j++] = path[i];
        }
    }
}

int compress_file_to_zip(VMWSTR zip_path) {

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
    file = vm_file_open(zip_path, MODE_READ, FALSE);
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
    vm_get_filename(zip_path, w_my_file_name);
    vm_ucs2_to_ascii(my_file_name, sizeof(my_file_name), w_my_file_name);

    if (!mz_zip_writer_add_mem(&zip, my_file_name, file_data, file_size, MZ_BEST_COMPRESSION)) {
        mz_zip_writer_end(&zip);
        vm_free(file_data);
        return -6;
    }

    // 4. Užbaigti archyvą ir gauti ZIP duomenis
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
    VMINT len = vm_wstrlen(zip_path);
    vm_wstrncpy(autoFullPathName, zip_path, len - 3);  // pašalinam pvz. ".txt"
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

void get_relative_zip_path(VMWSTR base_path, VMWSTR full_path, char *relative_ascii) {

    char base_ascii[260];
    char full_ascii[260];

    vm_ucs2_to_ascii(base_ascii, sizeof(base_ascii), base_path);
    vm_ucs2_to_ascii(full_ascii, sizeof(full_ascii), full_path);
    int i;

    int base_len = strlen(base_ascii);
    if (strncmp(base_ascii, full_ascii, base_len) == 0) {
        const char *relative = full_ascii + base_len;
        if (*relative == '\\' || *relative == '/')
            relative++;

        strncpy(relative_ascii, relative, 259);
        relative_ascii[259] = '\0';
    } else {
        // fallback jei nesutampa
        strcpy(relative_ascii, full_ascii);
    }

    // Konvertuoti visus '\' į '/'
    for (i = 0; relative_ascii[i]; i++) {
        if (relative_ascii[i] == '\\')
            relative_ascii[i] = '/';
    }
}

int add_files_to_zip_mem_recursive(mz_zip_archive *zip, VMWSTR base_path, VMWSTR current_path, VMUINT *total_size_ptr) {

    VMINT hnd;
    vm_fileinfo_ext fileInfo;
    VMWCHAR search_path[260], full_path[260];
    VMWCHAR dot1[6] = {0}, dot2[6] = {0};

    int found_any_file = 0; // Naudojama tik tuščių katalogų tikrinimui

    vm_ascii_to_ucs2(dot1, sizeof(dot1), ".");
    vm_ascii_to_ucs2(dot2, sizeof(dot2), "..");

    create_search_path(search_path, current_path, "\\*.*");
    hnd = vm_find_first_ext(search_path, &fileInfo);
    if (hnd < 0) return 0;

    do {
        if (vm_wstrcmp(fileInfo.filefullname, dot1) == 0 || vm_wstrcmp(fileInfo.filefullname, dot2) == 0)
            continue;

        found_any_file = 1;

        vm_wstrcpy(full_path, current_path);
        append_backslash_to_path(full_path);
        vm_wstrcat(full_path, fileInfo.filefullname);

        if (fileInfo.attributes & VM_FS_ATTR_DIR) {
            // Rekursyviai apdorojam katalogą
            add_files_to_zip_mem_recursive(zip, base_path, full_path, total_size_ptr);
        } else {

            VMFILE file = vm_file_open(full_path, MODE_READ, FALSE);
            if (file < 0) continue;

            if (fileInfo.filesize == 0 || fileInfo.filesize > MAX_TOTAL_SIZE) {
                vm_file_close(file);
                continue;
            }

            char *file_data = vm_malloc(fileInfo.filesize);
            if (!file_data) {
                vm_file_close(file);
                continue;
            }

            VMUINT read_size = 0;
            VMINT read_result = vm_file_read(file, file_data, fileInfo.filesize, &read_size);
            vm_file_close(file);

            if (read_size != fileInfo.filesize) {
                vm_free(file_data);
                continue;
            }

            char zip_name[260];
            get_relative_zip_path(base_path, full_path, zip_name);
            if (strlen(zip_name) == 0) {
                vm_free(file_data);
                continue;
            }

            if (!mz_zip_writer_add_mem(zip, zip_name, file_data, read_size, MZ_BEST_COMPRESSION)) {
                vm_free(file_data);
                continue;
            }

            *total_size_ptr += read_size;
            vm_free(file_data);
        }

    } while (vm_find_next_ext(hnd, &fileInfo) == 0);

    vm_find_close_ext(hnd);

    // Jei nieko nerasta kataloge – įtraukiam jį kaip tuščią į ZIP
    if (!found_any_file) {
        char zip_name[260];
        get_relative_zip_path(base_path, current_path, zip_name);
        if (strlen(zip_name) > 0 && zip_name[strlen(zip_name) - 1] != '/') {
            strcat(zip_name, "/");
        }

        mz_zip_writer_add_mem(zip, zip_name, NULL, 0, MZ_BEST_COMPRESSION); // 0 baitų tuščias katalogas
    }

    return 0;
}



int compress_directory_to_zip(VMWSTR dir_path) {

    mz_zip_archive zip;
    char dir_ascii[260], zip_filename_ascii[260];
    VMWCHAR zip_filename_ucs2[260];
    void *zip_buffer = NULL;
    size_t zip_size = 0;
    VMFILE file;
    VMUINT written, total_size = 0;

    vm_ucs2_to_ascii(dir_ascii, sizeof(dir_ascii), dir_path);

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        return -1;
    }

    if (add_files_to_zip_mem_recursive(&zip, dir_path, dir_path, &total_size) != 0) {
        mz_zip_writer_end(&zip);
        return -2;
    }

    if (total_size > MAX_TOTAL_SIZE) {
        mz_zip_writer_end(&zip);
        return -3;
    }

    if (!mz_zip_writer_finalize_heap_archive(&zip, &zip_buffer, &zip_size)) {
        mz_zip_writer_end(&zip);
        return -4;
    }

    if (!zip_buffer || zip_size == 0) {
        mz_zip_writer_end(&zip);
        return -5;
    }

    strcpy(zip_filename_ascii, dir_ascii);
    strcat(zip_filename_ascii, ".zip");
    vm_ascii_to_ucs2(zip_filename_ucs2, sizeof(zip_filename_ucs2), zip_filename_ascii);

    file = vm_file_open(zip_filename_ucs2, MODE_CREATE_ALWAYS_WRITE, FALSE);
    if (file < 0) {
        mz_zip_writer_end(&zip);
        mz_free(zip_buffer);
        return -6;
    }

    vm_file_write(file, zip_buffer, (VMUINT)zip_size, &written);
    vm_file_close(file);

    mz_zip_writer_end(&zip);
    mz_free(zip_buffer);

    return 0;
}

void create_search_path(VMWSTR result, VMWSTR source, VMSTR text) {

    VMWCHAR wtext[100];
    vm_ascii_to_ucs2(wtext, (strlen(text) + 1) * 2, text); // pvz., "*.*" arba "\\"
    vm_wstrcpy(result, source);
    vm_wstrcat(result, wtext);
}

// Naudojama vietoj L'\\'
void append_backslash_to_path(VMWSTR dest) {

    VMWCHAR backslash[2];
    vm_ascii_to_ucs2(backslash, sizeof(backslash), "\\");
    vm_wstrcat(dest, backslash);
}

int decompress_gzip_file(VMWSTR input_path) {

    VMFILE file_handle;
    VMUINT file_size, read_size, written_size;
    uint8_t *gzip_data = NULL;
    uint8_t *decompressed_data = NULL;
    mz_stream stream;
    char debug_buf[256] = {0};
    int result = -1;
    VMCHAR extension_name[12] = {0};
    VMWCHAR output_path[260] = {0};
    char filename_ascii[260] = {0};
    char n_without_ext[260] = {0};

    // 1. Atidarome GZIP failą
    file_handle = vm_file_open(input_path, MODE_READ, TRUE);
    if (file_handle < 0) return -1;

    if (vm_file_getfilesize(file_handle, &file_size) != 0 || file_size < 18) {
        vm_file_close(file_handle);
        return -2;
    }

    gzip_data = vm_malloc(file_size);
    if (!gzip_data) {
        vm_file_close(file_handle);
        return -3;
    }

    vm_file_read(file_handle, gzip_data, file_size, &read_size);
    vm_file_close(file_handle);
    if (read_size != file_size) {
        vm_free(gzip_data);
        return -4;
    }

    // 2. Patikriname GZIP magic bytes
    if (!(gzip_data[0] == 0x1F && gzip_data[1] == 0x8B && gzip_data[2] == 0x08)) {
        vm_free(gzip_data);
        return -5;
    }

    // 3. Nustatome kur prasideda suspausti duomenys
    uint8_t flg = gzip_data[3];
    int header_offset = 10;

    if (flg & 0x04) { // FEXTRA
        uint16_t xlen = gzip_data[header_offset] | (gzip_data[header_offset + 1] << 8);
        header_offset += 2 + xlen;
    }
    if (flg & 0x08) { // FNAME
        while (gzip_data[header_offset++] != 0);
    }
    if (flg & 0x10) { // FCOMMENT
        while (gzip_data[header_offset++] != 0);
    }
    if (flg & 0x02) { // FHCRC
        header_offset += 2;
    }

    if (header_offset >= (file_size - 8)) {
        vm_free(gzip_data);
        return -6;
    }

    mz_ulong compressed_size = file_size - header_offset - 8; // be CRC ir ISIZE
    uint8_t *compressed_ptr = gzip_data + header_offset;

    // 4. Sukuriame buferį išpakavimui
    decompressed_data = vm_malloc(MAX_UNCOMPRESS_SIZE);
    if (!decompressed_data) {
        vm_free(gzip_data);
        return -7;
    }

    // 5. Naudojame apeitą DEFLATE išpakavimą
    memset(&stream, 0, sizeof(stream));
    stream.next_in = compressed_ptr;
    stream.avail_in = compressed_size;
    stream.next_out = decompressed_data;
    stream.avail_out = MAX_UNCOMPRESS_SIZE;

    if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) {
       vm_free(gzip_data);
        vm_free(decompressed_data);
        return -8;
    }

    int status = mz_inflate(&stream, MZ_FINISH);
    mz_inflateEnd(&stream);

    if (status != MZ_STREAM_END) {
        vm_free(gzip_data);
        vm_free(decompressed_data);
        return -9;
    }

    // 7. Sukuriame pilną išvesties kelią

    vm_ucs2_to_ascii(filename_ascii, sizeof(filename_ascii), input_path);

    extract_end_text(extension_name, filename_ascii, '.');

    strncpy(n_without_ext, filename_ascii, strlen(filename_ascii) - strlen(extension_name));
    n_without_ext[strlen(filename_ascii) - strlen(extension_name)] = '\0';

    strcat(n_without_ext, "bin");

    vm_ascii_to_ucs2(output_path, sizeof(output_path), n_without_ext);

    // 6. Įrašome rezultatą
    file_handle = vm_file_open(output_path, MODE_CREATE_ALWAYS_WRITE, TRUE);

    if (file_handle >= 0) {
        vm_file_write(file_handle, decompressed_data, stream.total_out, &written_size);
        vm_file_close(file_handle);
        result = 0;
    }

    vm_free(gzip_data);
    vm_free(decompressed_data);

    return result;
}

void extract_end_text(char* result_data, const char* inp_data, char separator) {

    int i = 0;
    int last_sep_pos = -1;

    while (inp_data[i] != '\0') {
        if (inp_data[i] == separator) {
            last_sep_pos = i;
        }
        i++;
    }

    // Copy characters after the last separator
    i = 0;
    int j = last_sep_pos + 1;
    while (inp_data[j] != '\0') {
        result_data[i++] = inp_data[j++];
    }
    result_data[i] = '\0';  // Null-terminate the string
}