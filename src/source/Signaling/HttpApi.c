#define LOG_CLASS "HttpApi"
#include "../Include_i.h"

#define HTTP_API_ENTER()
#define HTTP_API_EXIT()

#define API_ENDPOINT_TCP_PORT                    "443"
#define API_CALL_CONNECTION_TIMEOUT              (2 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define API_CALL_COMPLETION_TIMEOUT              (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define API_CALL_CONNECTING_RETRY                (3)
#define API_CALL_CONNECTING_RETRY_INTERVAL_IN_MS (1000)
#define API_CALL_CHANNEL_PROTOCOL                "\"WSS\", \"HTTPS\""

// API postfix definitions
// https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_CreateSignalingChannel.html
#define API_CREATE_SIGNALING_CHANNEL "/createSignalingChannel"
// https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_DescribeSignalingChannel.html
#define API_DESCRIBE_SIGNALING_CHANNEL "/describeSignalingChannel"
// https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_GetSignalingChannelEndpoint.html
#define API_GET_SIGNALING_CHANNEL_ENDPOINT "/getSignalingChannelEndpoint"
// https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_DeleteSignalingChannel.html
#define API_DELETE_SIGNALING_CHANNEL "/deleteSignalingChannel"
// https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_AWSAcuitySignalingService_GetIceServerConfig.html
#define API_GET_ICE_CONFIG "/v1/get-ice-server-config"
// https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_ListSignalingChannels.html
#define API_LIST_SIGNALING_CHANNEL "/listSignalingChannels"
// https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_UpdateSignalingChannel.html
#define API_UPDATE_SIGNALING_CHANNEL "/updateSignalingChannel"
/**
 * @brief   API_CreateSignalingChannel
 * POST /createSignalingChannel HTTP/1.1
 * Content-type: application/json
 *
 * {
 *    "ChannelName": "string",
 *    "ChannelType": "string",
 *    "SingleMasterConfiguration": {
 *       "MessageTtlSeconds": number
 *    },
 *    "Tags": [
 *       {
 *          "Key": "string",
 *          "Value": "string"
 *       }
 *    ]
 * }
 */
// #YC_TBD, need to add the rest part.
#define BODY_TEMPLATE_CREATE_CHANNEL                                                                                                                 \
    "{\n\t\"ChannelName\": \"%s\""                                                                                                                   \
    "\n}"

// Parameterized string for TagStream API - we should have at least one tag
#define BODY_TEMPLATE_TAGS ",\n\t\"Tags\": [%s\n\t]"
#define BODY_TEMPLATE_TAG  "\n\t\t{\"Key\": \"%s\", \"Value\": \"%s\"},"
/**
 * @brief   API_DescribeSignalingChannel
 * POST /describeSignalingChannel HTTP/1.1
 * Content-type: application/json
 *
 * {
 *    "ChannelARN": "string",
 *    "ChannelName": "string"
 * }
 */
#define BODY_TEMPLATE_DESCRIBE_CHANNEL "{\n\t\"ChannelName\": \"%s\"\n}"
/**
 * @brief   API_GetSignalingChannelEndpoint
 * POST /getSignalingChannelEndpoint HTTP/1.1
 * Content-type: application/json
 *
 * {
 *    "ChannelARN": "string",
 *    "SingleMasterChannelEndpointConfiguration": {
 *       "Protocols": [ "string" ],
 *       "Role": "string"
 *    }
 * }
 */
#define BODY_TEMPLATE_GET_CHANNEL_ENDPOINT                                                                                                           \
    "{\n\t\"ChannelARN\": \"%s\","                                                                                                                   \
    "\n\t\"SingleMasterChannelEndpointConfiguration\": {"                                                                                            \
    "\n\t\t\"Protocols\": [%s],"                                                                                                                     \
    "\n\t\t\"Role\": \"%s\""                                                                                                                         \
    "\n\t}\n}"
/**
 * @brief   API_DeleteSignalingChannel
 * POST /deleteSignalingChannel HTTP/1.1
 * Content-type: application/json
 *
 * {
 *    "ChannelARN": "string",
 *    "CurrentVersion": "string"
 * }
 */
#define BODY_TEMPLATE_DELETE_CHANNEL                                                                                                                 \
    "{\n\t\"ChannelARN\": \"%s\","                                                                                                                   \
    "\n\t\"CurrentVersion\": \"%s\"\n}"
