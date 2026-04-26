#ifndef MOC_H
#define MOC_H
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t usize;

#define MEMORY_TABLE(X) \
    X(B, 1ULL, "Byte") \
    X(KB, 1024ULL, "KiB") \
    X(MB, (1024ULL*1024), "MiB") \
    X(GB, (1024ULL*1024*1024), "GiB")

#define AS_ENUM(type, amount, name) MEM_##type,
typedef enum { MEMORY_TABLE(AS_ENUM) } mem_size_type;
#undef AS_ENUM

typedef struct {
    usize amount;
    mem_size_type unit;
} MocMem;

#define AS_CONSTRUCTOR_SIZE(type, q, name) \
    static inline MocMem type(usize amount) { return (MocMem){.amount = (usize)(amount * q), .unit = MEM_##type}; }
MEMORY_TABLE(AS_CONSTRUCTOR_SIZE)

typedef enum {
    MOC_OPT_NONE = 0,
    MOC_OPT_BALANCED = 1,
    MOC_OPT_AGGRESSIVE = 2,
    MOC_OPT_EXTREME = 3,
    MOC_OPT_SIZE = 4
} MocOptimizationLevel;

typedef enum {
    MOC_CACHE_OFF = 0,
    MOC_CACHE_ON = 1,
    MOC_CACHE_VERBOSE = 2
} MocCacheMode;

typedef enum {
    MOC_INFO,
    MOC_DEBUG,
    MOC_WARN,
    MOC_ERROR,
    MOC_FATAL,
    MOC_VG       /* valgrind-specific output, special formatting */
} MOC_LOG_TYPE;

void moc_log_print(MOC_LOG_TYPE type, const char* msg, ...) __attribute__((weak));

#define MOC_LOG_INFO(msg, ...)  moc_log_print(MOC_INFO,  msg, ##__VA_ARGS__)
#define MOC_LOG_DEBUG(msg, ...) moc_log_print(MOC_DEBUG, msg, ##__VA_ARGS__)
#define MOC_LOG_WARN(msg, ...)  moc_log_print(MOC_WARN,  msg, ##__VA_ARGS__)
#define MOC_LOG_ERROR(msg, ...) moc_log_print(MOC_ERROR, msg, ##__VA_ARGS__)
#define MOC_LOG_FATAL(msg, ...) moc_log_print(MOC_FATAL, msg, ##__VA_ARGS__)
#define MOC_LOG_VG(msg, ...)    moc_log_print(MOC_VG,    msg, ##__VA_ARGS__)

typedef struct Moc Moc;

Moc* moc_begin(MocMem arena_cap, usize max_args);
void moc_end(Moc** moc_ref);

void moc_preset_standard_c(Moc* moc, const char* src_dir, const char* include_dir,
                           const char* out_dir, const char* exec_name);
void moc_preset_cpp(Moc* moc, const char* src_dir, const char* include_dir,
                    const char* out_dir, const char* exec_name);
void moc_set_compiler(Moc* moc, const char* compiler_name);

#define moc_add_flags(moc, ...) _moc_add_flags_impl(moc, (const char*[]){__VA_ARGS__, NULL})
void _moc_add_flags_impl(Moc* moc, const char* flags[]);

#define moc_add_link_flags(moc, ...) _moc_add_link_flags_impl(moc, (const char*[]){__VA_ARGS__, NULL})
void _moc_add_link_flags_impl(Moc* moc, const char* flags[]);

void moc_add_include(Moc* moc, const char* include_dir);
void moc_add_source(Moc* moc, const char* src_pattern);
void moc_add_watch(Moc* moc, const char* pattern);
void moc_set_output(Moc* moc, const char* out_dir, const char* exec_name);
void moc_add_library(Moc* moc, const char* lib_name);
void moc_add_library_path(Moc* moc, const char* lib_path);
void moc_set_optimization(Moc* moc, MocOptimizationLevel level);
void moc_enable_sanitizer(Moc* moc, bool enable);
void moc_set_cache_mode(Moc* moc, MocCacheMode mode);

/* Exclude a source file (or pattern) from the build.
 * Call AFTER moc_add_source / preset, since it filters the already-collected list.
 * Path resolution matches what moc_add_source did, so use the same form
 * (e.g. "./source/main.c"). Wildcards work too: moc_exclude_source(m, "./source/test_*.c")
 * Calling moc_exclude_source for a file that was never added is a no-op. */
void moc_exclude_source(Moc* moc, const char* path_or_pattern);

/* Watch tuning: how long (in milliseconds) to wait before retrying a build
 * after a failed one. Default: 2000 ms. Set to 0 to retry immediately on
 * every save. Useful to silence the console while you're mid-edit. */
void moc_set_watch_error_backoff_ms(Moc* moc, unsigned ms);

/* Watch tuning: how often to poll for file changes (in milliseconds).
 * Default: 500 ms. Lower = more responsive but more CPU. */
void moc_set_watch_poll_interval_ms(Moc* moc, unsigned ms);

void moc_watch_run_after_build(Moc* moc, bool enable);
void moc_dispatch(Moc* moc, int argc, char** argv);

const char* moc_get_output_path(Moc* moc);
const char* moc_get_compiler(Moc* moc);
usize moc_get_source_count(Moc* moc);
void moc_clear_cache(Moc* moc);

#ifdef __cplusplus
}
#endif

#endif

#ifdef moc_builder

#ifndef MOC_IMPLEMENTATION_GUARD
#define MOC_IMPLEMENTATION_GUARD

#define _GNU_SOURCE
#include <string.h>
#include <glob.h>
#include <fnmatch.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <ctype.h>

#define PATH_MAX 4096
#define cast(type, ref) ((type)ref)

typedef unsigned char byte;

#define ALIGNMENT_64 64LU
#define ALIGNMENT_REG ((usize)sizeof(void*))

#define ANSI_CLEAR_SCREEN  "\x1b[2J\x1b[H"
#define ANSI_RESET         "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"
#define ANSI_DIM           "\x1b[2m"
#define ANSI_GRAY          "\x1b[90m"
#define ANSI_GREEN         "\x1b[1;32m"
#define ANSI_CYAN          "\x1b[1;36m"
#define ANSI_YELLOW        "\x1b[1;33m"
#define ANSI_RED           "\x1b[1;31m"
#define ANSI_MAGENTA       "\x1b[1;35m"
#define ANSI_BLUE          "\x1b[1;34m"

typedef struct {
    uint64_t timestamp;
    uint64_t file_size;
    uint32_t checksum;
} MocFileInfo;

typedef struct {
    uint64_t last_build_time;
    uint32_t num_files;
    MocFileInfo files[];
} MocCacheHeader;

static inline usize alignto(usize number, usize alignment) {
    return ((number + (alignment - 1)) & ~(alignment - 1));
}

typedef struct {
    MocMem moc_mem;
    usize used;
    byte* start;
} MocBlob;

#define BLOB_SIZE sizeof(MocBlob)
#define BLOB_SIZE_A64 alignto(BLOB_SIZE, ALIGNMENT_64)

typedef struct { usize offset; usize length; } MocString;
typedef struct { usize capacity; usize count; MocString args[]; } MocCommand;
typedef struct { MocString build; MocString output; MocString include; } MocDirs;

struct Moc {
    MocBlob* blob;
    MocCommand* compile_flags;
    MocCommand* link_flags;
    MocCommand* sources;
    MocCommand* objects;
    MocCommand* watch_files;
    MocCommand* active_cmd;
    MocCommand* libraries;
    MocCommand* lib_paths;
    MocDirs* dirs;
    const char* compiler;
    usize target_dir_offset;
    usize target_exec_offset;
    usize obj_dir_offset;
    bool run_after_build;
    MocOptimizationLevel opt_level;
    MocCacheMode cache_mode;
    bool sanitizer;
    char* cache_file_path;
    usize arena_config_mark;
    bool  arena_marked;
    unsigned watch_error_backoff_ms;
    unsigned watch_poll_ms;
};

void moc_log_print(MOC_LOG_TYPE type, const char* msg, ...) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    static const char* log_styles[] = {
        ANSI_GREEN, ANSI_CYAN, ANSI_YELLOW, ANSI_RED, ANSI_MAGENTA, ANSI_BLUE
    };
    static const char* log_labels[] = { "INFO", "DEBUG", "WARN", "ERROR", "FATAL", "  VG" };

    fprintf(stderr, ANSI_GRAY "[%02d:%02d:%02d]" ANSI_RESET " %s%5s:" ANSI_RESET " ",
        t->tm_hour, t->tm_min, t->tm_sec, log_styles[type], log_labels[type]);

    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static bool moc_report_child_status(int status, const char* what) {
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0) return true;
        MOC_LOG_ERROR("%s exited with code %d", what, code);
        return false;
    }
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        const char* signame = strsignal(sig);
        MOC_LOG_ERROR("%s killed by signal %d (%s)",
                      what, sig, signame ? signame : "unknown");
        return false;
    }
    MOC_LOG_ERROR("%s terminated abnormally", what);
    return false;
}

