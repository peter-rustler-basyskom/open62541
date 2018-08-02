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

    UA_StatusCode
    (*serverSetHistoryData)(UA_Server *server,
                            void *hdbContext,
                            const UA_NodeId *nodeId,
                            const UA_DataValue *value);

    // read raw high level interface
    UA_StatusCode
    (*getHistoryData)(UA_Server *server,
                      const UA_NodeId *sessionId,
                      void *sessionContext,
                      const UA_HistoryDataBackend *backend,
                      const UA_DateTime start,
                      const UA_DateTime end,
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
    (*getDateTimeMatch)(UA_Server *server,
                        void *hdbContext,
                        const UA_NodeId *sessionId,
                        void *sessionContext,
                        const UA_NodeId *nodeId,
                        const UA_DateTime timestamp,
                        const MatchStrategy strategy);

    size_t
    (*getEnd)(UA_Server *server,
              void *hdbContext,
              const UA_NodeId *sessionId,
              void *sessionContext,
              const UA_NodeId *nodeId);
    size_t
    (*lastIndex)(UA_Server *server,
                 void *hdbContext,
                 const UA_NodeId *sessionId,
                 void *sessionContext,
                 const UA_NodeId *nodeId);
    size_t
    (*firstIndex)(UA_Server *server,
                  void *hdbContext,
                  const UA_NodeId *sessionId,
                  void *sessionContext,
                  const UA_NodeId *nodeId);

    size_t
    (*resultSize)(UA_Server *server,
                  void *hdbContext,
                  const UA_NodeId *sessionId,
                  void *sessionContext,
                  const UA_NodeId *nodeId,
                  size_t startIndex,
                  size_t endIndex);


    UA_StatusCode
    (*copyDataValues)(UA_Server *server,
                      void *hdbContext,
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
    (*getDataValue)(UA_Server *server,
                    void *hdbContext,
                    const UA_NodeId *sessionId,
                    void *sessionContext,
                    const UA_NodeId *nodeId,
                    size_t index);

    UA_Boolean
    (*boundSupported)(UA_Server *server,
                      void *hdbContext,
                      const UA_NodeId *sessionId,
                      void *sessionContext,
                      const UA_NodeId *nodeId);

    UA_Boolean
    (*timestampsToReturnSupported)(UA_Server *server,
                                   void *hdbContext,
                                   const UA_NodeId *sessionId,
                                   void *sessionContext,
                                   const UA_NodeId *nodeId,
                                   const UA_TimestampsToReturn timestampsToReturn);

};

#ifdef __cplusplus
}
#endif

#endif /* UA_PLUGIN_HISTORY_DATA_BACKEND_H_ */