/**
 * @brief   API_AWSAcuitySignalingService_GetIceServerConfig
 * POST /v1/get-ice-server-config HTTP/1.1
 * Content-type: application/json
 *
 * {
 *    "ChannelARN": "string",
 *    "ClientId": "string",
 *    "Service": "string",
 *    "Username": "string"
 * }
 */
#define BODY_TEMPLATE_GET_ICE_CONFIG                                                                                                                 \
    "{\n\t\"ChannelARN\": \"%s\","                                                                                                                   \
    "\n\t\"ClientId\": \"%s\","                                                                                                                      \
    "\n\t\"Service\": \"TURN\""                                                                                                                      \
    "\n}"
/**
 * @brief   API_ListSignalingChannels
 * POST /listSignalingChannels HTTP/1.1
 * Content-type: application/json
 *
 * {
 *    "ChannelNameCondition": {
 *       "ComparisonOperator": "string",
 *       "ComparisonValue": "string"
 *    },
 *    "MaxResults": number,
 *    "NextToken": "string"
 * }
 */
//#define BODY_TEMPLATE_LIST_CHANNELS

/**
 * @brief   API_UpdateSignalingChannel
 * POST /updateSignalingChannel HTTP/1.1
 * Content-type: application/json
 *
 * {
 *    "ChannelARN": "string",
 *    "CurrentVersion": "string",
 *    "SingleMasterConfiguration": {
 *       "MessageTtlSeconds": number
 *    }
 * }
 */
//#define BODY_TEMPLATE_UPDATE_CHANNEL
// #YC_TBD.
#define MAX_STRLEN_OF_UINT32 (10)

//////////////////////////////////////////////////////////////////////////
// API calls
//////////////////////////////////////////////////////////////////////////
STATUS httpApiCreateChannl(PSignalingClient pSignalingClient, UINT64 time)
{
    HTTP_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    PChannelInfo pChannelInfo = pSignalingClient->pChannelInfo;

    /* Variables for network connection */
    NetworkContext_t* pNetworkContext = NULL;
    SIZE_T uConnectionRetryCnt = 0;
    UINT32 uBytesToSend = 0, uBytesReceived = 0;

    /* Variables for HTTP request */
    PCHAR pUrl = NULL;
    PRequestInfo pRequestInfo = NULL;
    PCHAR pHttpBody = NULL;
    UINT32 httpBodyLen;
    PCHAR pHost = NULL;
    // rsp
    UINT32 uHttpStatusCode = 0;
    HttpResponseContext* pHttpRspCtx = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;

    CHK(NULL != (pHost = (PCHAR) MEMALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN)), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pUrl = (PCHAR) MEMALLOC(STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(API_CREATE_SIGNALING_CHANNEL) + 1)),
        STATUS_NOT_ENOUGH_MEMORY);
    // Create the API url
    STRCPY(pUrl, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(pUrl, API_CREATE_SIGNALING_CHANNEL);
    httpBodyLen = SIZEOF(BODY_TEMPLATE_CREATE_CHANNEL) + STRLEN(pChannelInfo->pChannelName) + 1;
    CHK(NULL != (pHttpBody = (CHAR*) MEMALLOC(httpBodyLen)), STATUS_NOT_ENOUGH_MEMORY);

    /* generate HTTP request body */
    SNPRINTF(pHttpBody, httpBodyLen, BODY_TEMPLATE_CREATE_CHANNEL, pChannelInfo->pChannelName);
    // Create the request info with the body
    CHK_STATUS(createRequestInfo(pUrl, pHttpBody, pSignalingClient->pChannelInfo->pRegion, (PCHAR) pSignalingClient->pChannelInfo->pCertPath, NULL,
                                 NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent, API_CALL_CONNECTION_TIMEOUT,
                                 API_CALL_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT,
                                 pSignalingClient->pAwsCredentials, &pRequestInfo));

    /* Initialize and generate HTTP request, then send it. */
    CHK(NULL != (pNetworkContext = (NetworkContext_t*) MEMALLOC(SIZEOF(NetworkContext_t))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(initNetworkContext(pNetworkContext));

    httpPackSendBuf(pRequestInfo, HTTP_REQUEST_VERB_POST_STRING, pHost, MAX_CONTROL_PLANE_URI_CHAR_LEN, (PCHAR) pNetworkContext->pHttpSendBuffer,
                    MAX_HTTP_SEND_BUFFER_LEN, FALSE, NULL);

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

    CHK_STATUS(httpParserStart(&pHttpRspCtx, (CHAR*) pNetworkContext->pHttpRecvBuffer, (UINT32) uBytesReceived, NULL));

    pResponseStr = httpParserGetHttpBodyLocation(pHttpRspCtx);
    resultLen = httpParserGetHttpBodyLength(pHttpRspCtx);
    uHttpStatusCode = httpParserGetHttpStatusCode(pHttpRspCtx);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) uHttpStatusCode);
    /* Check HTTP results */

    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);

    CHK_STATUS(httpApiRspCreateChannel((const CHAR*) pResponseStr, resultLen, pSignalingClient));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pNetworkContext != NULL) {
        disconnectFromServer(pNetworkContext);
        terminateNetworkContext(pNetworkContext);
        MEMFREE(pNetworkContext);
    }

    if (pHttpRspCtx != NULL) {
        retStatus = httpParserDetroy(pHttpRspCtx);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("destroying http parset failed. \n");
        }
    }
    SAFE_MEMFREE(pHttpBody);
    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    freeRequestInfo(&pRequestInfo);

    HTTP_API_EXIT();
    return retStatus;
}