static inline usize blob_remaining_space(MocBlob* blob) {
    return blob->moc_mem.amount - blob->used;
}

static void* alloc_from_blob(MocBlob* blob, usize to_alloc) {
    usize aligned_to_alloc = alignto(to_alloc, ALIGNMENT_REG);
    if (blob_remaining_space(blob) < aligned_to_alloc) {
        MOC_LOG_FATAL("Arena exhausted: tried to allocate %zu bytes (used %zu of %zu). "
                      "Increase the size passed to moc_begin().",
                      aligned_to_alloc, blob->used, blob->moc_mem.amount);
        exit(1);
    }
    void* data_start = cast(void*, blob->start + blob->used);
    blob->used += aligned_to_alloc;
    return data_start;
}

static void new_moc_blob(MocBlob** out_blob, MocMem size) {
    usize aligned_data_size = alignto(size.amount, ALIGNMENT_64);
    usize aligned_blob_size = BLOB_SIZE_A64;
    usize total_alloc = aligned_data_size + aligned_blob_size;

    MocBlob* blob = (MocBlob*)aligned_alloc(ALIGNMENT_64, total_alloc);
    if (!blob) {
        fprintf(stderr, "System out of memory.\n");
        exit(1);
    }

    blob->moc_mem.amount = aligned_data_size;
    blob->moc_mem.unit = size.unit;
    blob->used = 0;
    blob->start = cast(byte*, blob) + aligned_blob_size;
    memset(cast(void*, blob->start), 0, aligned_data_size);
    *out_blob = blob;
}

static void free_moc_blob(MocBlob** blob_ref) {
    if (!blob_ref || !(*blob_ref)) return;
    MocBlob* blob = *blob_ref;
    *blob_ref = NULL;
    free(blob);
}

void ensure_directory(const char* dir) {
    if (!dir || strlen(dir) == 0) return;
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            struct stat st = {0};
            if (stat(tmp, &st) == -1) mkdir(tmp, 0777);
            *p = sep;
        }
    }
    struct stat st = {0};
    if (stat(tmp, &st) == -1) mkdir(tmp, 0777);
}

static MocCommand* new_moc_command(MocBlob* blob, usize max_args) {
    usize cmd_header_size = sizeof(MocCommand);
    usize strings_size = sizeof(MocString) * max_args;
    usize total = alignto(cmd_header_size + strings_size, ALIGNMENT_64);

    MocCommand* cmd = cast(MocCommand*, alloc_from_blob(blob, total));
    cmd->capacity = max_args;
    cmd->count = 0;
    return cmd;
}

char* moc_path_join(MocBlob* blob, const char* string_1, const char* string_2) {
    usize len_1 = strlen(string_1);
    usize len_2 = strlen(string_2);
    usize total = len_1 + len_2 + 2;
    char* concat = cast(char*, alloc_from_blob(blob, total));
    memcpy(concat, string_1, len_1);
    if (len_1 > 0 && string_1[len_1-1] != '/' && len_2 > 0 && string_2[0] != '/') {
        concat[len_1] = '/';
        memcpy(concat + len_1 + 1, string_2, len_2 + 1);
    } else {
        memcpy(concat + len_1, string_2, len_2 + 1);
    }
    return concat;
}

static inline bool moc_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    while (*prefix) if (*prefix++ != *str++) return false;
    return true;
}

static char* resolve_and_alloc_path(Moc* moc, const char* path) {
    if (moc_starts_with(path, "./")) {
        path += 2;
        char cwd_buffer[PATH_MAX];
        if (!getcwd(cwd_buffer, PATH_MAX)) {
            MOC_LOG_FATAL("Failed to retrieve working directory.");
            exit(1);
        }
        return moc_path_join(moc->blob, cwd_buffer, path);
    }
    usize len = strlen(path);
    char* dest = cast(char*, alloc_from_blob(moc->blob, len + 1));
    memcpy(dest, path, len + 1);
    return dest;
}

