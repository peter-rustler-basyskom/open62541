/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2018 (c) basysKom GmbH <opensource@basyskom.com> (Author: Peter Rustler)
 */

#include "ua_types.h"
#include "ua_server.h"
#include "ua_client.h"
#include "client/ua_client_internal.h"
#include "ua_client_highlevel.h"
#include "ua_config_default.h"
#include "ua_network_tcp.h"

#include "check.h"
#include "testing_clock.h"
#include "testing_networklayers.h"
#include "thread_wrapper.h"
#include "ua_historydatabackend_memory.h"
#include "historical_read_test_data.h"
#include <stddef.h>

UA_Server *server;
UA_ServerConfig *config;
UA_Boolean running;
THREAD_HANDLE server_thread;

UA_Client *client;
UA_NodeId parentNodeId;
UA_NodeId parentReferenceNodeId;
UA_NodeId outNodeId;

THREAD_CALLBACK(serverloop) {
    while(running)
        UA_Server_run_iterate(server, true);
    return 0;
}

static void
setup(void) {
    running = true;
    config = UA_ServerConfig_new_default();
    server = UA_Server_new(config);
    UA_StatusCode retval = UA_Server_run_startup(server);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    THREAD_CREATE(server_thread, serverloop);
    /* Define the attribute of the uint32 variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_UInt32 myUint32 = 40;
    UA_Variant_setScalar(&attr.value, &myUint32, &UA_TYPES[UA_TYPES_UINT32]);
    attr.description = UA_LOCALIZEDTEXT("en-US","the answer");
    attr.displayName = UA_LOCALIZEDTEXT("en-US","the answer");
    attr.dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE | UA_ACCESSLEVELMASK_HISTORYREAD;
    attr.historizing = true;

    /* Add the variable node to the information model */
    UA_NodeId uint32NodeId = UA_NODEID_STRING(1, "the.answer");
    UA_QualifiedName uint32Name = UA_QUALIFIEDNAME(1, "the answer");
    parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId_init(&outNodeId);
    ck_assert_uint_eq(UA_Server_addVariableNode(server,
                                                uint32NodeId,
                                                parentNodeId,
                                                parentReferenceNodeId,
                                                uint32Name,
                                                UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                                attr,
                                                NULL,
                                                &outNodeId)
                      , UA_STATUSCODE_GOOD);

    client = UA_Client_new(UA_ClientConfig_default);
    retval = UA_Client_connect(client, "opc.tcp://localhost:4840");
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    UA_Client_recv = client->connection.recv;
    client->connection.recv = UA_Client_recvTesting;
}

static void
teardown(void) {
    /* cleanup */
    UA_Client_disconnect(client);
    UA_Client_delete(client);
    UA_NodeId_deleteMembers(&parentNodeId);
    UA_NodeId_deleteMembers(&parentReferenceNodeId);
    UA_NodeId_deleteMembers(&outNodeId);
    running = false;
    THREAD_JOIN(server_thread);
    UA_Server_run_shutdown(server);
    UA_Server_delete(server);
    UA_ServerConfig_delete(config);
}

#ifdef UA_ENABLE_HISTORIZING

#include <stdio.h>
#include "ua_session.h"

static UA_StatusCode
setUInt32(UA_Client *thisClient, UA_NodeId node, UA_UInt32 value) {
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_UINT32]);
    return UA_Client_writeValueAttribute(thisClient, node, &variant);
}

static void
printTimestamp(UA_DateTime timestamp) {
    if (timestamp == TIMESTAMP_FIRST) {
        fprintf(stderr, "FIRST,");
    } else if (timestamp == TIMESTAMP_LAST) {
        fprintf(stderr, "LAST,");
    } else {
        fprintf(stderr, "%3lld,", timestamp / UA_DATETIME_SEC);
    }
}

static void
printResult(UA_DataValue * value) {
    if (value->status != UA_STATUSCODE_GOOD)
        fprintf(stderr, "%s:", UA_StatusCode_name(value->status));
    printTimestamp(value->sourceTimestamp);
}