STATUS httpApiDescribeChannel(PSignalingClient pSignalingClient, UINT64 time)
{
    HTTP_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    PChannelInfo pChannelInfo = pSignalingClient->pChannelInfo;

    /* Variables for network connection */
    NetworkContext_t* pNetworkContext = NULL;
    SIZE_T uConnectionRetryCnt = 0;
    UINT32 uBytesToSend = 0, uBytesReceived = 0;

    /* Variables for HTTP request */
    // http req.
    PCHAR pUrl = NULL;
    PRequestInfo pRequestInfo = NULL;
    PCHAR pHttpBody = NULL;
    UINT32 httpBodyLen;
    PCHAR pHost = NULL;
    // rsp
    UINT32 uHttpStatusCode = 0;
    HttpResponseContext* pHttpRspCtx = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;

    CHK(NULL != (pHost = (PCHAR) MEMALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN)), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pUrl = (PCHAR) MEMALLOC(STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(API_DESCRIBE_SIGNALING_CHANNEL) + 1)),
        STATUS_NOT_ENOUGH_MEMORY);
    httpBodyLen = STRLEN(BODY_TEMPLATE_DESCRIBE_CHANNEL) + STRLEN(pSignalingClient->pChannelInfo->pChannelName) + 1;
    CHK(NULL != (pHttpBody = (PCHAR) MEMALLOC(httpBodyLen)), STATUS_NOT_ENOUGH_MEMORY);
    // Create the http url
    STRCPY(pUrl, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(pUrl, API_DESCRIBE_SIGNALING_CHANNEL);
    // create the http body
    SNPRINTF(pHttpBody, httpBodyLen, BODY_TEMPLATE_DESCRIBE_CHANNEL, pSignalingClient->pChannelInfo->pChannelName);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(pUrl, pHttpBody, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent, API_CALL_CONNECTION_TIMEOUT,
                                 API_CALL_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT,
                                 pSignalingClient->pAwsCredentials, &pRequestInfo));

    /* Initialize and generate HTTP request, then send it. */
    CHK(NULL != (pNetworkContext = (NetworkContext_t*) MEMALLOC(SIZEOF(NetworkContext_t))), STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(initNetworkContext(pNetworkContext));

    httpPackSendBuf(pRequestInfo, HTTP_REQUEST_VERB_POST_STRING, pHost, MAX_CONTROL_PLANE_URI_CHAR_LEN, (PCHAR) pNetworkContext->pHttpSendBuffer,
                    MAX_HTTP_SEND_BUFFER_LEN, FALSE, NULL);

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

    CHK_STATUS(httpParserStart(&pHttpRspCtx, (CHAR*) pNetworkContext->pHttpRecvBuffer, (UINT32) uBytesReceived, NULL));
    pResponseStr = httpParserGetHttpBodyLocation(pHttpRspCtx);
    resultLen = httpParserGetHttpBodyLength(pHttpRspCtx);
    uHttpStatusCode = httpParserGetHttpStatusCode(pHttpRspCtx);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) uHttpStatusCode);
    /* Check HTTP results */
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);
    CHK_STATUS(httpApiRspDescribeChannel((const CHAR*) pResponseStr, resultLen, pSignalingClient));

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (pNetworkContext != NULL) {
        disconnectFromServer(pNetworkContext);
        terminateNetworkContext(pNetworkContext);
        MEMFREE(pNetworkContext);
    }
    if (pHttpRspCtx != NULL) {
        retStatus = httpParserDetroy(pHttpRspCtx);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("destroying http parset failed. \n");
        }
    }
    SAFE_MEMFREE(pHttpBody);
    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    freeRequestInfo(&pRequestInfo);

    HTTP_API_EXIT();
    return retStatus;
}