static void cmd_list_push(Moc* moc, MocCommand* cmd_list, const char* text) {
    if (cmd_list->count >= cmd_list->capacity) {
        MOC_LOG_FATAL("Command list capacity (%zu) exceeded. Increase max_args in moc_begin().",
                      cmd_list->capacity);
        exit(1);
    }
    usize text_len = strlen(text);
    char* dest = cast(char*, alloc_from_blob(moc->blob, text_len + 1));
    memcpy(dest, text, text_len + 1);

    cmd_list->args[cmd_list->count].offset = cast(byte*, dest) - moc->blob->start;
    cmd_list->args[cmd_list->count].length = text_len;
    cmd_list->count++;
}

static void cmd_clear(Moc* moc) {
    if (moc && moc->active_cmd) moc->active_cmd->count = 0;
}

static void cmd_push(Moc* moc, const char* text) {
    cmd_list_push(moc, moc->active_cmd, text);
}

/* === EXCLUDE: filter the sources/watch lists ============================ */

static void filter_command(Moc* moc, MocCommand* list, const char* abs_pattern) {
    usize write = 0;
    for (usize read = 0; read < list->count; read++) {
        const char* p = cast(const char*, moc->blob->start + list->args[read].offset);
        bool matches =
            (strcmp(p, abs_pattern) == 0) ||
            (fnmatch(abs_pattern, p, 0) == 0);
        if (!matches) {
            list->args[write++] = list->args[read];
        }
    }
    list->count = write;
}

void moc_exclude_source(Moc* moc, const char* path_or_pattern) {
    if (!moc || !path_or_pattern) return;
    char* abs = resolve_and_alloc_path(moc, path_or_pattern);
    usize before = moc->sources->count;
    filter_command(moc, moc->sources, abs);
    filter_command(moc, moc->watch_files, abs);
    usize removed = before - moc->sources->count;
    if (removed > 0) {
        MOC_LOG_INFO("Excluded %zu source file(s) matching '%s'", removed, path_or_pattern);
    } else {
        MOC_LOG_WARN("No source matched exclude pattern: '%s'", path_or_pattern);
    }
}

void moc_set_watch_error_backoff_ms(Moc* moc, unsigned ms) {
    if (moc) moc->watch_error_backoff_ms = ms;
}

void moc_set_watch_poll_interval_ms(Moc* moc, unsigned ms) {
    if (moc) moc->watch_poll_ms = (ms == 0) ? 100 : ms;
}

/* === Object name and exec ============================================== */

static char* generate_obj_name(Moc* moc, const char* source_path, const char* obj_dir) {
    const char* base_name = strrchr(source_path, '/');
#if defined(_WIN32) || defined(_WIN64)
    const char* base_name_win = strrchr(source_path, '\\');
    if (base_name_win > base_name) base_name = base_name_win;
#endif
    if (!base_name) base_name = source_path;
    else base_name++;

    const char* dot = strrchr(base_name, '.');
    usize name_len = dot ? (usize)(dot - base_name) : strlen(base_name);

    char* new_name = cast(char*, alloc_from_blob(moc->blob, name_len + 3));
    memcpy(new_name, base_name, name_len);
    new_name[name_len] = '.';
    new_name[name_len + 1] = 'o';
    new_name[name_len + 2] = '\0';

    return moc_path_join(moc->blob, obj_dir, new_name);
}

static char** argv_from_active_cmd(Moc* moc) {
    usize count = moc->active_cmd->count;
    usize total_size = (count + 1) * sizeof(char*);
    char** argv = cast(char**, alloc_from_blob(moc->blob, total_size));

    for (usize i = 0; i < count; i++) {
        argv[i] = cast(char*, moc->blob->start + moc->active_cmd->args[i].offset);
    }
    argv[count] = NULL;
    return argv;
}

static bool moc_exec_cmd(Moc* moc) {
    if (!moc || !moc->active_cmd || moc->active_cmd->count == 0) return false;

    char** argv = argv_from_active_cmd(moc);

    pid_t pid = fork();
    if (pid < 0) {
        MOC_LOG_FATAL("Failed to fork process: %s", strerror(errno));
        exit(1);
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return moc_report_child_status(status, argv[0]);
}

/* === Cache ============================================================= */

static uint32_t simple_checksum(const byte* data, usize len) {
    uint32_t hash = 5381;
    for (usize i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

static uint64_t get_file_timestamp(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (uint64_t)st.st_mtime;
    }
    return 0;
}

static uint64_t get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (uint64_t)st.st_size;
    }
    return 0;
}

static uint32_t get_file_checksum(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fclose(f);
        return 0;
    }

    byte* buffer = malloc((usize)len);
    if (!buffer) {
        fclose(f);
        return 0;
    }

    fread(buffer, 1, (usize)len, f);
    uint32_t checksum = simple_checksum(buffer, (usize)len);

    free(buffer);
    fclose(f);
    return checksum;
}

static char* get_cache_path(Moc* moc) {
    char* out_dir = cast(char*, moc->blob->start + moc->target_dir_offset);
    usize len = strlen(out_dir) + 32;
    char* cache_path = cast(char*, alloc_from_blob(moc->blob, len));
    snprintf(cache_path, len, "%s/.moc_cache", out_dir);
    return cache_path;
}

static bool is_cache_valid(Moc* moc) {
    if (moc->cache_mode == MOC_CACHE_OFF) return false;

    char* cache_path = get_cache_path(moc);
    FILE* cache_file = fopen(cache_path, "rb");
    if (!cache_file) return false;

    MocCacheHeader header;
    if (fread(&header, sizeof(MocCacheHeader), 1, cache_file) != 1) {
        fclose(cache_file);
        return false;
    }

    if (header.num_files != moc->watch_files->count) {
        fclose(cache_file);
        return false;
    }

    bool valid = true;
    for (usize i = 0; i < moc->watch_files->count; i++) {
        char* file = cast(char*, moc->blob->start + moc->watch_files->args[i].offset);
        MocFileInfo info;

        if (fread(&info, sizeof(MocFileInfo), 1, cache_file) != 1) {
            valid = false;
            break;
        }

        uint64_t current_timestamp = get_file_timestamp(file);
        uint64_t current_size = get_file_size(file);

        if (current_timestamp != info.timestamp || current_size != info.file_size) {
            valid = false;
            break;
        }
    }

    fclose(cache_file);
    return valid;
}

