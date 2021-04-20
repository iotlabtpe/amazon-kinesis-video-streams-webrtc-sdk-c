#define LOG_CLASS "WssApi"
#include "../Include_i.h"

#define WSS_API_ENTER()
#define WSS_API_EXIT()

#define API_ENDPOINT_TCP_PORT                    "443"
#define API_CALL_CONNECTION_TIMEOUT              (2 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define API_CALL_COMPLETION_TIMEOUT              (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define API_CALL_CONNECTING_RETRY                (3)
#define API_CALL_CONNECTING_RETRY_INTERVAL_IN_MS (1000)

#define HTTP_HEADER_FIELD_CONNECTION    "Connection"
#define HTTP_HEADER_FIELD_UPGRADE       "upgrade"
#define HTTP_HEADER_FIELD_SEC_WS_ACCEPT "sec-websocket-accept"
#define HTTP_HEADER_VALUE_UPGRADE       "upgrade"
#define HTTP_HEADER_VALUE_WS            "websocket"

// Parameterized string for WSS connect
#define URL_TEMPLATE_ENDPOINT_MASTER "%s?%s=%s"
#define URL_TEMPLATE_ENDPOINT_VIEWER "%s?%s=%s&%s=%s"
#define URL_PARAM_CHANNEL_ARN        "X-Amz-ChannelARN"
#define URL_PARAM_CLIENT_ID          "X-Amz-ClientId"

// Send message JSON template
#define WSS_MESSAGE_TEMPLATE                                                                                                                         \
    "{\n"                                                                                                                                            \
    "\t\"action\": \"%s\",\n"                                                                                                                        \
    "\t\"RecipientClientId\": \"%.*s\",\n"                                                                                                           \
    "\t\"MessagePayload\": \"%s\"\n"                                                                                                                 \
    "}"

// Send message JSON template with correlation id
#define WSS_MESSAGE_TEMPLATE_WITH_CORRELATION_ID                                                                                                     \
    "{\n"                                                                                                                                            \
    "\t\"action\": \"%s\",\n"                                                                                                                        \
    "\t\"RecipientClientId\": \"%.*s\",\n"                                                                                                           \
    "\t\"MessagePayload\": \"%s\",\n"                                                                                                                \
    "\t\"CorrelationId\": \"%.*s\"\n"                                                                                                                \
    "}"