STATUS httpApiGetChannelEndpoint(PSignalingClient pSignalingClient, UINT64 time)
{
    HTTP_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    PChannelInfo pChannelInfo = pSignalingClient->pChannelInfo;

    /* Variables for network connection */
    NetworkContext_t* pNetworkContext = NULL;
    SIZE_T uConnectionRetryCnt = 0;
    UINT32 uBytesToSend = 0, uBytesReceived = 0;

    /* Variables for HTTP request */
    PCHAR pUrl = NULL;
    PRequestInfo pRequestInfo = NULL;
    PCHAR pHttpBody = NULL;
    UINT32 httpBodyLen;
    PCHAR pHost = NULL;
    // rsp
    UINT32 uHttpStatusCode = 0;
    HttpResponseContext* pHttpRspCtx = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;

    CHK(NULL != (pHost = (PCHAR) MEMALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN)), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pUrl = (PCHAR) MEMALLOC(STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(API_GET_SIGNALING_CHANNEL_ENDPOINT) + 1)),
        STATUS_NOT_ENOUGH_MEMORY);

    // Create the API url
    STRCPY(pUrl, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(pUrl, API_GET_SIGNALING_CHANNEL_ENDPOINT);
    httpBodyLen = SIZEOF(BODY_TEMPLATE_GET_CHANNEL_ENDPOINT) + STRLEN(pSignalingClient->channelDescription.channelArn) +
        STRLEN(API_CALL_CHANNEL_PROTOCOL) + STRLEN(getStringFromChannelRoleType(pChannelInfo->channelRoleType)) + 1;
    CHK(NULL != (pHttpBody = (PCHAR) MEMALLOC(httpBodyLen)), STATUS_NOT_ENOUGH_MEMORY);

    /* generate HTTP request body */
    SNPRINTF(pHttpBody, httpBodyLen, BODY_TEMPLATE_GET_CHANNEL_ENDPOINT, pSignalingClient->channelDescription.channelArn, API_CALL_CHANNEL_PROTOCOL,
             getStringFromChannelRoleType(pChannelInfo->channelRoleType));
    // Create the request info with the body
    CHK_STATUS(createRequestInfo(pUrl, pHttpBody, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent, API_CALL_CONNECTION_TIMEOUT,
                                 API_CALL_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT,
                                 pSignalingClient->pAwsCredentials, &pRequestInfo));

    /* Initialize and generate HTTP request, then send it. */
    CHK(NULL != (pNetworkContext = (NetworkContext_t*) MEMALLOC(SIZEOF(NetworkContext_t))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(initNetworkContext(pNetworkContext) != STATUS_SUCCESS);

    httpPackSendBuf(pRequestInfo, HTTP_REQUEST_VERB_POST_STRING, pHost, MAX_CONTROL_PLANE_URI_CHAR_LEN, (PCHAR) pNetworkContext->pHttpSendBuffer,
                    MAX_HTTP_SEND_BUFFER_LEN, FALSE, NULL);

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

    CHK_STATUS(httpParserStart(&pHttpRspCtx, (CHAR*) pNetworkContext->pHttpRecvBuffer, (UINT32) uBytesReceived, NULL));
    pResponseStr = httpParserGetHttpBodyLocation(pHttpRspCtx);
    resultLen = httpParserGetHttpBodyLength(pHttpRspCtx);
    uHttpStatusCode = httpParserGetHttpStatusCode(pHttpRspCtx);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) uHttpStatusCode);
    /* Check HTTP results */
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);

    CHK_STATUS(httpApiRspGetChannelEndpoint((const CHAR*) pResponseStr, resultLen, pSignalingClient));
    /* We got a success response here. */

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (pNetworkContext != NULL) {
        disconnectFromServer(pNetworkContext);
        terminateNetworkContext(pNetworkContext);
        MEMFREE(pNetworkContext);
    }

    if (pHttpRspCtx != NULL) {
        retStatus = httpParserDetroy(pHttpRspCtx);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("destroying http parset failed. \n");
        }
    }
    SAFE_MEMFREE(pHttpBody);
    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    freeRequestInfo(&pRequestInfo);
    HTTP_API_EXIT();
    return retStatus;
}