static void save_cache(Moc* moc) {
    if (moc->cache_mode == MOC_CACHE_OFF) return;

    char* cache_path = get_cache_path(moc);
    FILE* cache_file = fopen(cache_path, "wb");
    if (!cache_file) {
        MOC_LOG_WARN("Failed to create cache file: %s", cache_path);
        return;
    }

    MocCacheHeader header;
    header.last_build_time = (uint64_t)time(NULL);
    header.num_files = moc->watch_files->count;

    fwrite(&header, sizeof(MocCacheHeader), 1, cache_file);

    for (usize i = 0; i < moc->watch_files->count; i++) {
        char* file = cast(char*, moc->blob->start + moc->watch_files->args[i].offset);
        MocFileInfo info;
        info.timestamp = get_file_timestamp(file);
        info.file_size = get_file_size(file);
        info.checksum = get_file_checksum(file);
        fwrite(&info, sizeof(MocFileInfo), 1, cache_file);
    }

    fclose(cache_file);
}

void moc_clear_cache(Moc* moc) {
    if (!moc) return;
    char* cache_path = get_cache_path(moc);
    if (remove(cache_path) == 0) {
        MOC_LOG_INFO("Cache cleared");
    } else {
        MOC_LOG_INFO("No cache to clear");
    }
}

/* === Build ============================================================= */

static const char* opt_level_to_flag(MocOptimizationLevel level) {
    switch(level) {
        case MOC_OPT_NONE: return "-O0";
        case MOC_OPT_BALANCED: return "-O1";
        case MOC_OPT_AGGRESSIVE: return "-O2";
        case MOC_OPT_EXTREME: return "-O3";
        case MOC_OPT_SIZE: return "-Os";
        default: return "-O0";
    }
}

static void moc_setup_optimization(Moc* moc) {
    static bool setup_done = false;
    if (setup_done) return;
    setup_done = true;

    if (moc->opt_level != MOC_OPT_NONE) {
        const char* opt_flag = opt_level_to_flag(moc->opt_level);
        cmd_list_push(moc, moc->compile_flags, opt_flag);
    }

    if (moc->sanitizer) {
        cmd_list_push(moc, moc->compile_flags, "-fsanitize=address");
        cmd_list_push(moc, moc->link_flags, "-fsanitize=address");
    }
}

static void moc_freeze_config(Moc* moc) {
    if (moc->arena_marked) return;
    moc->arena_config_mark = moc->blob->used;
    moc->arena_marked = true;
}

static void moc_rewind_arena(Moc* moc) {
    if (!moc->arena_marked) return;
    moc->blob->used = moc->arena_config_mark;
    moc->objects->count = 0;
}

static const char* get_current_working_dir(void) {
    static char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return ".";
    }
    return cwd;
}

static void moc_generate_compile_commands(Moc* moc) {
    char* compile_commands_path = moc_path_join(moc->blob, ".", "compile_commands.json");

    FILE* json_file = fopen(compile_commands_path, "w");
    if (!json_file) {
        MOC_LOG_ERROR("Failed to create compile_commands.json");
        return;
    }

    fprintf(json_file, "[\n");

    for (usize i = 0; i < moc->sources->count; i++) {
        char* src = cast(char*, moc->blob->start + moc->sources->args[i].offset);
        char temp_obj_dir[PATH_MAX];
        snprintf(temp_obj_dir, sizeof(temp_obj_dir), "%s/obj", get_current_working_dir());
        char* obj = generate_obj_name(moc, src, temp_obj_dir);

        fprintf(json_file, "  {\n");
        fprintf(json_file, "    \"directory\": \"%s\",\n", get_current_working_dir());
        fprintf(json_file, "    \"file\": \"%s\",\n", src);
        fprintf(json_file, "    \"output\": \"%s\",\n", obj);
        fprintf(json_file, "    \"arguments\": [\n");
        fprintf(json_file, "      \"%s\",\n", moc->compiler);

        for (usize j = 0; j < moc->compile_flags->count; j++) {
            char* flag = cast(char*, moc->blob->start + moc->compile_flags->args[j].offset);
            fprintf(json_file, "      \"%s\",\n", flag);
        }

        fprintf(json_file, "      \"-c\",\n");
        fprintf(json_file, "      \"%s\"\n", src);
        fprintf(json_file, "    ]\n");
        fprintf(json_file, "  }%s\n", (i < moc->sources->count - 1) ? "," : "");
    }

    fprintf(json_file, "]\n");
    fclose(json_file);

    MOC_LOG_INFO("Generated compile_commands.json with %zu entries", moc->sources->count);
}

static void moc_generate_clangd_config(Moc* moc) {
    char* clangd_path = moc_path_join(moc->blob, ".", ".clangd");

    FILE* clangd_file = fopen(clangd_path, "w");
    if (!clangd_file) {
        MOC_LOG_ERROR("Failed to create .clangd");
        return;
    }

    fprintf(clangd_file, "CompileFlags:\n");
    fprintf(clangd_file, "  Add: [\n");

    for (usize j = 0; j < moc->compile_flags->count; j++) {
        char* flag = cast(char*, moc->blob->start + moc->compile_flags->args[j].offset);
        fprintf(clangd_file, "    \"%s\",\n", flag);
    }

    fprintf(clangd_file, "  ]\n");
    fprintf(clangd_file, "Diagnostics:\n");
    fprintf(clangd_file, "  UnusedIncludes: Strict\n");
    fprintf(clangd_file, "  MissingIncludes: Strict\n");

    fclose(clangd_file);
    MOC_LOG_INFO("Generated .clangd configuration");
}

