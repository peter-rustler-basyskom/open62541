/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) basysKom GmbH <opensource@basyskom.com> (Author: Peter Rustler)
 */

#include "ua_historydatabackend_memory.h"
#include <limits.h>
#include <string.h>

typedef struct {
    UA_DateTime timestamp;
    UA_DataValue value;
} UA_DataValueMemoryStoreItem;

static void UA_DataValueMemoryStoreItem_deleteMembers(UA_DataValueMemoryStoreItem* item) {
    UA_DateTime_deleteMembers(&item->timestamp);
    UA_DataValue_deleteMembers(&item->value);
}

typedef struct {
    UA_NodeId nodeId;
    UA_DataValueMemoryStoreItem **dataStore;
    size_t storeEnd;
    size_t storeSize;
} UA_NodeIdStoreContextItem;

static void UA_NodeIdStoreContextItem_deleteMembers(UA_NodeIdStoreContextItem* item) {
    UA_NodeId_deleteMembers(&item->nodeId);
    for (size_t i = 0; i < item->storeEnd; ++i) {
        UA_DataValueMemoryStoreItem_deleteMembers(item->dataStore[i]);
        UA_free(item->dataStore[i]);
    }
    UA_free(item->dataStore);
}

typedef struct {
    UA_NodeIdStoreContextItem **nodeStore;
    size_t storeEnd;
    size_t storeSize;
    size_t initialStoreSize;
} UA_MemoryStoreContext;

static void UA_MemoryStoreContext_deleteMembers(UA_MemoryStoreContext* ctx) {
    for (size_t i = 0; i < ctx->storeEnd; ++i) {
        UA_NodeIdStoreContextItem_deleteMembers(ctx->nodeStore[i]);
        UA_free(ctx->nodeStore[i]);
    }
    UA_free(ctx->nodeStore);
}

static void*
getNodeIdContext(void *context, const UA_NodeId *node) {
    UA_MemoryStoreContext *ctx = (UA_MemoryStoreContext*)context;
    for (size_t i = 0; i < ctx->storeEnd; ++i) {
        if (UA_NodeId_equal(&ctx->nodeStore[i]->nodeId, node))
            return ctx->nodeStore[i];
    }
    // not found a node item
    if (ctx->storeEnd >= ctx->storeSize) {
        size_t newStoreSize = ctx->storeSize == 0 ? INITIAL_MEMORY_STORE_SIZE : ctx->storeSize * 2;
        ctx->nodeStore = (UA_NodeIdStoreContextItem **)UA_realloc(ctx->nodeStore,  (newStoreSize * sizeof(UA_NodeIdStoreContextItem*)));
        if (!ctx->nodeStore) {
            ctx->storeSize = 0;
            return NULL;
        }
        ctx->storeSize = newStoreSize;
    }
    UA_NodeIdStoreContextItem *item = (UA_NodeIdStoreContextItem *)UA_calloc(1, sizeof(UA_NodeIdStoreContextItem));
    if (!item)
        return NULL;
    UA_NodeId_copy(node, &item->nodeId);
    item->storeEnd = 0;
    item->storeSize = ctx->initialStoreSize;
    item->dataStore = (UA_DataValueMemoryStoreItem **)UA_calloc(ctx->initialStoreSize, sizeof(UA_DataValueMemoryStoreItem*));
    if (!item->dataStore) {
        UA_free(item);
        return NULL;
    }
    ctx->nodeStore[ctx->storeEnd] = item;
    ++ctx->storeEnd;

    return item;
}

static UA_Boolean binarySearch(const UA_NodeIdStoreContextItem* item, const UA_DateTime timestamp, size_t *index) {
    if (item->storeEnd == 0) {
        *index = item->storeEnd;
        return false;
    }
    size_t min = 0;
    size_t max = item->storeEnd - 1;
    while (min <= max) {
        *index = (min + max) / 2;
        if (item->dataStore[*index]->timestamp == timestamp) {
            return true;
        } else if (item->dataStore[*index]->timestamp < timestamp) {
            if (*index == item->storeEnd - 1) {
                *index = item->storeEnd;
                return false;
            }
            min = *index + 1;
        } else {
            if (*index == 0)
                return false;
            max = *index - 1;
        }
    }
    *index = min;
    return false;

}

