/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) basysKom GmbH <opensource@basyskom.com> (Author: Peter Rustler)
 */

#ifndef UA_PLUGIN_HISTORY_DATA_BACKEND_H_
#define UA_PLUGIN_HISTORY_DATA_BACKEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_types.h"

typedef enum {
    MATCH_EQUAL,
    MATCH_AFTER,
    MATCH_EQUAL_OR_AFTER,
    MATCH_BEFORE,
    MATCH_EQUAL_OR_BEFORE
} MatchStrategy;

typedef struct {
    void *context;
    //    UA_StatusCode (*getHistoryData)(void * context,
    //                                    UA_DateTime start,
    //                                    UA_DateTime end,
    //                                    UA_NodeId * nodeId,
    //                                    size_t skip,
    //                                    size_t maxSize,
    //                                    UA_UInt32 numValuesPerNode,
    //                                    UA_Boolean returnBounds,
    //                                    UA_DataValue ** result,
    //                                    size_t * resultSize,
    //                                    UA_Boolean * hasMoreData);

    void * (*getNodeIdContext)(void *context,
                               const UA_NodeId *nodeId);

    UA_StatusCode (*insertHistoryData)(void * nodeIdContext,
                                       UA_DataValue * value);

    size_t (*getDateTimeMatch)(const void * nodeIdContext,
                               const UA_DateTime timestamp,
                               const MatchStrategy strategy);

    size_t (*getEnd)(const void * nodeIdContext);
    size_t (*lastIndex)(const void * nodeIdContext);
    size_t (*firstIndex)(const void * nodeIdContext);

    size_t (*resultSize)(const void* nodeIdContext,
                         size_t startIndex,
                         size_t endIndex);

    size_t (*copyDataValues)(const void* nodeIdContext,
                             size_t startIndex,
                             size_t endIndex,
                             UA_Boolean reverse,
                             size_t skip,
                             size_t maxValues,
                             size_t * skipedValues,
                             UA_NumericRange * range,
                             UA_DataValue * values);

    const UA_DataValue* (*getDataValue)(const void* nodeIdContext,
                                        size_t index);

    UA_Boolean (*boundSupported)(void);

} UA_HistoryDataBackend;

typedef enum {
    UA_HISTORIZINGUPDATESTRATEGY_USER     = 0x00,
    UA_HISTORIZINGUPDATESTRATEGY_VALUESET = 0x01,
    UA_HISTORIZINGUPDATESTRATEGY_POLL     = 0x02
} UA_HistorizingUpdateStrategy;

typedef struct {
    UA_HistoryDataBackend historizingBackend;
    UA_HistorizingUpdateStrategy historizingUpdateStrategy;
    size_t maxHistoryDataResponseSize;
} UA_HistorizingSetting;

#ifdef __cplusplus
}
#endif

#endif /* UA_PLUGIN_HISTORY_DATA_BACKEND_H_ */