static bool moc_build_project(Moc* moc) {
    if (moc->sources->count == 0) {
        MOC_LOG_WARN("No source files found.");
        return false;
    }
    if (moc->obj_dir_offset == 0 || moc->target_exec_offset == 0) {
        MOC_LOG_FATAL("Output directory not set.");
        return false;
    }

    moc_freeze_config(moc);

    if (is_cache_valid(moc)) {
        MOC_LOG_INFO("Up to date (cached)");
        moc_rewind_arena(moc);
        return true;
    }

    if (moc->cache_mode != MOC_CACHE_OFF) {
        MOC_LOG_INFO("Changes detected, rebuilding...");
    }

    char* obj_dir = cast(char*, moc->blob->start + moc->obj_dir_offset);

    moc->objects->count = 0;

    bool success = true;
    struct timespec build_start, build_end;
    clock_gettime(CLOCK_MONOTONIC, &build_start);

    for (usize i = 0; i < moc->sources->count; i++) {
        char* src = cast(char*, moc->blob->start + moc->sources->args[i].offset);
        char* obj = generate_obj_name(moc, src, obj_dir);

        cmd_list_push(moc, moc->objects, obj);

        cmd_clear(moc);
        cmd_push(moc, moc->compiler);

        for (usize j = 0; j < moc->compile_flags->count; j++) {
            cmd_push(moc, cast(char*, moc->blob->start + moc->compile_flags->args[j].offset));
        }

        cmd_push(moc, "-c");
        cmd_push(moc, src);
        cmd_push(moc, "-o");
        cmd_push(moc, obj);

        MOC_LOG_INFO("Compiling: %s", src);

        if (!moc_exec_cmd(moc)) {
            MOC_LOG_ERROR("Compilation failed: %s", src);
            success = false;
            goto cleanup;
        }
    }

    cmd_clear(moc);
    cmd_push(moc, moc->compiler);

    for (usize j = 0; j < moc->objects->count; j++) {
        cmd_push(moc, cast(char*, moc->blob->start + moc->objects->args[j].offset));
    }

    for (usize j = 0; j < moc->lib_paths->count; j++) {
        cmd_push(moc, cast(char*, moc->blob->start + moc->lib_paths->args[j].offset));
    }

    for (usize j = 0; j < moc->libraries->count; j++) {
        cmd_push(moc, cast(char*, moc->blob->start + moc->libraries->args[j].offset));
    }

    for (usize j = 0; j < moc->link_flags->count; j++) {
        cmd_push(moc, cast(char*, moc->blob->start + moc->link_flags->args[j].offset));
    }

    cmd_push(moc, "-o");
    cmd_push(moc, cast(char*, moc->blob->start + moc->target_exec_offset));

    MOC_LOG_INFO("Linking executable...");
    if (!moc_exec_cmd(moc)) {
        MOC_LOG_ERROR("Linking failed.");
        success = false;
        goto cleanup;
    }

    save_cache(moc);

    clock_gettime(CLOCK_MONOTONIC, &build_end);
    double elapsed_ms = (build_end.tv_sec - build_start.tv_sec) * 1000.0
                      + (build_end.tv_nsec - build_start.tv_nsec) / 1e6;
    MOC_LOG_INFO("Build successful (%.0fms, arena: %zu/%zu bytes)",
                 elapsed_ms, moc->blob->used, moc->blob->moc_mem.amount);

cleanup:
    moc_rewind_arena(moc);
    return success;
}

/* === Begin/End ========================================================= */

Moc* moc_begin(MocMem blob_capacity, usize max_args) {
    MocBlob* blob = NULL;
    new_moc_blob(&blob, blob_capacity);

    Moc* m = cast(Moc*, alloc_from_blob(blob, sizeof(struct Moc)));
    m->blob = blob;

    m->active_cmd    = new_moc_command(blob, max_args);
    m->compile_flags = new_moc_command(blob, max_args);
    m->link_flags    = new_moc_command(blob, max_args);
    m->sources       = new_moc_command(blob, max_args);
    m->objects       = new_moc_command(blob, max_args);
    m->watch_files   = new_moc_command(blob, max_args);
    m->libraries     = new_moc_command(blob, max_args);
    m->lib_paths     = new_moc_command(blob, max_args);

    m->dirs = NULL;
    m->compiler = "gcc";
    m->target_dir_offset = 0;
    m->target_exec_offset = 0;
    m->obj_dir_offset = 0;
    m->run_after_build = false;
    m->opt_level = MOC_OPT_NONE;
    m->cache_mode = MOC_CACHE_ON;
    m->sanitizer = false;
    m->cache_file_path = NULL;
    m->arena_config_mark = 0;
    m->arena_marked = false;
    m->watch_error_backoff_ms = 2000;
    m->watch_poll_ms = 500;

    return m;
}

void moc_end(Moc** moc_ref) {
    if (!moc_ref || !(*moc_ref)) return;
    Moc* moc = *moc_ref;

    if (moc->blob) {
      free_moc_blob(&moc->blob);
    }

    *moc_ref = NULL;
}

void moc_watch_run_after_build(Moc* moc, bool enable) {
    if (moc) moc->run_after_build = enable;
}

void moc_set_compiler(Moc* moc, const char* compiler_name) {
    usize len = strlen(compiler_name);
    char* dest = cast(char*, alloc_from_blob(moc->blob, len + 1));
    memcpy(dest, compiler_name, len + 1);
    moc->compiler = dest;
}

void _moc_add_flags_impl(Moc* moc, const char* flags[]) {
    for (usize i = 0; flags[i] != NULL; i++) {
        cmd_list_push(moc, moc->compile_flags, flags[i]);
    }
}

void _moc_add_link_flags_impl(Moc* moc, const char* flags[]) {
    for (usize i = 0; flags[i] != NULL; i++) {
        cmd_list_push(moc, moc->link_flags, flags[i]);
    }
}

void moc_add_include(Moc* moc, const char* include_dir) {
    if (!include_dir) return;
    char* abs_dir = resolve_and_alloc_path(moc, include_dir);
    usize total_len = strlen(abs_dir) + 3;
    char* flag = cast(char*, alloc_from_blob(moc->blob, total_len));
    snprintf(flag, total_len, "-I%s", abs_dir);
    cmd_list_push(moc, moc->compile_flags, flag);
}

void moc_add_source(Moc* moc, const char* src_pattern) {
    char* abs_pattern = resolve_and_alloc_path(moc, src_pattern);
    glob_t g;
    if (glob(abs_pattern, GLOB_TILDE, NULL, &g) == 0) {
        for (usize i = 0; i < g.gl_pathc; i++) {
            cmd_list_push(moc, moc->sources, g.gl_pathv[i]);
            cmd_list_push(moc, moc->watch_files, g.gl_pathv[i]);
        }
    }
    globfree(&g);
}