STATUS httpApiGetIceConfig(PSignalingClient pSignalingClient, UINT64 time)
{
    HTTP_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    PChannelInfo pChannelInfo = pSignalingClient->pChannelInfo;

    /* Variables for network connection */
    NetworkContext_t* pNetworkContext = NULL;
    SIZE_T uConnectionRetryCnt = 0;
    UINT32 uBytesToSend = 0, uBytesReceived = 0;

    /* Variables for HTTP request */
    // http req.
    PCHAR pUrl = NULL;
    PRequestInfo pRequestInfo = NULL;
    PCHAR pHttpBody = NULL;
    UINT32 httpBodyLen;
    PCHAR pHost = NULL;
    // rsp
    UINT32 uHttpStatusCode = 0;
    HttpResponseContext* pHttpRspCtx = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;

    CHK(NULL != (pHost = (PCHAR) MEMALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN)), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pUrl = (PCHAR) MEMALLOC(STRLEN(pSignalingClient->channelEndpointHttps) + STRLEN(API_GET_ICE_CONFIG) + 1)), STATUS_NOT_ENOUGH_MEMORY);
    httpBodyLen = SIZEOF(BODY_TEMPLATE_GET_ICE_CONFIG) + STRLEN(pSignalingClient->channelDescription.channelArn) +
        STRLEN(pSignalingClient->clientInfo.signalingClientInfo.clientId) + 1;
    CHK(NULL != (pHttpBody = (PCHAR) MEMALLOC(httpBodyLen)), STATUS_NOT_ENOUGH_MEMORY);

    STRCPY(pUrl, pSignalingClient->channelEndpointHttps);
    STRCAT(pUrl, API_GET_ICE_CONFIG);
    /* generate HTTP request body */
    SNPRINTF(pHttpBody, httpBodyLen, BODY_TEMPLATE_GET_ICE_CONFIG, pSignalingClient->channelDescription.channelArn,
             pSignalingClient->clientInfo.signalingClientInfo.clientId);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(pUrl, pHttpBody, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent, API_CALL_CONNECTION_TIMEOUT,
                                 API_CALL_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT,
                                 pSignalingClient->pAwsCredentials, &pRequestInfo));

    /* Initialize and generate HTTP request, then send it. */
    CHK(NULL != (pNetworkContext = (NetworkContext_t*) MEMALLOC(SIZEOF(NetworkContext_t))), STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(initNetworkContext(pNetworkContext) != STATUS_SUCCESS);

    httpPackSendBuf(pRequestInfo, HTTP_REQUEST_VERB_POST_STRING, pHost, MAX_CONTROL_PLANE_URI_CHAR_LEN, (PCHAR) pNetworkContext->pHttpSendBuffer,
                    MAX_HTTP_SEND_BUFFER_LEN, FALSE, NULL);

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

    CHK_STATUS(httpParserStart(&pHttpRspCtx, (CHAR*) pNetworkContext->pHttpRecvBuffer, (UINT32) uBytesReceived, NULL));

    pResponseStr = httpParserGetHttpBodyLocation(pHttpRspCtx);
    resultLen = httpParserGetHttpBodyLength(pHttpRspCtx);
    uHttpStatusCode = httpParserGetHttpStatusCode(pHttpRspCtx);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) uHttpStatusCode);
    /* Check HTTP results */
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);

    CHK_STATUS(httpApiRspGetIceConfig((const CHAR*) pResponseStr, resultLen, pSignalingClient));

    if (retStatus != STATUS_SUCCESS) {
        DLOGD("parse failed.");
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (pNetworkContext != NULL) {
        disconnectFromServer(pNetworkContext);
        terminateNetworkContext(pNetworkContext);
        MEMFREE(pNetworkContext);
    }
    if (pHttpRspCtx != NULL) {
        retStatus = httpParserDetroy(pHttpRspCtx);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("destroying http parset failed. \n");
        }
    }
    SAFE_MEMFREE(pHttpBody);
    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    freeRequestInfo(&pRequestInfo);
    HTTP_API_EXIT();
    return retStatus;
}

