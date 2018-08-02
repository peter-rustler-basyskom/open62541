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

    /* This function registers a node for the gathering of historical data.
     *
     * server is the server the node lives in.
     * hdgContext is the context of the UA_HistoryDataGathering.
     * nodeId is the node id of the node to register.
     * setting contains the gatering settings for the node to register.
     */
    UA_StatusCode
    (*registerNodeId)(UA_Server *server,
                      void *hdgContext,
                      const UA_NodeId *nodeId,
                      const UA_HistorizingNodeIdSettings setting);

    /* This function stops polling a node for value changes.
     *
     * server is the server the node lives in.
     * hdgContext is the context of the UA_HistoryDataGathering.
     * nodeId is id of the node for which polling shall be stopped.
     */
    UA_StatusCode
    (*stopPoll)(UA_Server *server,
                void *hdgContext,
                const UA_NodeId *nodeId);

    /* This function starts polling a node for value changes.
     *
     * server is the server the node lives in.
     * hdgContext is the context of the UA_HistoryDataGathering.
     * nodeId is the id of the node for which polling shall be started.
     */
    UA_StatusCode
    (*startPoll)(UA_Server *server,
                 void *hdgContext,
                 const UA_NodeId *nodeId);

    /* This function modifies the gathering settings for a node.
     *
     * server is the server the node lives in.
     * hdgContext is the context of the UA_HistoryDataGathering.
     * nodeId is the node id of the node for which gathering shall be modified.
     */
    UA_Boolean
    (*updateNodeIdSetting)(UA_Server *server,
                           void *hdgContext,
                           const UA_NodeId *nodeId,
                           const UA_HistorizingNodeIdSettings setting);

    /* Returns the gathering settings for a node.
     *
     * server is the server the node lives in.
     * hdgContext is the context of the UA_HistoryDataGathering.
     * nodeId is the node id of the node for which the gathering settings shall be retrieved.
     */
    const UA_HistorizingNodeIdSettings*
    (*getHistorizingSetting)(UA_Server *server,
                             void *hdgContext,
                             const UA_NodeId *nodeId);

    /* Sets a DataValue for a node in the historical data storage.
     *
     * server is the server the node lives in.
     * hdgContext is the context of the UA_HistoryDataGathering.
     * sessionId and sessionContext identify the session which wants to set this value.
     * nodeId is the node id of the node for which a value shall be set.
     */
    void
    (*setValue)(UA_Server *server,
                void *hdgContext,
                const UA_NodeId *sessionId,
                void *sessionContext,
                const UA_NodeId *nodeId,
                const UA_DataValue *value);
};

#ifdef __cplusplus
}
#endif

#endif /* UA_PLUGIN_HISTORY_DATA_GATHERING_H_ */