void moc_add_watch(Moc* moc, const char* pattern) {
    char* abs_pattern = resolve_and_alloc_path(moc, pattern);
    glob_t g;
    if (glob(abs_pattern, GLOB_TILDE, NULL, &g) == 0) {
        for (usize i = 0; i < g.gl_pathc; i++) {
            cmd_list_push(moc, moc->watch_files, g.gl_pathv[i]);
        }
    }
    globfree(&g);
}

void moc_set_output(Moc* moc, const char* out_dir, const char* exec_name) {
    char* abs_out_dir = resolve_and_alloc_path(moc, out_dir);
    ensure_directory(abs_out_dir);

    char* obj_dir = moc_path_join(moc->blob, abs_out_dir, "obj/");
    ensure_directory(obj_dir);

    char* final_out = moc_path_join(moc->blob, abs_out_dir, exec_name);

    moc->target_dir_offset = cast(byte*, abs_out_dir) - moc->blob->start;
    moc->obj_dir_offset    = cast(byte*, obj_dir)     - moc->blob->start;
    moc->target_exec_offset= cast(byte*, final_out)   - moc->blob->start;
}

void moc_add_library(Moc* moc, const char* lib_name) {
    char lib_flag[256];
    snprintf(lib_flag, sizeof(lib_flag), "-l%s", lib_name);
    cmd_list_push(moc, moc->libraries, lib_flag);
}

void moc_add_library_path(Moc* moc, const char* lib_path) {
    char* abs_path = resolve_and_alloc_path(moc, lib_path);
    char flag[PATH_MAX + 3];
    snprintf(flag, sizeof(flag), "-L%s", abs_path);
    cmd_list_push(moc, moc->lib_paths, flag);
}

void moc_set_optimization(Moc* moc, MocOptimizationLevel level) {
    if (moc) moc->opt_level = level;
}

void moc_enable_sanitizer(Moc* moc, bool enable) {
    if (moc) moc->sanitizer = enable;
}

void moc_set_cache_mode(Moc* moc, MocCacheMode mode) {
    if (moc) moc->cache_mode = mode;
}