STATUS httpApiDeleteChannl(PSignalingClient pSignalingClient, UINT64 time)
{
    HTTP_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    PChannelInfo pChannelInfo = pSignalingClient->pChannelInfo;
    NetworkContext_t* pNetworkContext = NULL;
    SIZE_T uConnectionRetryCnt = 0;
    UINT32 uBytesToSend = 0, uBytesReceived = 0;
    PCHAR pUrl = NULL;
    PRequestInfo pRequestInfo = NULL;
    PCHAR pHttpBody = NULL;
    UINT32 httpBodyLen;
    PCHAR pHost = NULL;
    SIZE_T result;
    UINT32 uHttpStatusCode = 0;
    HttpResponseContext* pHttpRspCtx = NULL;

    CHK(NULL != (pHost = (PCHAR) MEMALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN)), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pUrl = (PCHAR) MEMALLOC(STRLEN(pSignalingClient->pChannelInfo->pControlPlaneUrl) + STRLEN(API_DELETE_SIGNALING_CHANNEL) + 1)),
        STATUS_NOT_ENOUGH_MEMORY);
    // Check if we need to terminate the ongoing listener
    if (pSignalingClient->pWssContext != NULL) {
        wssTerminateConnection(pSignalingClient, SERVICE_CALL_RESULT_OK);
    }
    // Create the API url
    STRCPY(pUrl, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(pUrl, API_DELETE_SIGNALING_CHANNEL);
    httpBodyLen = SIZEOF(BODY_TEMPLATE_DELETE_CHANNEL) + STRLEN(pSignalingClient->channelDescription.channelArn) +
        STRLEN(pSignalingClient->channelDescription.updateVersion) + 1;
    CHK(NULL != (pHttpBody = (CHAR*) MEMALLOC(httpBodyLen)), STATUS_NOT_ENOUGH_MEMORY);

    /* generate HTTP request body */
    SNPRINTF(pHttpBody, httpBodyLen, BODY_TEMPLATE_DELETE_CHANNEL, pSignalingClient->channelDescription.channelArn,
             pSignalingClient->channelDescription.updateVersion);
    // Create the request info with the body
    CHK_STATUS(createRequestInfo(pUrl, pHttpBody, pSignalingClient->pChannelInfo->pRegion, (PCHAR) pSignalingClient->pChannelInfo->pCertPath, NULL,
                                 NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent, API_CALL_CONNECTION_TIMEOUT,
                                 API_CALL_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT,
                                 pSignalingClient->pAwsCredentials, &pRequestInfo));

    /* Initialize and generate HTTP request, then send it. */
    CHK(NULL != (pNetworkContext = (NetworkContext_t*) MEMALLOC(SIZEOF(NetworkContext_t))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(initNetworkContext(pNetworkContext));

    httpPackSendBuf(pRequestInfo, HTTP_REQUEST_VERB_POST_STRING, pHost, MAX_CONTROL_PLANE_URI_CHAR_LEN, (PCHAR) pNetworkContext->pHttpSendBuffer,
                    MAX_HTTP_SEND_BUFFER_LEN, FALSE, NULL);

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

    CHK_STATUS(httpParserStart(&pHttpRspCtx, (CHAR*) pNetworkContext->pHttpRecvBuffer, (UINT32) uBytesReceived, NULL));
    uHttpStatusCode = httpParserGetHttpStatusCode(pHttpRspCtx);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) uHttpStatusCode);
    /* Check HTTP results */
    result = ATOMIC_LOAD(&pSignalingClient->result);
    CHK((SERVICE_CALL_RESULT) result == SERVICE_CALL_RESULT_OK || (SERVICE_CALL_RESULT) result == SERVICE_CALL_RESOURCE_NOT_FOUND, retStatus);

    ATOMIC_STORE_BOOL(&pSignalingClient->deleted, TRUE);

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pNetworkContext != NULL) {
        disconnectFromServer(pNetworkContext);
        terminateNetworkContext(pNetworkContext);
        MEMFREE(pNetworkContext);
    }

    if (pHttpRspCtx != NULL) {
        retStatus = httpParserDetroy(pHttpRspCtx);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("destroying http parset failed. \n");
        }
    }
    SAFE_MEMFREE(pHttpBody);
    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    freeRequestInfo(&pRequestInfo);

    HTTP_API_EXIT();
    return retStatus;
}