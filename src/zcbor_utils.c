/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "golioth_sys.h"

LOG_TAG_DEFINE(zcbor_utils);

#include <errno.h>

#include "zcbor_utils.h"
#include "zcbor_any_skip_fixed.h"

static struct zcbor_map_entry* map_entry_get(
        struct zcbor_map_entry* entries,
        size_t num_entries,
        struct zcbor_map_key* key) {
    for (struct zcbor_map_entry* entry = entries; entry < &entries[num_entries]; entry++) {
        switch (entry->key.type) {
            case ZCBOR_MAP_KEY_TYPE_U32:
                if (entry->key.u32 == key->u32) {
                    return entry;
                }
                break;
            case ZCBOR_MAP_KEY_TYPE_TSTR:
                if (key->tstr.len == entry->key.tstr.len
                    && memcmp(entry->key.tstr.value, key->tstr.value, entry->key.tstr.len) == 0) {
                    return entry;
                }
                break;
        }
    }

    return NULL;
}

int zcbor_map_int64_decode(zcbor_state_t* zsd, void* value) {
    bool ok;

    ok = zcbor_int64_decode(zsd, value);
    if (!ok) {
        return -EBADMSG;
    }

    return 0;
}

int zcbor_map_tstr_decode(zcbor_state_t* zsd, void* value) {
    bool ok;

    ok = zcbor_tstr_decode(zsd, value);
    if (!ok) {
        return -EBADMSG;
    }

    return 0;
}

static int zcbor_map_key_decode(zcbor_state_t* zsd, struct zcbor_map_key* key) {
    zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*zsd->payload);
    bool ok;

    switch (major_type) {
        case ZCBOR_MAJOR_TYPE_TSTR:
            ok = zcbor_tstr_decode(zsd, &key->tstr);
            if (!ok) {
                GLTH_LOGW(TAG, "Failed to decode %s map key", "tstr");
                return -EBADMSG;
            }

            key->type = ZCBOR_MAP_KEY_TYPE_TSTR;
            return 0;
        case ZCBOR_MAJOR_TYPE_PINT:
            ok = zcbor_uint32_decode(zsd, &key->u32);
            if (!ok) {
                GLTH_LOGW(TAG, "Failed to decode %s map key", "u32");
                return -EBADMSG;
            }

            key->type = ZCBOR_MAP_KEY_TYPE_U32;
            return 0;
        default:
            break;
    }

    return -EBADMSG;
}

int zcbor_map_decode(zcbor_state_t* zsd, struct zcbor_map_entry* entries, size_t num_entries) {
    struct zcbor_map_entry* entry;
    size_t num_decoded = 0;
    struct zcbor_map_key key;
    int err = 0;
    bool ok;

    ok = zcbor_map_start_decode(zsd);
    if (!ok) {
        GLTH_LOGW(TAG, "Did not start CBOR map correctly");
        return -EBADMSG;
    }

    while (num_decoded < num_entries && !zcbor_list_or_map_end(zsd)) {
        err = zcbor_map_key_decode(zsd, &key);
        if (err) {
            return err;
        }

        entry = map_entry_get(entries, num_entries, &key);
        if (entry) {
            err = entry->decode(zsd, entry->value);
            if (err) {
                return err;
            }

            num_decoded++;
        } else {
            ok = zcbor_any_skip(zsd, NULL);
            if (!ok) {
                return -EBADMSG;
            }
        }
    }

    if (num_decoded == 0) {
        err = -ENOENT;
        goto map_end_decode;
    }

    if (num_decoded < num_entries) {
        return -EBADMSG;
    }

map_end_decode:
    ok = zcbor_list_map_end_force_decode(zsd);
    if (!ok) {
        GLTH_LOGW(TAG, "Did not end CBOR map correctly");
        return -EBADMSG;
    }

    return err;
}