static size_t
resultSize(const void* nodeIdContext,
           size_t startIndex,
           size_t endIndex) {
    const UA_NodeIdStoreContextItem* item = (const UA_NodeIdStoreContextItem*)nodeIdContext;
    if (item->storeEnd == 0
            || startIndex == item->storeEnd
            || endIndex == item->storeEnd)
        return 0;
    return endIndex - startIndex + 1;
}

static size_t getDateTimeMatch(const void * nodeIdContext,
                               const UA_DateTime timestamp,
                               const MatchStrategy strategy) {
    const UA_NodeIdStoreContextItem* item = (const UA_NodeIdStoreContextItem*)nodeIdContext;
    size_t current;
    UA_Boolean retval = binarySearch(item, timestamp, &current);

    if ((strategy == MATCH_EQUAL
         || strategy == MATCH_EQUAL_OR_AFTER
         || strategy == MATCH_EQUAL_OR_BEFORE)
            && retval)
        return current;
    switch (strategy) {
    case MATCH_AFTER:
        if (retval)
            return current+1;
        return current;
    case MATCH_EQUAL_OR_AFTER:
        return current;
    case MATCH_EQUAL_OR_BEFORE:
        // retval == true aka "equal" is handled before
        // Fall through if !retval
    case MATCH_BEFORE:
        if (current > 0)
            return current-1;
        else
            return item->storeEnd;
    default:
        break;
    }
    return item->storeEnd;
}

static UA_StatusCode
insertHistoryData(void* nodeIdContext,
                  UA_DataValue *value)
{
    UA_NodeIdStoreContextItem *item = (UA_NodeIdStoreContextItem *)nodeIdContext;

    if (item->storeEnd >= item->storeSize) {
        size_t newStoreSize = item->storeSize == 0 ? INITIAL_MEMORY_STORE_SIZE : item->storeSize * 2;
        item->dataStore = (UA_DataValueMemoryStoreItem **)UA_realloc(item->dataStore,  (newStoreSize * sizeof(UA_DataValueMemoryStoreItem*)));
        if (!item->dataStore) {
            item->storeSize = 0;
            return UA_STATUSCODE_BADOUTOFMEMORY;
        }
        item->storeSize = newStoreSize;
    }
    UA_DateTime timestamp = 0;
    if (value->hasSourceTimestamp) {
        timestamp = value->sourceTimestamp;
    } else if (value->hasServerTimestamp) {
        timestamp = value->serverTimestamp;
    } else {
        timestamp = UA_DateTime_now();
    }
    UA_DataValueMemoryStoreItem *newItem = (UA_DataValueMemoryStoreItem *)UA_calloc(1, sizeof(UA_DataValueMemoryStoreItem));
    newItem->timestamp = timestamp;
    UA_DataValue_copy(value, &newItem->value);
    size_t index = getDateTimeMatch(item, timestamp, MATCH_EQUAL_OR_AFTER);
    if (item->storeEnd > 0 && index < item->storeEnd) {
        memmove(&item->dataStore[index+1], &item->dataStore[index], sizeof(UA_DataValueMemoryStoreItem*) * (item->storeEnd - index));
    }
    item->dataStore[index] = newItem;
    ++item->storeEnd;
    return UA_STATUSCODE_GOOD;
}

static void
UA_MemoryStoreContext_delete(UA_MemoryStoreContext* ctx) {
    UA_MemoryStoreContext_deleteMembers(ctx);
    UA_free(ctx);
}

static size_t
getEnd(const void * nodeIdContext) {
    const UA_NodeIdStoreContextItem* item = (const UA_NodeIdStoreContextItem*)nodeIdContext;
    return item->storeEnd;
}

static size_t
lastIndex(const void * nodeIdContext) {
    const UA_NodeIdStoreContextItem* item = (const UA_NodeIdStoreContextItem*)nodeIdContext;
    return item->storeEnd - 1;
}