STATUS wssConnectSignalingChannel(PSignalingClient pSignalingClient, UINT64 time)
{
    WSS_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    /* Variables for network connection */
    NetworkContext_t* pNetworkContext = NULL;
    SIZE_T uConnectionRetryCnt = 0;
    UINT32 uBytesToSend = 0, uBytesReceived = 0;

    /* Variables for HTTP request */
    PCHAR pUrl = NULL;
    PRequestInfo pRequestInfo = NULL;
    CHAR clientKey[WSS_CLIENT_BASED64_RANDOM_SEED_LEN + 1];
    PCHAR pHost = NULL;

    UINT32 uHttpStatusCode = 0;
    HttpResponseContext* pHttpRspCtx = NULL;
    UINT32 urlLen = 0;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelEndpointWss[0] != '\0', STATUS_INTERNAL_ERROR);
    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    CHK(NULL != (pHost = (CHAR*) MEMALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN)), STATUS_NOT_ENOUGH_MEMORY);

    // Prepare the json params for the call
    if (pSignalingClient->pChannelInfo->channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        urlLen = STRLEN(URL_TEMPLATE_ENDPOINT_VIEWER) + STRLEN(pSignalingClient->channelEndpointWss) + STRLEN(URL_PARAM_CHANNEL_ARN) +
            STRLEN(pSignalingClient->channelDescription.channelArn) + STRLEN(URL_PARAM_CLIENT_ID) +
            STRLEN(pSignalingClient->clientInfo.signalingClientInfo.clientId) + 1;
        CHK(NULL != (pUrl = (PCHAR) MEMALLOC(urlLen)), STATUS_NOT_ENOUGH_MEMORY);
        SNPRINTF(pUrl, urlLen, URL_TEMPLATE_ENDPOINT_VIEWER, pSignalingClient->channelEndpointWss, URL_PARAM_CHANNEL_ARN,
                 pSignalingClient->channelDescription.channelArn, URL_PARAM_CLIENT_ID, pSignalingClient->clientInfo.signalingClientInfo.clientId);
    } else {
        urlLen = STRLEN(URL_TEMPLATE_ENDPOINT_MASTER) + STRLEN(pSignalingClient->channelEndpointWss) + STRLEN(URL_PARAM_CHANNEL_ARN) +
            STRLEN(pSignalingClient->channelDescription.channelArn) + 1;
        CHK(NULL != (pUrl = (PCHAR) MEMALLOC(urlLen)), STATUS_NOT_ENOUGH_MEMORY);

        SNPRINTF(pUrl, urlLen, URL_TEMPLATE_ENDPOINT_MASTER, pSignalingClient->channelEndpointWss, URL_PARAM_CHANNEL_ARN,
                 pSignalingClient->channelDescription.channelArn);
    }

    /* Initialize and generate HTTP request, then send it. */
    CHK(NULL != (pNetworkContext = (NetworkContext_t*) MEMALLOC(sizeof(NetworkContext_t))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(initNetworkContext(pNetworkContext));

    CHK_STATUS(createRequestInfo(pUrl, NULL, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent, API_CALL_CONNECTION_TIMEOUT,
                                 API_CALL_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT,
                                 pSignalingClient->pAwsCredentials, &pRequestInfo));
    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;
    MEMSET(clientKey, 0, WSS_CLIENT_BASED64_RANDOM_SEED_LEN + 1);
    wssClientGenerateClientKey(clientKey, WSS_CLIENT_BASED64_RANDOM_SEED_LEN + 1);
    httpPackSendBuf(pRequestInfo, HTTP_REQUEST_VERB_GET_STRING, pHost, MAX_CONTROL_PLANE_URI_CHAR_LEN, (PCHAR) pNetworkContext->pHttpSendBuffer,
                    MAX_HTTP_SEND_BUFFER_LEN, TRUE, clientKey);

    for (uConnectionRetryCnt = 0; uConnectionRetryCnt < API_CALL_CONNECTING_RETRY; uConnectionRetryCnt++) {
        if ((retStatus = connectToServer(pNetworkContext, pHost, API_ENDPOINT_TCP_PORT)) == STATUS_SUCCESS) {
            break;
        }
        THREAD_SLEEP(API_CALL_CONNECTING_RETRY_INTERVAL_IN_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    CHK_STATUS(retStatus);

    uBytesToSend = STRLEN((PCHAR) pNetworkContext->pHttpSendBuffer);
    CHK(uBytesToSend == networkSend(pNetworkContext, pNetworkContext->pHttpSendBuffer, uBytesToSend), STATUS_SEND_DATA_FAILED);
    uBytesReceived = networkRecv(pNetworkContext, pNetworkContext->pHttpRecvBuffer, pNetworkContext->uHttpRecvBufferLen);
    CHK(uBytesReceived > 0, STATUS_RECV_DATA_FAILED);

    struct list_head* requiredHeader = malloc(sizeof(struct list_head));
    INIT_LIST_HEAD(requiredHeader);
    httpParserAddRequiredHeader(requiredHeader, HTTP_HEADER_FIELD_CONNECTION, STRLEN(HTTP_HEADER_FIELD_CONNECTION), NULL, 0);
    httpParserAddRequiredHeader(requiredHeader, HTTP_HEADER_FIELD_UPGRADE, STRLEN(HTTP_HEADER_FIELD_UPGRADE), NULL, 0);
    httpParserAddRequiredHeader(requiredHeader, HTTP_HEADER_FIELD_SEC_WS_ACCEPT, STRLEN(HTTP_HEADER_FIELD_SEC_WS_ACCEPT), NULL, 0);
    CHK_STATUS(httpParserStart(&pHttpRspCtx, (CHAR*) pNetworkContext->pHttpRecvBuffer, (UINT32) uBytesReceived, requiredHeader));

    PHttpField node;
    node = httpParserGetValueByField(requiredHeader, HTTP_HEADER_FIELD_CONNECTION, STRLEN(HTTP_HEADER_FIELD_CONNECTION));
    CHK(node != NULL && node->valueLen == STRLEN(HTTP_HEADER_VALUE_UPGRADE) && MEMCMP(node->value, HTTP_HEADER_VALUE_UPGRADE, node->valueLen) == 0,
        STATUS_WSS_UPGRADE_CONNECTION_ERROR);

    node = httpParserGetValueByField(requiredHeader, HTTP_HEADER_FIELD_UPGRADE, STRLEN(HTTP_HEADER_FIELD_UPGRADE));
    CHK(node != NULL && node->valueLen == STRLEN(HTTP_HEADER_VALUE_WS) && MEMCMP(node->value, HTTP_HEADER_VALUE_WS, node->valueLen) == 0,
        STATUS_WSS_UPGRADE_PROTOCOL_ERROR);

    node = httpParserGetValueByField(requiredHeader, HTTP_HEADER_FIELD_SEC_WS_ACCEPT, STRLEN(HTTP_HEADER_FIELD_SEC_WS_ACCEPT));
    CHK(node != NULL && wssClientValidateAcceptKey(clientKey, WSS_CLIENT_BASED64_RANDOM_SEED_LEN, node->value, node->valueLen) == STATUS_SUCCESS,
        STATUS_WSS_ACCEPT_KEY_ERROR);

    uHttpStatusCode = httpParserGetHttpStatusCode(pHttpRspCtx);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) uHttpStatusCode);

    /* Check HTTP results */
    if (uHttpStatusCode == 101) {
        TID threadId;
        /**
         * switch to wss client.
         */
        /* We got a success response here. */
        WssClientContext* wssClientCtx = NULL;
        wssClientCreate(&wssClientCtx, pNetworkContext, pSignalingClient, wssHandleDataMsg, wssHandleCtrlMsg);
        pSignalingClient->pWssContext = wssClientCtx;

        CHK_STATUS(THREAD_CREATE(&threadId, wssClientStart, (PVOID) wssClientCtx));
        CHK_STATUS(THREAD_DETACH(threadId));

        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
        ATOMIC_STORE_BOOL(&pSignalingClient->connected, TRUE);

        if (ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
        }
    }
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK, retStatus);

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (pHttpRspCtx != NULL) {
        retStatus = httpParserDetroy(pHttpRspCtx);
        if (retStatus != STATUS_SUCCESS) {
            DLOGD("destroying http parset failed.");
        }
    }

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        // Fix-up the timeout case
        SERVICE_CALL_RESULT serviceCallResult =
            (retStatus == STATUS_OPERATION_TIMED_OUT) ? SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT : SERVICE_CALL_UNKNOWN;
        // Trigger termination
        if (pNetworkContext != NULL) {
            disconnectFromServer(pNetworkContext);
            terminateNetworkContext(pNetworkContext);
            MEMFREE(pNetworkContext);
        }

        if (pSignalingClient->pWssContext != NULL) {
            wssTerminateConnection(pSignalingClient, serviceCallResult);
        }

        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) serviceCallResult);
    }
    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    freeRequestInfo(pRequestInfo);
    WSS_API_EXIT();
    return retStatus;
}