static UA_Boolean resultIsEqual(const UA_DataValue * result, const testTuple * tuple, size_t index) {
    switch (tuple->result[index]) {
    case TIMESTAMP_FIRST:
        if (result->status != UA_STATUSCODE_BADBOUNDNOTFOUND
                || !UA_Variant_isEmpty(&result->value))
            return false;
        /* we do not test timestamp if TIMESTAMP_UNSPECIFIED is given for start.
         * See OPC UA Part 11, Version 1.03, Page 5-6, Table 1, Mark b for details.*/
        if (tuple->start != TIMESTAMP_UNSPECIFIED
                && tuple->start != result->sourceTimestamp)
            return false;
        break;
    case TIMESTAMP_LAST:
        if (result->status != UA_STATUSCODE_BADBOUNDNOTFOUND
                || !UA_Variant_isEmpty(&result->value))
            return false;
        /* we do not test timestamp if TIMESTAMP_UNSPECIFIED is given for end.
         * See OPC UA Part 11, Version 1.03, Page 5-6, Table 1, Mark a for details.*/
        if (tuple->end != TIMESTAMP_UNSPECIFIED
                && tuple->end != result->sourceTimestamp)
            return false;
        break;
    default:
        if (result->sourceTimestamp != tuple->result[index]
                || result->value.type != &UA_TYPES[UA_TYPES_INT64]
                || *((UA_Int64*)result->value.data) != tuple->result[index])
            return false;
    }
    return true;
}

static UA_Boolean fillHistoricalDataBackend(UA_HistoryDataBackend *backend) {
    int i = 0;
    UA_DateTime currentDateTime = testData[i];
    fprintf(stderr, "Adding to historical data backend: ");
    void *nodeIdContext = backend->getNodeIdContext(
                backend->context,
                &outNodeId);
    while (currentDateTime) {
        fprintf(stderr, "%lld, ", currentDateTime / UA_DATETIME_SEC);
        UA_DataValue value;
        UA_DataValue_init(&value);
        value.hasValue = true;
        UA_Int64 d = currentDateTime;
        UA_Variant_setScalarCopy(&value.value, &d, &UA_TYPES[UA_TYPES_INT64]);
        value.hasSourceTimestamp = true;
        value.sourceTimestamp = currentDateTime;
        value.hasStatus = true;
        value.status = UA_STATUSCODE_GOOD;
        if (backend->insertHistoryData(nodeIdContext, &value) != UA_STATUSCODE_GOOD) {
            fprintf(stderr, "\n");
            return false;
        }
        UA_DataValue_deleteMembers(&value);
        currentDateTime = testData[++i];
    }
    fprintf(stderr, "\n");
    return true;
}

void
Service_HistoryRead(UA_Server *server, UA_Session *session,
                    const UA_HistoryReadRequest *request,
                    UA_HistoryReadResponse *response);

static void
requestHistory(UA_DateTime start,
               UA_DateTime end,
               UA_HistoryReadResponse * response,
               UA_UInt32 numValuesPerNode,
               UA_Boolean returnBounds,
               UA_ByteString *continuationPoint) {
    UA_ReadRawModifiedDetails *details = UA_ReadRawModifiedDetails_new();
    details->startTime = start;
    details->endTime = end;
    details->isReadModified = false;
    details->numValuesPerNode = numValuesPerNode;
    details->returnBounds = returnBounds;

    UA_HistoryReadValueId *valueId = UA_HistoryReadValueId_new();
    UA_NodeId_copy(&outNodeId, &valueId->nodeId);
    if (continuationPoint)
        UA_ByteString_copy(continuationPoint, &valueId->continuationPoint);

    UA_HistoryReadRequest request;
    UA_HistoryReadRequest_init(&request);
    request.historyReadDetails.encoding = UA_EXTENSIONOBJECT_DECODED;
    request.historyReadDetails.content.decoded.type = &UA_TYPES[UA_TYPES_READRAWMODIFIEDDETAILS];
    request.historyReadDetails.content.decoded.data = details;

    request.nodesToReadSize = 1;
    request.nodesToRead = valueId;

    Service_HistoryRead(server, NULL, &request, response);
    UA_HistoryReadRequest_deleteMembers(&request);
}

