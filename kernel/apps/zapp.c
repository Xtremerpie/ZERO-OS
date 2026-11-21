/* ZERO KERNEL - Native Application Package Format (.zapp) Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "apps/zapp.h"
#include "zero/string.h"

static const uint32_t s_crc32_table[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

uint32_t zapp_calc_checksum(const uint8_t *data, size_t len)
{
    if (!data || len == 0) return 0;

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte & 0x0Fu)];
        crc = (crc >> 4) ^ s_crc32_table[(crc & 0x0Fu) ^ (byte >> 4)];
    }
    return ~crc;
}

zapp_err_t zapp_validate_path(const char *path)
{
    if (!path || path[0] == '\0') {
        return ZAPP_ERR_PATH_TRAVERSAL;
    }

    size_t len = strlen(path);
    if (len >= ZAPP_MAX_PATH) {
        return ZAPP_ERR_PATH_TRAVERSAL;
    }

    /* Guard: No leading root slash or backslash */
    if (path[0] == '/' || path[0] == '\\') {
        return ZAPP_ERR_PATH_TRAVERSAL;
    }

    /* Guard: Scan for path traversal ".." elements and Windows drive specifiers */
    for (size_t i = 0; i < len; i++) {
        char c = path[i];

        /* Disallow control characters and drive colons */
        if (c < 32 || c > 126 || c == ':') {
            return ZAPP_ERR_PATH_TRAVERSAL;
        }

        /* Check for ".." sequences */
        if (c == '.' && i + 1 < len && path[i + 1] == '.') {
            /* Check if preceded by start or slash */
            bool prev_sep = (i == 0 || path[i - 1] == '/' || path[i - 1] == '\\');
            /* Check if followed by end or slash */
            bool next_sep = (i + 2 == len || path[i + 2] == '/' || path[i + 2] == '\\');

            if (prev_sep && next_sep) {
                return ZAPP_ERR_PATH_TRAVERSAL;
            }
        }
    }

    return ZAPP_OK;
}

zapp_err_t zapp_validate_header(const zapp_header_t *hdr, size_t total_size)
{
    if (!hdr) return ZAPP_ERR_NULL_PTR;

    if (total_size < sizeof(zapp_header_t)) {
        return ZAPP_ERR_BAD_SIZE;
    }

    if (hdr->magic != ZAPP_MAGIC) {
        return ZAPP_ERR_BAD_MAGIC;
    }

    if (hdr->format_version != ZAPP_FORMAT_VER) {
        return ZAPP_ERR_BAD_VERSION;
    }

    if (strcmp(hdr->target_arch, "x86_64") != 0) {
        return ZAPP_ERR_BAD_ARCH;
    }

    if (hdr->entry_count > ZAPP_MAX_ENTRIES) {
        return ZAPP_ERR_BAD_SIZE;
    }

    /* Boundary checks */
    uint64_t manifest_end = (uint64_t)hdr->manifest_offset + hdr->manifest_size;
    if (manifest_end > total_size || hdr->manifest_size < sizeof(zapp_manifest_t)) {
        return ZAPP_ERR_OUT_OF_BOUNDS;
    }

    uint64_t entries_end = (uint64_t)hdr->entries_offset +
                           ((uint64_t)hdr->entry_count * sizeof(zapp_file_entry_t));
    if (entries_end > total_size) {
        return ZAPP_ERR_OUT_OF_BOUNDS;
    }

    uint64_t payload_end = (uint64_t)hdr->payload_offset + hdr->payload_size;
    if (payload_end > total_size) {
        return ZAPP_ERR_OUT_OF_BOUNDS;
    }

    return ZAPP_OK;
}

