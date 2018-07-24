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
#include "ua_server.h"

typedef enum {
    MATCH_EQUAL,
    MATCH_AFTER,
    MATCH_EQUAL_OR_AFTER,
    MATCH_BEFORE,
    MATCH_EQUAL_OR_BEFORE
} MatchStrategy;

typedef struct UA_HistoryDataBackend UA_HistoryDataBackend;

struct UA_HistoryDataBackend {
    void *context;

    void
    (*deleteMembers)(UA_HistoryDataBackend *backend);

    // Manipulation high level interface
    UA_StatusCode
    (*serverSetHistoryData)(void *context,
                            UA_Server *server,
                            const UA_NodeId *nodeId,
                            const UA_DataValue *value);

    // read raw high level interface
    UA_StatusCode
    (*getHistoryData)(const UA_HistoryDataBackend *backend,
                      const UA_DateTime start,
                      const UA_DateTime end,
                      UA_Server *server,
                      const UA_NodeId *sessionId,
                      void *sessionContext,
                      const UA_NodeId *nodeId,
                      size_t maxSizePerResponse,
                      UA_UInt32 numValuesPerNode,
                      UA_Boolean returnBounds,
                      UA_TimestampsToReturn timestampsToReturn,
                      UA_NumericRange range,
                      UA_Boolean releaseContinuationPoints,
                      const UA_ByteString *continuationPoint,
                      UA_ByteString *outContinuationPoint,
                      UA_HistoryData *result);

    // read raw low level interface
    size_t
    (*getDateTimeMatch)(void *context,
                        UA_Server *server,
                        const UA_NodeId *sessionId,
                        void *sessionContext,
                        const UA_NodeId *nodeId,
                        const UA_DateTime timestamp,
                        const MatchStrategy strategy);

    size_t
    (*getEnd)(void *context,
              UA_Server *server,
              const UA_NodeId *sessionId,
              void *sessionContext,
              const UA_NodeId *nodeId);
    size_t
    (*lastIndex)(void *context,
                 UA_Server *server,
                 const UA_NodeId *sessionId,
                 void *sessionContext,
                 const UA_NodeId *nodeId);
    size_t
    (*firstIndex)(void *context,
                  UA_Server *server,
                  const UA_NodeId *sessionId,
                  void *sessionContext,
                  const UA_NodeId *nodeId);

    size_t
    (*resultSize)(void *context,
                  UA_Server *server,
                  const UA_NodeId *sessionId,
                  void *sessionContext,
                  const UA_NodeId *nodeId,
                  size_t startIndex,
                  size_t endIndex);


    UA_StatusCode
    (*copyDataValues)(void *context,
                      UA_Server *server,
                      const UA_NodeId *sessionId,
                      void *sessionContext,
                      const UA_NodeId *nodeId,
                      size_t startIndex,
                      size_t endIndex,
                      UA_Boolean reverse,
                      size_t valueSize,
                      UA_NumericRange range,
                      UA_Boolean releaseContinuationPoints,
                      const UA_ByteString *continuationPoint,
                      UA_ByteString *outContinuationPoint,
                      size_t *providedValues,
                      UA_DataValue *values);

    const UA_DataValue*
    (*getDataValue)(void *context,
                    UA_Server *server,
                    const UA_NodeId *sessionId,
                    void *sessionContext,
                    const UA_NodeId *nodeId,
                    size_t index);

    UA_Boolean
    (*boundSupported)(void *context,
                      UA_Server *server,
                      const UA_NodeId *sessionId,
                      void *sessionContext,
                      const UA_NodeId *nodeId);

    UA_Boolean
    (*timestampsToReturnSupported)(void *context,
                                   UA_Server *server,
                                   const UA_NodeId *sessionId,
                                   void *sessionContext,
                                   const UA_NodeId *nodeId,
                                   const UA_TimestampsToReturn timestampsToReturn);

};

#ifdef __cplusplus
}
#endif

#endif /* UA_PLUGIN_HISTORY_DATA_BACKEND_H_ */