static UA_UInt32 testHistoricalDataBackend(UA_HistoryDataBackend *backend, size_t maxResponseSize) {
    UA_HistorizingSetting setting;
    setting.historizingBackend = *backend;
    setting.maxHistoryDataResponseSize = maxResponseSize;
    setting.historizingUpdateStrategy = UA_HISTORIZINGUPDATESTRATEGY_USER;
    UA_StatusCode retx = UA_Server_setHistorizingSettingToVariableNode(server, &outNodeId, setting);
    ck_assert_uint_eq(retx, UA_STATUSCODE_GOOD);

    UA_UInt32 retval = 0;
    size_t i = 0;
    testTuple *current = &testRequests[i];
    fprintf(stderr, "Start | End  | numValuesPerNode | returnBounds | {Expected}{Result}{Cont 1}{Cont 2} Result\n");
    fprintf(stderr, "------+------+------------------+--------------+----------------\n");
    size_t j;
    while (current->start || current->end) {
        j = 0;
        if (current->start == TIMESTAMP_UNSPECIFIED) {
            fprintf(stderr, "UNSPEC|");
        } else {
            fprintf(stderr, "  %3lld |", current->start / UA_DATETIME_SEC);
        }
        if (current->end == TIMESTAMP_UNSPECIFIED) {
            fprintf(stderr, "UNSPEC|");
        } else {
            fprintf(stderr, "  %3lld |", current->end / UA_DATETIME_SEC);
        }
        fprintf(stderr, "               %2u |          %s | {", current->numValuesPerNode, (current->returnBounds ? "Yes" : " No"));
        while (current->result[j]) {
            printTimestamp(current->result[j]);
            ++j;
        }
        fprintf(stderr, "}");

        UA_DataValue *result = NULL;
        size_t resultSize = 0;
        UA_Boolean hasMoreData = true;
        UA_ByteString continuous;
        UA_ByteString_init(&continuous);
        UA_Boolean readOk = true;
        size_t reseivedValues = 0;
        fprintf(stderr, "{");
        while (hasMoreData) {
            UA_HistoryReadResponse response;
            requestHistory(current->start,
                           current->end,
                           &response,
                           current->numValuesPerNode,
                           current->returnBounds,
                           &continuous);
            if(response.resultsSize != 1) {
                fprintf(stderr, "ResultError:Size");
                readOk = false;
                hasMoreData = false;
                break;
            }
            hasMoreData = response.results[0].continuationPoint.length > 0;
            UA_StatusCode stat = response.results[0].statusCode;
            if (stat == UA_STATUSCODE_BADBOUNDNOTSUPPORTED && current->returnBounds) {
                hasMoreData = false;
                fprintf(stderr, "%s", UA_StatusCode_name(stat));
                break;
            }

            if(response.results[0].historyData.encoding != UA_EXTENSIONOBJECT_DECODED
                    || response.results[0].historyData.content.decoded.type != &UA_TYPES[UA_TYPES_HISTORYDATA]) {
                fprintf(stderr, "ResultError:HistoryData");
                readOk = false;
                hasMoreData = false;
                break;
            }
            UA_HistoryData * data = (UA_HistoryData *)response.results[0].historyData.content.decoded.data;
            resultSize = data->dataValuesSize;
            result = data->dataValues;
            if (resultSize > maxResponseSize) {
                    fprintf(stderr, "ContinuousError");
                    readOk = false;
                    hasMoreData = false;
                    break;
            }
            if (hasMoreData) {
                UA_ByteString_deleteMembers(&continuous);
                UA_ByteString_copy(&response.results[0].continuationPoint, &continuous);
            }

            if (stat != UA_STATUSCODE_GOOD) {
                fprintf(stderr, "%s", UA_StatusCode_name(stat));
            } else {
                for (size_t k = 0; k < resultSize; ++k)
                    printResult(&result[k]);
            }

            if (stat == UA_STATUSCODE_GOOD && j >= resultSize + reseivedValues) {
                for (size_t l = 0; l < resultSize; ++l) {
                    /* See OPC UA Part 11, Version 1.03, Page 5-6, Table 1, Mark a for details.*/
                    if (current->result[l + reseivedValues] == TIMESTAMP_LAST && current->end == TIMESTAMP_UNSPECIFIED) {
                        // This test will work on not continous read, only
                        if (reseivedValues == 0 && !(l > 0 && result[l].sourceTimestamp == result[l-1].sourceTimestamp + UA_DATETIME_SEC))
                            readOk = false;
                    }
                    /* See OPC UA Part 11, Version 1.03, Page 5-6, Table 1, Mark b for details.*/
                    if (current->result[l + reseivedValues] == TIMESTAMP_FIRST && current->start == TIMESTAMP_UNSPECIFIED) {
                        // This test will work on not continous read, only
                        if (reseivedValues == 0 && !(l > 0 && result[l].sourceTimestamp == result[l-1].sourceTimestamp - UA_DATETIME_SEC))
                            readOk = false;
                    }
                    if (!resultIsEqual(&result[l], current, l + reseivedValues))
                        readOk = false;
                }
                reseivedValues += resultSize;
            } else {
                readOk = false;
                hasMoreData = false;
                break;
            }
            UA_HistoryReadResponse_deleteMembers(&response);
        }
        UA_ByteString_deleteMembers(&continuous);
        if (!readOk) {
            fprintf(stderr, "} Fail\n");
            ++retval;
        } else {
            fprintf(stderr, "} OK\n");
        }
        current = &testRequests[++i];
    }
    return retval;
}

