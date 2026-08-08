#include "md_catalog_service.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE) && \
    CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

namespace {

constexpr char kTag[] = "MdCatalog";
constexpr char kCatalogRoot[] = "/sdcard/roms/md";
constexpr char kCatalogTsv[] = "/sdcard/roms/md/catalog.tsv";
constexpr size_t kLineBytes = 1024;
constexpr size_t kMaxTsvFields = 12;

bool IsRegularFile(const char* path) {
    struct stat info = {};
    return path && stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

bool IsDirectory(const char* path) {
    struct stat info = {};
    return path && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool JoinPath(char* output, size_t output_size, const char* parent,
              const char* child) {
    if (!output || output_size == 0 || !parent || !child) {
        return false;
    }
    const size_t parent_length = strlen(parent);
    const size_t child_length = strlen(child);
    if (parent_length + 1 + child_length + 1 > output_size) {
        return false;
    }
    memcpy(output, parent, parent_length);
    output[parent_length] = '/';
    memcpy(output + parent_length + 1, child, child_length);
    output[parent_length + 1 + child_length] = '\0';
    return true;
}

bool HasSupportedExtension(const char* path) {
    if (!path) {
        return false;
    }
    const char* dot = strrchr(path, '.');
    return dot && (strcasecmp(dot, ".md") == 0 || strcasecmp(dot, ".gen") == 0 ||
                   strcasecmp(dot, ".bin") == 0 || strcasecmp(dot, ".smd") == 0 ||
                   strcasecmp(dot, ".zip") == 0);
}

bool IsSafeRelativePath(const char* path) {
    if (!path || path[0] == '\0' || path[0] == '/' || strchr(path, '\\') ||
        strchr(path, ':')) {
        return false;
    }
    const char* component = path;
    while (*component) {
        const char* slash = strchr(component, '/');
        const size_t length = slash ? static_cast<size_t>(slash - component)
                                    : strlen(component);
        if (length == 0 || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.')) {
            return false;
        }
        if (!slash) {
            break;
        }
        component = slash + 1;
    }
    return true;
}

void DeriveTitle(const char* relative_path, char* output, size_t output_size) {
    if (!output || output_size == 0) {
        return;
    }
    const char* name = relative_path ? strrchr(relative_path, '/') : nullptr;
    name = name ? name + 1 : relative_path;
    snprintf(output, output_size, "%s", name ? name : "MD Game");
    char* dot = strrchr(output, '.');
    if (dot) {
        *dot = '\0';
    }
    for (char* cursor = output; *cursor; ++cursor) {
        if (*cursor == '_') {
            *cursor = ' ';
        }
    }
}

bool ParseTsvField(char** cursor, char** field) {
    if (!cursor || !*cursor || !field || **cursor == '\0' ||
        **cursor == '\r' || **cursor == '\n') {
        return false;
    }
    char* read = *cursor;
    char* write = read;
    if (*read == '"') {
        ++read;
        write = *cursor;
        while (*read) {
            if (*read == '"') {
                if (read[1] == '"') {
                    *write++ = '"';
                    read += 2;
                    continue;
                }
                ++read;
                break;
            }
            *write++ = *read++;
        }
    } else {
        while (*read && *read != '\t' && *read != '\r' && *read != '\n') {
            *write++ = *read++;
        }
    }
    char* next = read;
    while (*next && *next != '\t' && *next != '\r' && *next != '\n') {
        ++next;
    }
    if (*next == '\t') {
        ++next;
    }
    *write = '\0';
    *field = *cursor;
    *cursor = next;
    return true;
}

size_t ParseTsvLine(char* line, char** fields, size_t capacity) {
    if (!line || !fields || capacity == 0) {
        return 0;
    }
    if (static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        memmove(line, line + 3, strlen(line + 3) + 1);
    }
    char* cursor = line;
    size_t count = 0;
    while (count < capacity && ParseTsvField(&cursor, &fields[count])) {
        ++count;
    }
    return count;
}

int FindTsvColumn(char** fields, size_t count, const char* name) {
    for (size_t index = 0; index < count; ++index) {
        if (fields[index] && strcasecmp(fields[index], name) == 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

}  // namespace

MdCatalogService::~MdCatalogService() {
    Clear();
}

esp_err_t MdCatalogService::Load() {
    Clear();
    if (!IsDirectory(kCatalogRoot)) {
        ESP_LOGW(kTag, "catalog directory missing: %s", kCatalogRoot);
        return ESP_ERR_NOT_FOUND;
    }

    entries_ = static_cast<MdCatalogEntry*>(heap_caps_calloc(
        kMaxEntries, sizeof(MdCatalogEntry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!entries_) {
        ESP_LOGE(kTag, "PSRAM catalog allocation failed bytes=%u",
                 static_cast<unsigned>(kMaxEntries * sizeof(MdCatalogEntry)));
        return ESP_ERR_NO_MEM;
    }

    const bool loaded_tsv = LoadTsv();
    if (!loaded_tsv) {
        ScanDirectory(kCatalogRoot, "roms/md", "MD", 1);
    }
    if (count_ == 0) {
        Clear();
        return ESP_ERR_NOT_FOUND;
    }

    std::sort(entries_, entries_ + count_, [](const MdCatalogEntry& left,
                                               const MdCatalogEntry& right) {
        return strcasecmp(left.title, right.title) < 0;
    });
    ESP_LOGI(kTag, "loaded %u games source=%s truncated=%d",
             static_cast<unsigned>(count_), loaded_tsv ? "catalog.tsv" : "scan",
             truncated_ ? 1 : 0);
    return ESP_OK;
}

void MdCatalogService::Clear() {
    if (entries_) {
        heap_caps_free(entries_);
    }
    entries_ = nullptr;
    count_ = 0;
    truncated_ = false;
}

const MdCatalogEntry* MdCatalogService::Entry(size_t index) const {
    return entries_ && index < count_ ? &entries_[index] : nullptr;
}

bool MdCatalogService::LoadTsv() {
    FILE* file = fopen(kCatalogTsv, "rb");
    if (!file) {
        return false;
    }
    char* line = static_cast<char*>(
        heap_caps_malloc(kLineBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!line) {
        fclose(file);
        return false;
    }

    int path_column = -1;
    int title_column = -1;
    int category_column = -1;
    bool columns_resolved = false;
    while (fgets(line, kLineBytes, file)) {
        char* fields[kMaxTsvFields] = {};
        const size_t field_count = ParseTsvLine(line, fields, kMaxTsvFields);
        if (field_count == 0 || !fields[0] || fields[0][0] == '\0' ||
            fields[0][0] == '#') {
            continue;
        }

        if (!columns_resolved) {
            path_column = FindTsvColumn(fields, field_count, "filename");
            if (path_column < 0) {
                path_column = FindTsvColumn(fields, field_count, "path");
            }
            if (path_column < 0) {
                path_column = FindTsvColumn(fields, field_count, "relative_path");
            }
            title_column = FindTsvColumn(fields, field_count, "title");
            category_column = FindTsvColumn(fields, field_count, "category");
            if (path_column >= 0 && title_column >= 0) {
                columns_resolved = true;
                continue;
            }

            // Also accept compact headerless rows: path, title, category.
            // The older test-pack format is id, filename, title, category, ...
            if (field_count >= 3 && HasSupportedExtension(fields[0])) {
                path_column = 0;
                title_column = 1;
                category_column = 2;
            } else if (field_count >= 4 && HasSupportedExtension(fields[1])) {
                path_column = 1;
                title_column = 2;
                category_column = 3;
            } else {
                continue;
            }
            columns_resolved = true;
        }

        if (path_column < 0 || title_column < 0 ||
            static_cast<size_t>(path_column) >= field_count ||
            static_cast<size_t>(title_column) >= field_count) {
            continue;
        }
        const char* catalog_path = fields[path_column];
        const char* title = fields[title_column];
        const char* category = category_column >= 0 &&
                                       static_cast<size_t>(category_column) < field_count
                                   ? fields[category_column]
                                   : "MD";
        char relative_path[sizeof(MdCatalogEntry::relative_path)]{};
        char absolute_path[sizeof(relative_path) + 16]{};
        if (!IsSafeRelativePath(catalog_path) ||
            !HasSupportedExtension(catalog_path)) {
            continue;
        }
        if (strncmp(catalog_path, "roms/md/", 8) == 0) {
            snprintf(relative_path, sizeof(relative_path), "%s", catalog_path);
        } else {
            snprintf(relative_path, sizeof(relative_path), "roms/md/%s", catalog_path);
        }
        snprintf(absolute_path, sizeof(absolute_path), "/sdcard/%s", relative_path);
        if (!IsRegularFile(absolute_path) &&
            strncmp(catalog_path, "roms/", 5) != 0) {
            snprintf(relative_path, sizeof(relative_path), "roms/md/roms/%s",
                     catalog_path);
            snprintf(absolute_path, sizeof(absolute_path), "/sdcard/%s", relative_path);
        }
        if (!IsRegularFile(absolute_path)) {
            continue;
        }
        AddEntry(relative_path, title, category);
    }

    heap_caps_free(line);
    fclose(file);
    return count_ > 0;
}

void MdCatalogService::ScanDirectory(const char* absolute_dir,
                                     const char* relative_dir,
                                     const char* category,
                                     unsigned depth) {
    DIR* directory = opendir(absolute_dir);
    if (!directory) {
        return;
    }
    while (dirent* item = readdir(directory)) {
        if (item->d_name[0] == '.' || strcmp(item->d_name, "catalog.tsv") == 0) {
            continue;
        }
        char absolute_path[256]{};
        char relative_path[sizeof(MdCatalogEntry::relative_path)]{};
        if (!JoinPath(absolute_path, sizeof(absolute_path), absolute_dir,
                      item->d_name) ||
            !JoinPath(relative_path, sizeof(relative_path), relative_dir,
                      item->d_name)) {
            ESP_LOGW(kTag, "skipping overlong catalog path");
            continue;
        }

        if (IsRegularFile(absolute_path) && HasSupportedExtension(item->d_name)) {
            AddEntry(relative_path, nullptr, category);
        } else if (depth > 0 && IsDirectory(absolute_path) &&
                   IsSafeRelativePath(item->d_name)) {
            ScanDirectory(absolute_path, relative_path, item->d_name, depth - 1);
        }
        if (count_ >= kMaxEntries) {
            truncated_ = true;
            break;
        }
    }
    closedir(directory);
}

bool MdCatalogService::AddEntry(const char* relative_path, const char* title,
                                const char* category) {
    if (!entries_ || !relative_path || !IsSafeRelativePath(relative_path) ||
        strncmp(relative_path, "roms/md/", 8) != 0 ||
        !HasSupportedExtension(relative_path)) {
        return false;
    }
    for (size_t index = 0; index < count_; ++index) {
        if (strcmp(entries_[index].relative_path, relative_path) == 0) {
            return true;
        }
    }
    if (count_ >= kMaxEntries) {
        truncated_ = true;
        return false;
    }

    MdCatalogEntry& entry = entries_[count_];
    snprintf(entry.relative_path, sizeof(entry.relative_path), "%s", relative_path);
    if (title && title[0]) {
        snprintf(entry.title, sizeof(entry.title), "%s", title);
    } else {
        DeriveTitle(relative_path, entry.title, sizeof(entry.title));
    }
    snprintf(entry.category, sizeof(entry.category), "%s",
             category && category[0] ? category : "MD");
    ++count_;
    return true;
}

#endif  // CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE
