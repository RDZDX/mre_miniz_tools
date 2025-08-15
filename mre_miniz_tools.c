#include "mre_miniz_tools.h"

#define MAX_TOTAL_SIZE (3 * 1024 * 1024) // 2 MB

VMBOOL trigeris = VM_FALSE;

void _sbrk(){}
void _write(){}
void _close(){}
void _lseek(){}
void _open(){}
void _read(){}
void _exit(int status) {
	vm_exit_app();
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

    //int unzip_res = unzip_all_files_to_folder(file_path); 		//RAM
    //int unzip_res = compress_file_to_zip(file_path); 			//RAM
    //int unzip_res = decompress_gzip_file(file_path); 			//RAM
    //int unzip_res = compress_directory_to_zip(dir_pathx); 		//RAM

    //int unzip_res = decompress_gzip_file_stream(file_path); 		//BLOCK
    int unzip_res = compress_file_to_zip_stream(file_path); 		//BLOCK

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


void append_backslash_to_path(VMWSTR dest) { // Naudojama vietoj L'\\'

    VMWCHAR backslash[2];
    vm_ascii_to_ucs2(backslash, sizeof(backslash), "\\");
    vm_wstrcat(dest, backslash);
}


int decompress_gzip_file_stream(VMWSTR input_path) {
    VMFILE in_file, out_file;
    VMUINT read_size, written_size;
    uint8_t in_buf[4096];
    uint8_t out_buf[4096];
    mz_stream stream;
    char debug_buf[256];
    int result = -1;
    uint32_t stored_crc, stored_size, calc_crc = 0;
    char out_filename_ascii[260] = {0};
    char asciipath[260] = {0};
    VMWCHAR out_filename_ucs2[260] = {0};
    VMWCHAR wpath[260] = {0};

    vm_get_path(input_path, wpath);
    vm_ucs2_to_ascii(asciipath, sizeof(asciipath), wpath);

    // Atidarom įvesties failą
    in_file = vm_file_open(input_path, MODE_READ, TRUE);
    if (in_file < 0) return -1;

    // Skaitom GZIP antraštę (10 baitų)
    vm_file_read(in_file, in_buf, 10, &read_size);
    if (read_size != 10 || in_buf[0] != 0x1F || in_buf[1] != 0x8B || in_buf[2] != 0x08) {
        vm_file_close(in_file);
        return -2;
    }

    uint8_t flg = in_buf[3];

    // Skip FEXTRA
    if (flg & 0x04) {
        uint8_t extra_len_buf[2];
        vm_file_read(in_file, extra_len_buf, 2, &read_size);
        uint16_t xlen = extra_len_buf[0] | (extra_len_buf[1] << 8);
        vm_file_seek(in_file, xlen, BASE_CURR);
    }

    // FNAME
    if (flg & 0x08) {
        int pos = 0;
        char fname_ascii[260];
        do {
            vm_file_read(in_file, &fname_ascii[pos], 1, &read_size);
        } while (read_size == 1 && fname_ascii[pos++] != 0 && pos < sizeof(fname_ascii) - 1);
        fname_ascii[pos - 1] = 0;
        strcpy(out_filename_ascii, asciipath);
        strcat(out_filename_ascii, fname_ascii);
    }

    // FCOMMENT
    if (flg & 0x10) {
        uint8_t tmp;
        do {
            vm_file_read(in_file, &tmp, 1, &read_size);
        } while (read_size == 1 && tmp != 0);
    }

    // FHCRC
    if (flg & 0x02) {
        vm_file_seek(in_file, 2, BASE_CURR);
    }

    // Jei FNAME neegzistavo
    if (out_filename_ascii[0] == 0) {
        vm_ucs2_to_ascii(out_filename_ascii, sizeof(out_filename_ascii), input_path);
        char *gz_pos = strstr(out_filename_ascii, ".gz");
        if (gz_pos) *gz_pos = '\0';
    }

    vm_ascii_to_ucs2(out_filename_ucs2, sizeof(out_filename_ucs2), out_filename_ascii);

    out_file = vm_file_open(out_filename_ucs2, MODE_CREATE_ALWAYS_WRITE, TRUE);
    if (out_file < 0) {
        vm_file_close(in_file);
        return -3;
    }

    // Init inflate
    memset(&stream, 0, sizeof(stream));
    if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) {
        vm_file_close(in_file);
        vm_file_close(out_file);
        return -4;
    }

    int rc;
    do {
        vm_file_read(in_file, in_buf, sizeof(in_buf), &read_size);
        if (read_size == 0) break; // EOF

        stream.next_in = in_buf;
        stream.avail_in = read_size;

        while (stream.avail_in > 0) {
            stream.next_out = out_buf;
            stream.avail_out = sizeof(out_buf);

            rc = mz_inflate(&stream, MZ_NO_FLUSH);
            if (rc < 0) {
                break;
            }

            int have_out = sizeof(out_buf) - stream.avail_out;
            if (have_out > 0) {
                vm_file_write(out_file, out_buf, have_out, &written_size);
                calc_crc = mz_crc32(calc_crc, out_buf, have_out);
            }

            if (rc == MZ_STREAM_END) break;
        }
    } while (rc != MZ_STREAM_END);

    mz_inflateEnd(&stream);

    // Skaityti CRC + ISIZE
    vm_file_seek(in_file, -8, BASE_END);
    uint8_t tail[8];
    vm_file_read(in_file, tail, 8, &read_size);
    stored_crc  = tail[0] | (tail[1] << 8) | (tail[2] << 16) | (tail[3] << 24);
    stored_size = tail[4] | (tail[5] << 8) | (tail[6] << 16) | (tail[7] << 24);

    vm_file_close(in_file);
    vm_file_close(out_file);

    return (stored_crc == calc_crc) ? 0 : -5;
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

int compress_file_to_zip_stream(VMWSTR src_file_path) {

    mz_zip_archive zip;
    VMWCHAR wzip_path[260], wfile_name[128], wfile_ext[12];
    char ascii_file_name[128];
    char zip_ascii[260];

    /* 1. Sugeneruojame ZIP failo kelią */
    VMINT len = vm_wstrlen(src_file_path);
    vm_wstrncpy(wzip_path, src_file_path, len - 3); // pašalinam ".txt" arba kitą plėtinį
    wzip_path[len - 3] = 0;
    vm_ascii_to_ucs2(wfile_ext, sizeof(wfile_ext), "zip");
    vm_wstrcat(wzip_path, wfile_ext);
    vm_ucs2_to_ascii(zip_ascii, sizeof(zip_ascii), wzip_path);

    /* 2. Inicializuojame ZIP rašytoją, kuris rašys tiesiai į failą */
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, zip_ascii, 0)) {
        return -1;
    }

    /* 3. Failo vardas ZIP viduje */
    vm_get_filename(src_file_path, wfile_name);
    vm_ucs2_to_ascii(ascii_file_name, sizeof(ascii_file_name), wfile_name);

    /* 4. Pridedame failą į ZIP (miniz pats skaitys blokais) */
    char src_ascii[260];
    vm_ucs2_to_ascii(src_ascii, sizeof(src_ascii), src_file_path);

    if (!mz_zip_writer_add_file(&zip, ascii_file_name, src_ascii, NULL, 0,
                                MZ_BEST_COMPRESSION)) {
        mz_zip_writer_end(&zip);
        return -2;
    }

    /* 5. Užbaigiame ZIP */
    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);

    return 0;
}