START_TEST(Server_HistorizingStrategyUser) {
    // set a data backend
    UA_HistorizingSetting setting;
    setting.historizingBackend = UA_HistoryDataBackend_Memory(3, 100);
    setting.maxHistoryDataResponseSize = 100;
    setting.historizingUpdateStrategy = UA_HISTORIZINGUPDATESTRATEGY_USER;
    UA_StatusCode retval = UA_Server_setHistorizingSettingToVariableNode(server, &outNodeId, setting);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    void *nodeIdContext = setting.historizingBackend.getNodeIdContext(setting.historizingBackend.context, &outNodeId);
    // fill the data
    UA_DateTime start = UA_DateTime_now();
    UA_DateTime end = start + (10 * UA_DATETIME_SEC);
    for (UA_UInt32 i = 0; i < 10; ++i) {
        UA_DataValue value;
        UA_DataValue_init(&value);
        value.hasValue = true;
        value.hasStatus = true;
        value.status = UA_STATUSCODE_GOOD;
        UA_Variant_setScalarCopy(&value.value, &i, &UA_TYPES[UA_TYPES_UINT32]);
        value.hasSourceTimestamp = true;
        value.sourceTimestamp = start + (i * UA_DATETIME_SEC);
        setting.historizingBackend.insertHistoryData(nodeIdContext, &value);
        UA_DataValue_deleteMembers(&value);
    }

    // request
    UA_HistoryReadResponse response;
    requestHistory(start, end, &response, 0, false, NULL);

    // test the response
    ck_assert_uint_eq(response.responseHeader.serviceResult, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(response.resultsSize, 1);
    for (size_t i = 0; i < response.resultsSize; ++i) {
        ck_assert_uint_eq(response.results[i].statusCode, UA_STATUSCODE_GOOD);
        ck_assert_uint_eq(response.results[i].historyData.encoding, UA_EXTENSIONOBJECT_DECODED);
        ck_assert(response.results[i].historyData.content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYDATA]);
        UA_HistoryData * data = (UA_HistoryData *)response.results[i].historyData.content.decoded.data;
        ck_assert_uint_eq(data->dataValuesSize, 10);
        for (size_t j = 0; j < data->dataValuesSize; ++j) {
            ck_assert_uint_eq(data->dataValues[j].hasSourceTimestamp, true);
            ck_assert_uint_eq(data->dataValues[j].sourceTimestamp, start + (j * UA_DATETIME_SEC));
            ck_assert_uint_eq(data->dataValues[j].hasStatus, true);
            ck_assert_uint_eq(data->dataValues[j].status, UA_STATUSCODE_GOOD);
            ck_assert_uint_eq(data->dataValues[j].hasValue, true);
            ck_assert(data->dataValues[j].value.type == &UA_TYPES[UA_TYPES_UINT32]);
            UA_UInt32 * value = (UA_UInt32 *)data->dataValues[j].value.data;
            ck_assert_uint_eq(*value, j);
        }
    }
    UA_HistoryDataBackend_Memory_deleteMembers(&setting.historizingBackend);
    UA_HistoryReadResponse_deleteMembers(&response);
}
END_TEST

