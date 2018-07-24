/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) basysKom GmbH <opensource@basyskom.com> (Author: Peter Rustler)
 */

#ifndef UA_PLUGIN_HISTORY_DATA_GATHERING_H_
#define UA_PLUGIN_HISTORY_DATA_GATHERING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_types.h"
#include "ua_server.h"
#include "ua_plugin_history_data_backend.h"

typedef enum {
    UA_HISTORIZINGUPDATESTRATEGY_USER     = 0x00,
    UA_HISTORIZINGUPDATESTRATEGY_VALUESET = 0x01,
    UA_HISTORIZINGUPDATESTRATEGY_POLL     = 0x02
} UA_HistorizingUpdateStrategy;

typedef struct {
    UA_HistoryDataBackend historizingBackend;
    size_t maxHistoryDataResponseSize;
    UA_HistorizingUpdateStrategy historizingUpdateStrategy;
    size_t pollingInterval;
    void * userContext;
} UA_HistorizingNodeIdSettings;

struct UA_HistoryDataGathering;
typedef struct UA_HistoryDataGathering UA_HistoryDataGathering;
struct UA_HistoryDataGathering {
    void *context;

    void
    (*deleteMembers)(UA_HistoryDataGathering *gathering);

    UA_StatusCode
    (*registerNodeId)(void *context,
                      UA_Server *server,
                      const UA_NodeId *nodeId,
                      const UA_HistorizingNodeIdSettings setting);

    UA_StatusCode
    (*stopPoll)(void *context,
                UA_Server *server,
                const UA_NodeId *nodeId);

    UA_StatusCode
    (*startPoll)(void *context,
                 UA_Server *server,
                 const UA_NodeId *nodeId);

    UA_Boolean
    (*updateNodeIdSetting)(void *context,
                           UA_Server *server,
                           const UA_NodeId *nodeId,
                           const UA_HistorizingNodeIdSettings setting);

    const UA_HistorizingNodeIdSettings*
    (*getHistorizingSetting)(void *context,
                             UA_Server *server,
                             const UA_NodeId *nodeId);
    void
    (*setValue)(void *context,
                UA_Server *server,
                const UA_NodeId *sessionId,
                void *sessionContext,
                const UA_NodeId *nodeId,
                const UA_DataValue *value);
};

#ifdef __cplusplus
}
#endif

#endif /* UA_PLUGIN_HISTORY_DATA_GATHERING_H_ */