static size_t
firstIndex(const void * nodeIdContext) {
    return 0;
}

static UA_Boolean
boundSupported(void) {
    return true;
}

static const UA_DataValue*
getDataValue(const void* nodeIdContext, size_t index) {
    const UA_NodeIdStoreContextItem* item = (const UA_NodeIdStoreContextItem*)nodeIdContext;
    return &item->dataStore[index]->value;
}

static UA_StatusCode
UA_DataValue_copyRange(const UA_DataValue *src, UA_DataValue *dst,
                       const UA_NumericRange range)
{
    memcpy(dst, src, sizeof(UA_DataValue));
    if (src->hasValue)
        return UA_Variant_copyRange(&src->value, &dst->value, range);
    return UA_STATUSCODE_BADDATAUNAVAILABLE;
}
static size_t
copyDataValues(const void* nodeIdContext,
               size_t startIndex,
               size_t endIndex,
               UA_Boolean reverse,
               size_t skip,
               size_t maxValues,
               size_t * skipedValues,
               UA_NumericRange * range,
               UA_DataValue * values)
{
    const UA_NodeIdStoreContextItem* item = (const UA_NodeIdStoreContextItem*)nodeIdContext;
    size_t index = startIndex;
    size_t counter = 0;
    if (reverse) {
        while (index >= endIndex && index < item->storeEnd && counter < maxValues) {
            if ((*skipedValues)++ >= skip) {
                if (range) {
                    UA_DataValue_copyRange(&item->dataStore[index]->value, &values[counter], *range);
                } else {
                    UA_DataValue_copy(&item->dataStore[index]->value, &values[counter]);
                }
                ++counter;
            }
            --index;
        }
    } else {
        while (index <= endIndex && counter < maxValues) {
            if ((*skipedValues)++ >= skip) {
                if (range) {
                    UA_DataValue_copyRange(&item->dataStore[index]->value, &values[counter], *range);
                } else {
                    UA_DataValue_copy(&item->dataStore[index]->value, &values[counter]);
                }
                ++counter;
            }
            ++index;
        }
    }
    return counter;
}

UA_HistoryDataBackend
UA_HistoryDataBackend_Memory(size_t initialNodeStoreSize, size_t initialDataStoreSize) {
    UA_HistoryDataBackend result;
    result.insertHistoryData = NULL;
    result.getEnd = NULL;
    result.lastIndex = NULL;
    result.firstIndex = NULL;
    result.getDateTimeMatch = NULL;
    result.getNodeIdContext = NULL;
    result.context = NULL;
    result.copyDataValues = NULL;
    result.getDataValue = NULL;
    result.boundSupported = NULL;
    UA_MemoryStoreContext *ctx = (UA_MemoryStoreContext *)UA_calloc(1, sizeof(UA_MemoryStoreContext));
    if (!ctx)
        return result;
    ctx->storeEnd = 0;
    ctx->storeSize = initialNodeStoreSize;
    ctx->initialStoreSize = initialDataStoreSize;
    ctx->nodeStore = (UA_NodeIdStoreContextItem **)UA_calloc(initialNodeStoreSize, sizeof(UA_NodeIdStoreContextItem*));
    if (!ctx->nodeStore) {
        UA_free(ctx);
        return result;
    }
    result.insertHistoryData = &insertHistoryData;
    result.resultSize = &resultSize;
    result.getNodeIdContext = &getNodeIdContext;
    result.getEnd = &getEnd;
    result.lastIndex = &lastIndex;
    result.firstIndex = &firstIndex;
    result.getDateTimeMatch = &getDateTimeMatch;
    result.copyDataValues = &copyDataValues;
    result.getDataValue = &getDataValue;
    result.boundSupported = &boundSupported;
    result.context = ctx;
    return result;
}

void
UA_HistoryDataBackend_Memory_deleteMembers(UA_HistoryDataBackend *backend)
{
    UA_MemoryStoreContext *ctx = (UA_MemoryStoreContext*)backend->context;
    UA_MemoryStoreContext_delete(ctx);
    memset(backend, 0, sizeof(UA_HistoryDataBackend));
}