zapp_err_t zapp_validate_package(const uint8_t *pkg_bytes, size_t total_size,
                                 zapp_header_t *out_hdr, zapp_manifest_t *out_manifest)
{
    if (!pkg_bytes || total_size < sizeof(zapp_header_t)) {
        return ZAPP_ERR_BAD_SIZE;
    }

    const zapp_header_t *hdr = (const zapp_header_t *)pkg_bytes;
    zapp_err_t err = zapp_validate_header(hdr, total_size);
    if (err != ZAPP_OK) {
        return err;
    }

    /* Check payload integrity */
    const uint8_t *payload = pkg_bytes + hdr->payload_offset;
    uint32_t calc_sum = zapp_calc_checksum(payload, hdr->payload_size);
    if (hdr->checksum != 0 && calc_sum != hdr->checksum) {
        return ZAPP_ERR_CHECKSUM_MISMATCH;
    }

    /* Parse Manifest */
    const zapp_manifest_t *manifest = (const zapp_manifest_t *)(pkg_bytes + hdr->manifest_offset);
    if (out_hdr) {
        memcpy(out_hdr, hdr, sizeof(zapp_header_t));
    }
    if (out_manifest) {
        memcpy(out_manifest, manifest, sizeof(zapp_manifest_t));
    }

    /* Parse and validate file entries */
    const zapp_file_entry_t *entries = (const zapp_file_entry_t *)(pkg_bytes + hdr->entries_offset);
    bool found_exec = false;

    for (uint32_t i = 0; i < hdr->entry_count; i++) {
        const zapp_file_entry_t *e = &entries[i];

        /* Validate entry path */
        zapp_err_t perr = zapp_validate_path(e->path);
        if (perr != ZAPP_OK) {
            return perr;
        }

        /* Check offset bounds within payload */
        uint64_t file_end = (uint64_t)e->offset + e->size;
        if (file_end > hdr->payload_size) {
            return ZAPP_ERR_OUT_OF_BOUNDS;
        }

        /* Duplicate entry check */
        for (uint32_t j = i + 1; j < hdr->entry_count; j++) {
            if (strcmp(e->path, entries[j].path) == 0) {
                return ZAPP_ERR_DUPLICATE_ENTRY;
            }
        }

        /* Verify executable presence */
        if (strcmp(e->path, manifest->exec_path) == 0) {
            found_exec = true;
        }
    }

    if (manifest->exec_path[0] != '\0' && !found_exec) {
        return ZAPP_ERR_MISSING_EXEC;
    }

    return ZAPP_OK;
}

zapp_err_t zapp_get_file(const uint8_t *pkg_bytes, size_t pkg_size,
                         const char *file_path, const uint8_t **out_data, size_t *out_size)
{
    if (!pkg_bytes || !file_path || !out_data || !out_size) {
        return ZAPP_ERR_NULL_PTR;
    }

    zapp_header_t hdr;
    zapp_manifest_t manifest;
    zapp_err_t err = zapp_validate_package(pkg_bytes, pkg_size, &hdr, &manifest);
    if (err != ZAPP_OK) {
        return err;
    }

    const zapp_file_entry_t *entries = (const zapp_file_entry_t *)(pkg_bytes + hdr.entries_offset);
    const uint8_t *payload = pkg_bytes + hdr.payload_offset;

    for (uint32_t i = 0; i < hdr.entry_count; i++) {
        if (strcmp(entries[i].path, file_path) == 0) {
            *out_data = payload + entries[i].offset;
            *out_size = entries[i].size;
            return ZAPP_OK;
        }
    }

    return ZAPP_ERR_OUT_OF_BOUNDS;
}

const char *zapp_err_str(zapp_err_t err)
{
    switch (err) {
    case ZAPP_OK:                    return "Success";
    case ZAPP_ERR_NULL_PTR:          return "Null pointer argument";
    case ZAPP_ERR_BAD_MAGIC:         return "Invalid ZAPP magic header";
    case ZAPP_ERR_BAD_VERSION:       return "Unsupported ZAPP format version";
    case ZAPP_ERR_BAD_ARCH:          return "Architecture mismatch (must be x86_64)";
    case ZAPP_ERR_BAD_SIZE:          return "Package size corrupted or truncated";
    case ZAPP_ERR_CORRUPT_PAYLOAD:   return "Corrupted payload section";
    case ZAPP_ERR_PATH_TRAVERSAL:    return "Path traversal attempt detected (rejected)";
    case ZAPP_ERR_DUPLICATE_ENTRY:   return "Duplicate file entry in package";
    case ZAPP_ERR_MISSING_EXEC:      return "Declared executable not found in package";
    case ZAPP_ERR_CHECKSUM_MISMATCH: return "Payload CRC32 checksum mismatch";
    case ZAPP_ERR_OUT_OF_BOUNDS:     return "Entry exceeds package payload boundary";
    default:                         return "Unknown error";
    }
}