/**
 * @brief
 *
 * @param[in] pSignalingClient
 * @param[in] awaitForResponse
 *
 * @return
 */
STATUS wssWriteData(PSignalingClient pSignalingClient, PBYTE pSendBuf, UINT32 bufLen, BOOL awaitForResponse)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pSignalingClient != NULL && pSignalingClient->pWssContext != NULL, STATUS_NULL_ARG);

    DLOGD("Sending data over web socket: %s", pSendBuf);
    CHK_STATUS(wssClientSendText(pSignalingClient->pWssContext, pSendBuf, bufLen));

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS wssSendMessage(PSignalingClient pSignalingClient, PCHAR pMessageType, PCHAR peerClientId, PCHAR pMessage, UINT32 messageLen,
                      PCHAR pCorrelationId, UINT32 correlationIdLen)
{
    WSS_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;

    PCHAR pEncodedMessage = NULL;
    UINT32 size, writtenSize, correlationLen;
    BOOL awaitForResponse;
    PBYTE pSendBuffer = NULL;

    // Ensure we are in a connected state
    CHK_STATUS(signalingFsmAccept(pSignalingClient, SIGNALING_STATE_CONNECTED));
    CHK(pSignalingClient != NULL && pSignalingClient->pWssContext != NULL, STATUS_NULL_ARG);
    // #YC_TBD, need to enhance, #heap.
    CHK(NULL != (pEncodedMessage = (PCHAR) MEMALLOC(MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1)), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pSendBuffer = (PBYTE) MEMALLOC(LWS_MESSAGE_BUFFER_SIZE)), STATUS_NOT_ENOUGH_MEMORY);

    // Calculate the lengths if not specified
    if (messageLen == 0) {
        size = (UINT32) STRLEN(pMessage);
    } else {
        size = messageLen;
    }

    if (correlationIdLen == 0) {
        correlationLen = (UINT32) STRLEN(pCorrelationId);
    } else {
        correlationLen = correlationIdLen;
    }

    // Base64 encode the message
    writtenSize = MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1;
    CHK_STATUS(base64Encode(pMessage, size, pEncodedMessage, &writtenSize));

    // Account for the template expansion + Action string + max recipient id
    size = LWS_MESSAGE_BUFFER_SIZE;
    CHK(writtenSize <= size, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);

    // Prepare json message
    if (correlationLen == 0) {
        writtenSize = (UINT32) SNPRINTF((PCHAR)(pSendBuffer), size, WSS_MESSAGE_TEMPLATE, pMessageType, MAX_SIGNALING_CLIENT_ID_LEN, peerClientId,
                                        pEncodedMessage);
    } else {
        writtenSize = (UINT32) SNPRINTF((PCHAR)(pSendBuffer), size, WSS_MESSAGE_TEMPLATE_WITH_CORRELATION_ID, pMessageType,
                                        MAX_SIGNALING_CLIENT_ID_LEN, peerClientId, pEncodedMessage, correlationLen, pCorrelationId);
    }

    // Validate against max
    CHK(writtenSize <= LWS_MESSAGE_BUFFER_SIZE, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);
    writtenSize *= SIZEOF(CHAR);
    CHK(writtenSize <= size, STATUS_INVALID_ARG);
    // Send the data to the web socket
    CHK_STATUS(wssWriteData(pSignalingClient, pSendBuffer, writtenSize, FALSE));

