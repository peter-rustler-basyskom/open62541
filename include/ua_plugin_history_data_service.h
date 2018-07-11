/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) basysKom GmbH <opensource@basyskom.com> (Author: Peter Rustler)
 */

#ifndef UA_PLUGIN_HISTORY_DATA_SERVICE_H_
#define UA_PLUGIN_HISTORY_DATA_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_types.h"
#include "ua_server.h"

struct UA_HistoryDataService;
typedef struct UA_HistoryDataService UA_HistoryDataService;

struct UA_HistoryDataService {
    void *context;

    void
    (*deleteMembers)(UA_HistoryDataService *service);

    /* This function will be called when a node value is set.
     * Use this to insert data into your database(s) if polling is not suitable
     * and you need to get all data changes.
     * Set it to NULL if you do not need it.
     *
     * context is the context of the UA_HistoryDataService. UA_HistoryDataService.context
     * server is the server this node lives in.
     * sessionId and sessionContext identify the session which set this value.
     * nodeId the node id for which data was set.
     * value the new value.
     */
    void
    (*setValue)(void *context,
                UA_Server *server,
                const UA_NodeId *sessionId,
                void *sessionContext,
                const UA_NodeId *nodeId,
                const UA_DataValue *value);

    /* This function is called if a history read is requested
     * with isRawReadModified set to false.
     * Setting it to NULL will result in a response with statuscode UA_STATUSCODE_BADHISTORYOPERATIONUNSUPPORTED.
     *
     * context is the context of the UA_HistoryDataService. UA_HistoryDataService.context
     * server is the server this node lives in.
     * sessionId and sessionContext identify the session which set this value.
     * nodeId the node id for which data was set.
     * request is the complete request from the client.
     * details is the details object which is also present in the request as an extension object.
     * response the response to fill for the client.
     * results is a pointer with an already allocated array of UA_HistoryReadResult with
     *         size of request->nodesToReadSize.
     *         This is also present in the response.
     */
    void
    (*readRaw)(void *context,
               UA_Server *server,
               const UA_NodeId *sessionId,
               void *sessionContext,
               const UA_HistoryReadRequest *request,
               const UA_ReadRawModifiedDetails *details,
               UA_HistoryReadResponse *response,
               UA_HistoryReadResult* results);

    // Add more function pointer here.
    // For example for read_event, read_modified, read_processed, read_at_time
};

#ifdef __cplusplus
}
#endif

#endif /* UA_PLUGIN_HISTORY_DATA_SERVICE_H_ */
