// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <dse/modelc/adapter/uid_index.h>


#define INITIAL_CAPACITY 16


typedef struct SignalValueIndexItem {
    uint32_t     uid;
    SignalValue* sv;
} SignalValueIndexItem;


struct SignalValueUidIndex {
    SignalValueIndexItem* entries;
    size_t                capacity;
    size_t                count;
};


static uint32_t _hash_uid(uint32_t uid)
{
    return uid * 0x9e3779b9U;
}


static size_t _find_slot(
    const SignalValueIndexItem* entries, size_t capacity, uint32_t uid)
{
    size_t slot = _hash_uid(uid) & (capacity - 1);
    while (entries[slot].uid != 0 && entries[slot].uid != uid) {
        slot = (slot + 1) & (capacity - 1);
    }
    return slot;
}


static bool _resize(SignalValueUidIndex* index, size_t capacity)
{
    if (capacity > SIZE_MAX / sizeof(SignalValueIndexItem)) {
        errno = EOVERFLOW;
        return false;
    }

    SignalValueIndexItem* entries = calloc(capacity, sizeof(*entries));
    if (entries == NULL) {
        if (errno == 0) errno = ENOMEM;
        return false;
    }

    for (size_t i = 0; i < index->capacity; i++) {
        SignalValueIndexItem entry = index->entries[i];
        if (entry.uid != 0) {
            size_t slot = _find_slot(entries, capacity, entry.uid);
            entries[slot] = entry;
        }
    }

    free(index->entries);
    index->entries = entries;
    index->capacity = capacity;
    return true;
}


static bool _reserve(SignalValueUidIndex* index, size_t count)
{
    size_t capacity = index->capacity ? index->capacity : INITIAL_CAPACITY;
    while (count > capacity - capacity / 3) {
        if (capacity > SIZE_MAX / 2) {
            errno = EOVERFLOW;
            return false;
        }
        capacity *= 2;
    }
    return capacity == index->capacity || _resize(index, capacity);
}


bool signal_value_uid_index_insert(
    SignalValueUidIndex** index_ptr, uint32_t uid, SignalValue* signal)
{
    if (index_ptr == NULL || signal == NULL) {
        errno = EINVAL;
        return false;
    }
    if (uid == 0) {
        errno = 0;
        return true;
    }

    SignalValueUidIndex* index = *index_ptr;
    if (index && index->capacity) {
        size_t slot = _find_slot(index->entries, index->capacity, uid);
        if (index->entries[slot].uid == uid) {
            errno = 0;
            return true;
        }
    }

    if (index == NULL) {
        index = calloc(1, sizeof(*index));
        if (index == NULL) {
            if (errno == 0) errno = ENOMEM;
            return false;
        }
        *index_ptr = index;
    }
    if (index->count == SIZE_MAX) {
        errno = EOVERFLOW;
        return false;
    }
    if (!_reserve(index, index->count + 1)) return false;

    size_t slot = _find_slot(index->entries, index->capacity, uid);
    index->entries[slot] = (SignalValueIndexItem){ .uid = uid, .sv = signal };
    index->count++;
    errno = 0;
    return true;
}


SignalValue* signal_value_uid_index_find(
    const SignalValueUidIndex* index, uint32_t uid)
{
    if (index == NULL || index->capacity == 0 || uid == 0) return NULL;

    size_t slot = _find_slot(index->entries, index->capacity, uid);
    return index->entries[slot].uid == uid ? index->entries[slot].sv : NULL;
}


void signal_value_uid_index_destroy(SignalValueUidIndex* index)
{
    if (index == NULL) return;
    free(index->entries);
    free(index);
}