CleanUp:
    SAFE_MEMFREE(pEncodedMessage);
    SAFE_MEMFREE(pSendBuffer);
    WSS_API_EXIT();
    return retStatus;
}

STATUS wssGetMessageTypeFromString(PCHAR typeStr, UINT32 typeLen, SIGNALING_MESSAGE_TYPE* pMessageType)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 len;

    CHK(typeStr != NULL && pMessageType != NULL, STATUS_NULL_ARG);

    if (typeLen == 0) {
        len = (UINT32) STRLEN(typeStr);
    } else {
        len = typeLen;
    }

    if (0 == STRNCMP(typeStr, SIGNALING_SDP_TYPE_OFFER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_OFFER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_SDP_TYPE_ANSWER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_ICE_CANDIDATE, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    } else if (0 == STRNCMP(typeStr, SIGNALING_GO_AWAY, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_GO_AWAY;
    } else if (0 == STRNCMP(typeStr, SIGNALING_RECONNECT_ICE_SERVER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_STATUS_RESPONSE, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE;
    } else {
        *pMessageType = SIGNALING_MESSAGE_TYPE_UNKNOWN;
        CHK_WARN(FALSE, retStatus, "Unrecognized message type received");
    }

CleanUp:

    LEAVES();
    return retStatus;
}

PVOID wssReceiveMessageWrapper(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingMessageWrapper pSignalingMessageWrapper = (PSignalingMessageWrapper) args;
    PSignalingClient pSignalingClient = NULL;

    CHK(pSignalingMessageWrapper != NULL, STATUS_NULL_ARG);

    pSignalingClient = pSignalingMessageWrapper->pSignalingClient;

    CHK(pSignalingClient != NULL, STATUS_INTERNAL_ERROR);

    // Updating the diagnostics info before calling the client callback
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfMessagesReceived);

    // Calling client receive message callback if specified
    if (pSignalingClient->signalingClientCallbacks.messageReceivedFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.messageReceivedFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                                &pSignalingMessageWrapper->receivedSignalingMessage));
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    SAFE_MEMFREE(pSignalingMessageWrapper);

    return (PVOID)(ULONG_PTR) retStatus;
}
#if defined(KVS_PLAT_ESP_FREERTOS) || defined(KVS_PLAT_RTK_FREERTOS)
/** #YC_TBD, need to add the code of initialization. */
TID receivedTid = INVALID_TID_VALUE;
QueueHandle_t lwsMsgQ = NULL;
#define KVSWEBRTC_LWS_MSGQ_LENGTH 32