START_TEST(Server_HistorizingStrategyPoll) {
    // init to a defined value
    UA_StatusCode retval = setUInt32(client, outNodeId, 43);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    // set data backend
    UA_HistorizingSetting setting;
    setting.historizingBackend = UA_HistoryDataBackend_Memory(3, 100);
    setting.maxHistoryDataResponseSize = 100;
    setting.historizingUpdateStrategy = UA_HISTORIZINGUPDATESTRATEGY_POLL;
    retval = UA_Server_setHistorizingSettingToVariableNode(server, &outNodeId, setting);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    // fill the data
    UA_DateTime start = UA_DateTime_now();
    retval = UA_Server_historizingPollingStart(server, &outNodeId, 5);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    retval = UA_Server_historizingPollingChangeInterval(server, &outNodeId, 10);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    for (size_t k = 0; k < 10; ++k) {
        UA_fakeSleep(15);
        UA_realSleep(15);
    }
    retval = UA_Server_historizingPollingStop(server, &outNodeId);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
    UA_DateTime end = UA_DateTime_now();

    // request
    UA_HistoryReadResponse response;
    requestHistory(start, end, &response, 0, false, NULL);

    // test the response
    ck_assert_uint_eq(response.responseHeader.serviceResult, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(response.resultsSize, 1);
    for (size_t i = 0; i < response.resultsSize; ++i) {
        ck_assert_uint_eq(response.results[i].statusCode, UA_STATUSCODE_GOOD);
        ck_assert_uint_eq(response.results[i].historyData.encoding, UA_EXTENSIONOBJECT_DECODED);
        ck_assert(response.results[i].historyData.content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYDATA]);
        UA_HistoryData * data = (UA_HistoryData *)response.results[i].historyData.content.decoded.data;
        ck_assert(data->dataValuesSize > 0);
        for (size_t j = 0; j < data->dataValuesSize; ++j) {
            ck_assert_uint_eq(data->dataValues[j].hasSourceTimestamp, true);
            ck_assert(data->dataValues[j].sourceTimestamp >= start);
            ck_assert(data->dataValues[j].sourceTimestamp < end);
            ck_assert_uint_eq(data->dataValues[j].hasValue, true);
            ck_assert(data->dataValues[j].value.type == &UA_TYPES[UA_TYPES_UINT32]);
            UA_UInt32 * value = (UA_UInt32 *)data->dataValues[j].value.data;
            ck_assert_uint_eq(*value, 43);
        }
    }
    UA_HistoryReadResponse_deleteMembers(&response);
    UA_HistoryDataBackend_Memory_deleteMembers(&setting.historizingBackend);
}
END_TEST