void moc_preset_standard_c(Moc* moc, const char* src_dir, const char* include_dir,
                           const char* out_dir, const char* exec_name) {
    moc_set_compiler(moc, "gcc");
    moc_add_flags(moc, "-Wall", "-Wextra", "-std=c11", "-g");
    moc_add_include(moc, include_dir);
    moc_set_optimization(moc, MOC_OPT_NONE);

    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s/*.c", src_dir);
    moc_add_source(moc, pattern);

    char h_pattern[1024];
    snprintf(h_pattern, sizeof(h_pattern), "%s/*.h", include_dir);
    moc_add_watch(moc, h_pattern);

    moc_set_output(moc, out_dir, exec_name);
}

void moc_preset_cpp(Moc* moc, const char* src_dir, const char* include_dir,
                    const char* out_dir, const char* exec_name) {
    moc_set_compiler(moc, "g++");
    moc_add_flags(moc, "-Wall", "-Wextra", "-std=c++17", "-g");
    moc_add_include(moc, include_dir);
    moc_set_optimization(moc, MOC_OPT_NONE);

    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s/*.cpp", src_dir);
    moc_add_source(moc, pattern);

    char h_pattern[1024];
    snprintf(h_pattern, sizeof(h_pattern), "%s/*.hpp", include_dir);
    moc_add_watch(moc, h_pattern);

    char hh_pattern[1024];
    snprintf(hh_pattern, sizeof(hh_pattern), "%s/*.h", include_dir);
    moc_add_watch(moc, hh_pattern);

    moc_set_output(moc, out_dir, exec_name);
}

const char* moc_get_output_path(Moc* moc) {
    if (!moc || moc->target_exec_offset == 0) return NULL;
    return cast(char*, moc->blob->start + moc->target_exec_offset);
}

const char* moc_get_compiler(Moc* moc) {
    return moc ? moc->compiler : NULL;
}

usize moc_get_source_count(Moc* moc) {
    return moc ? moc->sources->count : 0;
}

/* === Run/spawn helpers ================================================== */

static void moc_spawn_target(Moc* moc, const char* label) {
    char* target = cast(char*, moc->blob->start + moc->target_exec_offset);
    MOC_LOG_INFO("%s: %s", label, target);

    pid_t pid = fork();
    if (pid < 0) {
        MOC_LOG_ERROR("fork failed: %s", strerror(errno));
        return;
    }
    if (pid == 0) {
        char* target_argv[] = {target, NULL};
        execvp(target, target_argv);
        perror("execvp");
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    moc_report_child_status(status, target);
}

/* === Valgrind: run with output filtering ================================ */

typedef enum {
    VG_LINE_NOISE,    /* startup banner, command echo */
    VG_LINE_INFO,     /* informational */
    VG_LINE_LEAK,     /* leak summary lines */
    VG_LINE_ERROR,    /* "Invalid read", "definitely lost", etc */
    VG_LINE_STACK,    /* "at 0x...:" or "by 0x...:" */
    VG_LINE_SUMMARY,  /* ERROR SUMMARY, LEAK SUMMARY, HEAP SUMMARY */
    VG_LINE_PROGRAM   /* line from the actual program (no ==PID==) */
} VgLineKind;

/* Returns pointer to the content after the "==PID== " prefix, or NULL if
 * the line doesn't have that prefix. */
static const char* skip_vg_prefix(const char* line) {
    if (line[0] != '=' || line[1] != '=') return NULL;
    const char* p = line + 2;
    while (isdigit((unsigned char)*p)) p++;
    if (p[0] != '=' || p[1] != '=') return NULL;
    p += 2;
    while (*p == ' ') p++;
    return p;
}

static VgLineKind classify_vg_line(const char* line, const char** out_content) {
    const char* content = skip_vg_prefix(line);
    if (!content) {
        *out_content = line;
        return VG_LINE_PROGRAM;
    }
    *out_content = content;

    if (strstr(content, "ERROR SUMMARY")) return VG_LINE_SUMMARY;
    if (strstr(content, "LEAK SUMMARY"))  return VG_LINE_SUMMARY;
    if (strstr(content, "HEAP SUMMARY"))  return VG_LINE_SUMMARY;

    if (strstr(content, "definitely lost") ||
        strstr(content, "indirectly lost") ||
        strstr(content, "possibly lost")   ||
        strstr(content, "Invalid read")    ||
        strstr(content, "Invalid write")   ||
        strstr(content, "Conditional jump") ||
        strstr(content, "Use of uninitialised") ||
        strstr(content, "Address ") ||
        strstr(content, "Mismatched free")) {
        return VG_LINE_ERROR;
    }

    if (strstr(content, "still reachable") ||
        strstr(content, "suppressed:") ||
        strstr(content, "in use at exit") ||
        strstr(content, "total heap usage")) {
        return VG_LINE_LEAK;
    }

    if (strstr(content, "   at 0x") || strstr(content, "   by 0x")) {
        return VG_LINE_STACK;
    }

    if (strstr(content, "Memcheck") ||
        strstr(content, "Copyright") ||
        strstr(content, "Using Valgrind") ||
        strstr(content, "Command:") ||
        strstr(content, "Parent PID:") ||
        strstr(content, "For lists of detected") ||
        strstr(content, "For counts of detected") ||
        content[0] == '\0') {
        return VG_LINE_NOISE;
    }

    return VG_LINE_INFO;
}

static void print_vg_line(VgLineKind kind, const char* content) {
    switch (kind) {
        case VG_LINE_NOISE:
            break;
        case VG_LINE_PROGRAM:
            fprintf(stderr, "%s\n", content);
            break;
        case VG_LINE_ERROR:
            MOC_LOG_VG(ANSI_RED "%s" ANSI_RESET, content);
            break;
        case VG_LINE_LEAK:
            MOC_LOG_VG(ANSI_YELLOW "%s" ANSI_RESET, content);
            break;
        case VG_LINE_SUMMARY:
            MOC_LOG_VG(ANSI_BOLD "%s" ANSI_RESET, content);
            break;
        case VG_LINE_STACK:
            MOC_LOG_VG(ANSI_DIM "%s" ANSI_RESET, content);
            break;
        case VG_LINE_INFO:
        default:
            MOC_LOG_VG("%s", content);
            break;
    }
}

static int moc_run_valgrind(Moc* moc) {
    if (moc->opt_level != MOC_OPT_NONE) {
        MOC_LOG_WARN("Optimization is on (%s) - valgrind output may be less precise.",
                     opt_level_to_flag(moc->opt_level));
    }

    char* target = cast(char*, moc->blob->start + moc->target_exec_offset);
    MOC_LOG_INFO("Running under valgrind: %s", target);

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        MOC_LOG_ERROR("pipe failed: %s", strerror(errno));
        return 2;
    }

    pid_t pid = fork();
    if (pid < 0) {
        MOC_LOG_ERROR("fork failed: %s", strerror(errno));
        close(pipefd[0]); close(pipefd[1]);
        return 2;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char* vg_argv[] = {
            "valgrind",
            "--leak-check=full",
            "--show-leak-kinds=all",
            "--track-origins=yes",
            "--error-exitcode=1",
            target,
            NULL
        };
        execvp("valgrind", vg_argv);
        perror("execvp valgrind");
        _exit(127);
    }

    close(pipefd[1]);
    FILE* vg_out = fdopen(pipefd[0], "r");
    if (!vg_out) {
        MOC_LOG_ERROR("fdopen failed: %s", strerror(errno));
        close(pipefd[0]);
        int status; waitpid(pid, &status, 0);
        return 2;
    }

    char line[4096];
    while (fgets(line, sizeof(line), vg_out)) {
        usize len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        const char* content = NULL;
        VgLineKind kind = classify_vg_line(line, &content);
        print_vg_line(kind, content);
    }
    fclose(vg_out);

    int status;
    waitpid(pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 2;

    if (code == 0) {
        MOC_LOG_INFO(ANSI_GREEN "Valgrind: no errors detected." ANSI_RESET);
    } else if (code == 1) {
        MOC_LOG_ERROR("Valgrind: errors detected.");
    } else {
        MOC_LOG_ERROR("Valgrind itself exited with code %d.", code);
    }
    return code;
}

/* === Watch loop ========================================================= */

static time_t latest_mtime(Moc* moc) {
    time_t max = 0;
    for (usize i = 0; i < moc->watch_files->count; i++) {
        char* file = cast(char*, moc->blob->start + moc->watch_files->args[i].offset);
        struct stat st;
        if (stat(file, &st) == 0 && st.st_mtime > max) max = st.st_mtime;
    }
    return max;
}

static void sleep_ms(unsigned ms) {
    if (ms == 0) return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

typedef void (*MocOnBuildOk)(Moc* moc);

static void moc_run_watch_loop(Moc* moc, MocOnBuildOk on_success, const char* mode_label) {
    printf(ANSI_CLEAR_SCREEN);
    MOC_LOG_INFO("Watch mode: %s. Press Ctrl+C to stop.", mode_label);
    MOC_LOG_INFO("Watching %zu file(s).", moc->watch_files->count);

    time_t last_built_mtime = 0;
    bool first_build = true;
    bool last_build_failed = false;
    time_t error_throttle_until = 0;

    while (1) {
        time_t now = time(NULL);
        time_t max_mtime = latest_mtime(moc);

        bool changed = (max_mtime > last_built_mtime) || first_build;
        bool throttled = last_build_failed && now < error_throttle_until;

        if (changed && throttled) {
            sleep_ms(moc->watch_poll_ms);
            continue;
        }

        if (changed) {
            printf(ANSI_CLEAR_SCREEN);
            fflush(stdout);

            if (first_build) {
                MOC_LOG_INFO("Initial build...");
            } else if (last_build_failed) {
                MOC_LOG_INFO("Retrying build...");
            } else {
                MOC_LOG_INFO("Changes detected. Rebuilding...");
            }

            bool ok = moc_build_project(moc);
            first_build = false;

            if (ok) {
                last_built_mtime = max_mtime;
                last_build_failed = false;
                error_throttle_until = 0;
                if (on_success) on_success(moc);
            } else {
                last_build_failed = true;
                if (moc->watch_error_backoff_ms > 0) {
                    error_throttle_until = now + (moc->watch_error_backoff_ms / 1000) + 1;
                    MOC_LOG_WARN("Build failed. Pausing rebuilds for %u ms — fix and save again.",
                                 moc->watch_error_backoff_ms);
                }
            }
        }

        sleep_ms(moc->watch_poll_ms);
    }
}

static void on_build_ok_run(Moc* moc) {
    if (moc->run_after_build) moc_spawn_target(moc, "Running");
}

static void on_build_ok_valgrind(Moc* moc) {
    moc_run_valgrind(moc);
}

/* === Dispatcher ======================================================== */

static int parse_compound_cmd(const char* cmd, char* out_a, char* out_b, usize cap) {
    const char* sep = NULL;
    for (const char* p = cmd; *p; p++) {
        if (*p == '+' || *p == '&') { sep = p; break; }
    }
    if (!sep) return 0;
    usize a_len = (usize)(sep - cmd);
    if (a_len == 0 || a_len + 1 >= cap) return 0;
    memcpy(out_a, cmd, a_len);
    out_a[a_len] = '\0';
    snprintf(out_b, cap, "%s", sep + 1);
    return 1;
}

void moc_dispatch(Moc* moc, int argc, char** argv) {
    const char* cmd = (argc > 1) ? argv[1] : "build";

    char part_a[64] = {0}, part_b[64] = {0};
    bool compound = parse_compound_cmd(cmd, part_a, part_b, sizeof(part_a));
    bool wants_watch_valgrind = false;
    if (compound) {
        bool a_watch = (strcmp(part_a, "watch") == 0);
        bool b_watch = (strcmp(part_b, "watch") == 0);
        bool a_vg = (strcmp(part_a, "valgrind") == 0 || strcmp(part_a, "vg") == 0);
        bool b_vg = (strcmp(part_b, "valgrind") == 0 || strcmp(part_b, "vg") == 0);
        if ((a_watch && b_vg) || (a_vg && b_watch)) {
            wants_watch_valgrind = true;
        }
    }

    if (wants_watch_valgrind) {
        moc_setup_optimization(moc);
        moc_run_watch_loop(moc, on_build_ok_valgrind, "build + valgrind on every change");
        return;
    }

    if (strcmp(cmd, "build") == 0) {
        moc_setup_optimization(moc);
        moc_build_project(moc);
    }
    else if (strcmp(cmd, "run") == 0) {
        moc_setup_optimization(moc);
        if (moc_build_project(moc)) {
            moc_spawn_target(moc, "Executing");
        }
    }
    else if (strcmp(cmd, "valgrind") == 0 || strcmp(cmd, "vg") == 0) {
        moc_setup_optimization(moc);
        if (moc_build_project(moc)) {
            moc_run_valgrind(moc);
        }
    }
    else if (strcmp(cmd, "watch") == 0) {
        moc_setup_optimization(moc);
        moc_run_watch_loop(moc, on_build_ok_run, "rebuild on change");
    }
    else if (strcmp(cmd, "clean") == 0) {
        if (moc->target_dir_offset == 0) {
            MOC_LOG_WARN("Output directory not set.");
            return;
        }
        char* out_dir = cast(char*, moc->blob->start + moc->target_dir_offset);
        MOC_LOG_INFO("Cleaning directory: %s", out_dir);

        char rm_cmd[PATH_MAX + 32];
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", out_dir);
        int rc = system(rm_cmd);
        if (rc != 0) {
            MOC_LOG_WARN("rm exited with code %d", rc);
        }
        moc_clear_cache(moc);
        MOC_LOG_INFO("Clean successful.");
    }
    else if (strcmp(cmd, "info") == 0) {
        MOC_LOG_INFO("MOC Build System Info:");
        MOC_LOG_INFO("  Compiler: %s", moc->compiler);
        MOC_LOG_INFO("  Sources: %zu", moc->sources->count);
        MOC_LOG_INFO("  Watch files: %zu", moc->watch_files->count);
        MOC_LOG_INFO("  Compile flags: %zu", moc->compile_flags->count);
        MOC_LOG_INFO("  Link flags: %zu", moc->link_flags->count);
        MOC_LOG_INFO("  Libraries: %zu", moc->libraries->count);
        MOC_LOG_INFO("  Optimization: %s", opt_level_to_flag(moc->opt_level));
        MOC_LOG_INFO("  Cache mode: %s",
                    moc->cache_mode == MOC_CACHE_OFF ? "OFF" :
                    moc->cache_mode == MOC_CACHE_ON ? "ON" : "VERBOSE");
        MOC_LOG_INFO("  Sanitizer: %s", moc->sanitizer ? "ON" : "OFF");
        MOC_LOG_INFO("  Watch poll: %u ms", moc->watch_poll_ms);
        MOC_LOG_INFO("  Watch error backoff: %u ms", moc->watch_error_backoff_ms);
        MOC_LOG_INFO("  Arena: %zu / %zu bytes used", moc->blob->used, moc->blob->moc_mem.amount);
        if (moc->target_exec_offset) {
            MOC_LOG_INFO("  Output: %s", moc_get_output_path(moc));
        }
    }
    else if (strcmp(cmd, "cache-clear") == 0) {
        moc_clear_cache(moc);
    }
    else if (strcmp(cmd, "gen-cdb") == 0 || strcmp(cmd, "generate-compile-commands") == 0) {
        if (moc->sources->count == 0) {
            MOC_LOG_ERROR("No source files found. Please configure the project first.");
            return;
        }
        moc_generate_compile_commands(moc);
        moc_generate_clangd_config(moc);
        MOC_LOG_INFO("Done! You can now use clangd, clang-tidy, etc.");
    }
    else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        MOC_LOG_INFO("Usage: ./builder [command]");
        MOC_LOG_INFO("");
        MOC_LOG_INFO("Commands:");
        MOC_LOG_INFO("  build              Build the project (default).");
        MOC_LOG_INFO("  run                Build and execute the resulting binary.");
        MOC_LOG_INFO("  valgrind | vg      Build and run under valgrind, with filtered output.");
        MOC_LOG_INFO("  watch              Rebuild on changes; pauses on errors.");
        MOC_LOG_INFO("  watch+valgrind     Rebuild + run valgrind on every change.");
        MOC_LOG_INFO("                     (Aliases: 'watch+vg', 'vg+watch', 'valgrind+watch')");
        MOC_LOG_INFO("  clean              Remove the output directory and clear cache.");
        MOC_LOG_INFO("  info               Print current configuration.");
        MOC_LOG_INFO("  cache-clear        Clear only the build cache.");
        MOC_LOG_INFO("  gen-cdb            Generate compile_commands.json and .clangd.");
        MOC_LOG_INFO("  help               Show this help.");
    }
    else {
        MOC_LOG_ERROR("Unknown command: '%s'", cmd);
        MOC_LOG_INFO("Run './builder help' for usage.");
    }
}

#endif
#endif