/**
 * @brief for the original design, we create one thread for each message.
 */
PVOID wssHandleMsg(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingMessageWrapper pMsg;
    while (1) {
        BaseType_t err = xQueueReceive(lwsMsgQ, &pMsg, 0xffffffffUL);
        if (err == pdPASS) {
            DLOGD("handling wss");
            retStatus = STATUS_SUCCESS;

            PSignalingClient pSignalingClient = NULL;

            CHK(pMsg != NULL, STATUS_NULL_ARG);

            pSignalingClient = pMsg->pSignalingClient;

            CHK(pSignalingClient != NULL, STATUS_INTERNAL_ERROR);
            // Calling client receive message callback if specified
            if (pSignalingClient->signalingClientCallbacks.messageReceivedFn != NULL) {
                CHK_STATUS(pSignalingClient->signalingClientCallbacks.messageReceivedFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                                        &pMsg->receivedSignalingMessage));
            }
        CleanUp:
            CHK_LOG_ERR(retStatus);
            SAFE_MEMFREE(pMsg);
        } else {
            DLOGW("Did not get the lws msg.");
        }
    }
    DLOGW("should not happen.");
    return (PVOID)(ULONG_PTR) retStatus;
}

STATUS wssDispatchMsg(PVOID pMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingMessageWrapper msg = (PSignalingMessageWrapper) pMessage;

    if (receivedTid == INVALID_TID_VALUE) {
        lwsMsgQ = xQueueCreate(KVSWEBRTC_LWS_MSGQ_LENGTH, SIZEOF(PSignalingMessageWrapper));
        CHK(lwsMsgQ != NULL, STATUS_SIGNALING_CREATE_MSGQ_FAILED);
        CHK(THREAD_CREATE(&receivedTid, wssHandleMsg, (PVOID) NULL) == STATUS_SUCCESS, STATUS_SIGNALING_CREATE_THREAD_FAILED);
    }
    UBaseType_t num = uxQueueSpacesAvailable(lwsMsgQ);
    DLOGD("unhandled num in q: %d", KVSWEBRTC_LWS_MSGQ_LENGTH - num);
    CHK(xQueueSend(lwsMsgQ, &msg, 0) == pdPASS, STATUS_SIGNALING_DISPATCH_FAILED);

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (STATUS_FAILED(retStatus)) {
        SAFE_MEMFREE(msg);
    }
    return retStatus;
}
#endif