START_TEST(Server_HistorizingStrategyValueSet) {
    // init to a defined value
    UA_StatusCode retval = setUInt32(client, outNodeId, 43);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    // set data backend
    UA_HistorizingSetting setting;
    setting.historizingBackend = UA_HistoryDataBackend_Memory(3, 100);
    setting.maxHistoryDataResponseSize = 100;
    setting.historizingUpdateStrategy = UA_HISTORIZINGUPDATESTRATEGY_VALUESET;
    retval = UA_Server_setHistorizingSettingToVariableNode(server, &outNodeId, setting);
    ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);

    // fill the data
    UA_fakeSleep(100);
    UA_DateTime start = UA_DateTime_now();
    UA_fakeSleep(100);
    for (UA_UInt32 i = 0; i < 10; ++i) {
        retval = setUInt32(client, outNodeId, i);
        ck_assert_uint_eq(retval, UA_STATUSCODE_GOOD);
        UA_fakeSleep(100);
    }
    UA_DateTime end = UA_DateTime_now();

    // request
    UA_HistoryReadResponse response;
    requestHistory(start, end, &response, 0, false, NULL);

    // test the response
    ck_assert_uint_eq(response.responseHeader.serviceResult, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(response.resultsSize, 1);
    for (size_t i = 0; i < response.resultsSize; ++i) {
        ck_assert_uint_eq(response.results[i].statusCode, UA_STATUSCODE_GOOD);
        ck_assert_uint_eq(response.results[i].historyData.encoding, UA_EXTENSIONOBJECT_DECODED);
        ck_assert(response.results[i].historyData.content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYDATA]);
        UA_HistoryData * data = (UA_HistoryData *)response.results[i].historyData.content.decoded.data;
        ck_assert(data->dataValuesSize > 0);
        for (size_t j = 0; j < data->dataValuesSize; ++j) {
            ck_assert(data->dataValues[j].sourceTimestamp >= start && data->dataValues[j].sourceTimestamp < end);
            ck_assert_uint_eq(data->dataValues[j].hasSourceTimestamp, true);
            ck_assert_uint_eq(data->dataValues[j].status, UA_STATUSCODE_GOOD);
            ck_assert_uint_eq(data->dataValues[j].hasValue, true);
            ck_assert(data->dataValues[j].value.type == &UA_TYPES[UA_TYPES_UINT32]);
            UA_UInt32 * value = (UA_UInt32 *)data->dataValues[j].value.data;
            ck_assert_uint_eq(*value, j);
        }
    }
    UA_HistoryReadResponse_deleteMembers(&response);
    UA_HistoryDataBackend_Memory_deleteMembers(&setting.historizingBackend);
}
END_TEST

START_TEST(Server_HistorizingBackendMemory) {
    UA_HistoryDataBackend backend = UA_HistoryDataBackend_Memory(1, 1);

    // empty backend should not crash
    UA_UInt32 retval = testHistoricalDataBackend(&backend, 100);
    fprintf(stderr, "%d tests expected failed.\n", retval);

    // fill backend
    ck_assert_uint_eq(fillHistoricalDataBackend(&backend), true);

    // read all in one
    retval = testHistoricalDataBackend(&backend, 100);
    fprintf(stderr, "%d tests failed.\n", retval);
    ck_assert_uint_eq(retval, 0);

    // read continuous one at one request
    retval = testHistoricalDataBackend(&backend, 1);
    fprintf(stderr, "%d tests failed.\n", retval);
    ck_assert_uint_eq(retval, 0);

    // read continuous two at one request
    retval = testHistoricalDataBackend(&backend, 2);
    fprintf(stderr, "%d tests failed.\n", retval);
    ck_assert_uint_eq(retval, 0);

    UA_HistoryDataBackend_Memory_deleteMembers(&backend);
}
END_TEST

#endif /*UA_ENABLE_HISTORIZING*/

static Suite* testSuite_Client(void) {
    Suite *s = suite_create("Server Historical Data");
    TCase *tc_server = tcase_create("Server Historical Data Basic");
    tcase_add_checked_fixture(tc_server, setup, teardown);
#ifdef UA_ENABLE_HISTORIZING
    tcase_add_test(tc_server, Server_HistorizingStrategyPoll);
    tcase_add_test(tc_server, Server_HistorizingStrategyUser);
    tcase_add_test(tc_server, Server_HistorizingStrategyValueSet);
    tcase_add_test(tc_server, Server_HistorizingBackendMemory);
#endif /* UA_ENABLE_HISTORIZING */
    suite_add_tcase(s, tc_server);

    return s;
}

int main(void) {
    Suite *s = testSuite_Client();
    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr,CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