STATUS wssHandleDataMsg(PSignalingClient pSignalingClient, PCHAR pMessage, UINT32 messageLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingMessageWrapper pSignalingMessageWrapper = NULL;
#if !defined(KVS_PLAT_ESP_FREERTOS) && !defined(KVS_PLAT_RTK_FREERTOS)
    TID receivedTid = INVALID_TID_VALUE;
#endif
    PSignalingMessage pOngoingMessage;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // If we have a signalingMessage and if there is a correlation id specified then the response should be non-empty
    if (pMessage == NULL || messageLen == 0) {
        // Check if anything needs to be done
        CHK_WARN(pMessage != NULL && messageLen != 0, retStatus, "Signaling received an empty message");
    }

    CHK(NULL != (pSignalingMessageWrapper = (PSignalingMessageWrapper) MEMCALLOC(1, SIZEOF(SignalingMessageWrapper))), STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(wssApiRspReceivedMessage(pMessage, messageLen, pSignalingMessageWrapper));

    pSignalingMessageWrapper->pSignalingClient = pSignalingClient;

    switch (pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE:
            // ATOMIC_STORE(&pSignalingClient->messageResult,
            //                 (SIZE_T) getServiceCallResultFromHttpStatus(pSignalingMessageWrapper->receivedSignalingMessage.statusCode));
            if (pSignalingMessageWrapper->receivedSignalingMessage.statusCode != SERVICE_CALL_RESULT_OK) {
                DLOGW("Failed to deliver message. Correlation ID: %s, Error Type: %s, Error Code: %u, Description: %s",
                      pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.correlationId,
                      pSignalingMessageWrapper->receivedSignalingMessage.errorType, pSignalingMessageWrapper->receivedSignalingMessage.statusCode,
                      pSignalingMessageWrapper->receivedSignalingMessage.description);
            }
            // Notify the awaiting send
            // CVAR_BROADCAST(pSignalingClient->receiveCvar);
            // Delete the message wrapper and exit
            SAFE_MEMFREE(pSignalingMessageWrapper);
            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_GO_AWAY:
            // Move the describe state
            CHK_STATUS(wssTerminateConnection(pSignalingClient, SERVICE_CALL_RESULT_SIGNALING_GO_AWAY));

            // Delete the message wrapper and exit
            SAFE_MEMFREE(pSignalingMessageWrapper);

            // Iterate the state machinery
            CHK_STATUS(signalingFsmStep(pSignalingClient, retStatus));

            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER:
            // Move to get ice config state
            CHK_STATUS(wssTerminateConnection(pSignalingClient, SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE));

            // Delete the message wrapper and exit
            SAFE_MEMFREE(pSignalingMessageWrapper);

            // Iterate the state machinery
            CHK_STATUS(signalingFsmStep(pSignalingClient, retStatus));

            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_OFFER:
            CHK(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.peerClientId[0] != '\0',
                STATUS_SIGNALING_NO_PEER_CLIENT_ID_IN_MESSAGE);
            // Explicit fall-through !!!
        case SIGNALING_MESSAGE_TYPE_ANSWER:
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            CHK(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payloadLen > 0 &&
                    pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payloadLen <= MAX_SIGNALING_MESSAGE_LEN,
                STATUS_SIGNALING_INVALID_PAYLOAD_LEN_IN_MESSAGE);
            CHK(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payload[0] != '\0', STATUS_SIGNALING_NO_PAYLOAD_IN_MESSAGE);
            break;

        default:
            break;
    }

#if !defined(KVS_PLAT_ESP_FREERTOS) && !defined(KVS_PLAT_RTK_FREERTOS)
    // Issue the callback on a separate thread
    CHK_STATUS(THREAD_CREATE(&receivedTid, wssReceiveMessageWrapper, (PVOID) pSignalingMessageWrapper));
    CHK_STATUS(THREAD_DETACH(receivedTid));
#else
    CHK_STATUS(wssDispatchMsg((PVOID) pSignalingMessageWrapper));
#endif

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pSignalingClient != NULL && STATUS_FAILED(retStatus)) {
        ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfRuntimeErrors);
        if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
            retStatus = pSignalingClient->signalingClientCallbacks.errorReportFn(pSignalingClient->signalingClientCallbacks.customData, retStatus,
                                                                                 pMessage, messageLen);
        }
#if !defined(KVS_PLAT_ESP_FREERTOS) && !defined(KVS_PLAT_RTK_FREERTOS)
        // Kill the receive thread on error
        if (IS_VALID_TID_VALUE(receivedTid)) {
            THREAD_CANCEL(receivedTid);
        }

        SAFE_MEMFREE(pSignalingMessageWrapper);
#endif
    }
    LEAVES();
    return retStatus;
}

STATUS wssHandleCtrlMsg(PSignalingClient pSignalingClient, UINT8 opcode, PCHAR pMessage, UINT32 messageLen)
{
    WSS_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL connected;
    PCHAR pCurPtr;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    if (opcode == WSLAY_PONG) {
        DLOGD("<== pong, len: %ld", messageLen);
    } else if (opcode == WSLAY_PING) {
        DLOGD("<== ping, len: %ld", messageLen);
    } else if (opcode == WSLAY_CONNECTION_CLOSE) {
        DLOGD("<== connection close, len: %ld, reason:%s", messageLen, pMessage);
        pCurPtr = pMessage == NULL ? "(None)" : (PCHAR) pMessage;
        DLOGW("Client connection failed. Connection error string: %s", pCurPtr);

        connected = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->connected, FALSE);
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);

        // Update the diagnostics info
        ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfReconnects);
        CHK_STATUS(signalingFsmStep(pSignalingClient, retStatus));
        if (pSignalingClient != NULL) {
            // Call the error handler in case of an error
            if (STATUS_FAILED(retStatus)) {
                // Update the diagnostics before calling the error callback
                ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfRuntimeErrors);
                if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
                    // reconnectErrLen = SNPRINTF(pReconnectErrMsg, SIGNALING_MAX_ERROR_MESSAGE_LEN, SIGNALING_RECONNECT_ERROR_MSG, retStatus);
                    // pReconnectErrMsg[SIGNALING_MAX_ERROR_MESSAGE_LEN] = '\0';
                    // pSignalingClient->signalingClientCallbacks.errorReportFn(pSignalingClient->signalingClientCallbacks.customData,
                    //                                                         STATUS_SIGNALING_RECONNECT_FAILED, pReconnectErrMsg, reconnectErrLen);
                }
            }
            // if (connected && !ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown)) {
            //    // Handle re-connection in a reconnect handler thread
            //    CHK_STATUS(THREAD_CREATE(&pSignalingClient->reconnecterTracker.threadId, lwsReconnectHandler, (PVOID) pSignalingClient));
            //    CHK_STATUS(THREAD_DETACH(pSignalingClient->reconnecterTracker.threadId));
            //}
        }
    } else {
        DLOGD("<== ctrl msg(%d), len: %ld", opcode, messageLen);
    }

CleanUp:

    WSS_API_EXIT();
    return retStatus;
}

/**
 * @brief   terminate the websocket connection but will set the result of signaling client for the next step.
 *
 * @param[in]
 * @param[in]
 *
 * @return
 */
STATUS wssTerminateConnection(PSignalingClient pSignalingClient, SERVICE_CALL_RESULT callResult)
{
    WSS_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) callResult);

    // waiting the termination of listener thread.
    wssClientClose(pSignalingClient->pWssContext);
    pSignalingClient->pWssContext = NULL;

CleanUp:

    WSS_API_EXIT();
    return retStatus;
}
