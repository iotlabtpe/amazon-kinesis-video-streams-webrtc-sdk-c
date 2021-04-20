#define LOG_CLASS "WssClient"
#include "../Include_i.h"

#define WSS_CLIENT_ENTER()
#define WSS_CLIENT_EXIT()

#define CLIENT_LOCK(pWssCtx)     MUTEX_LOCK(pWssCtx->clientLock)
#define CLIENT_UNLOCK(pWssCtx)   MUTEX_UNLOCK(pWssCtx->clientLock)
#define LISTENER_LOCK(pWssCtx)   MUTEX_LOCK(pWssCtx->listenerLock)
#define LISTENER_UNLOCK(pWssCtx) MUTEX_UNLOCK(pWssCtx->listenerLock)

#define WSLAY_SUCCESS 0
/*-----------------------------------------------------------*/
STATUS wssClientGenerateRandomNumber(PUINT8 num, UINT32 len)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    CHK(mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0) == 0, STATUS_WSS_GENERATE_RANDOM_NUM_ERROR);
    CHK(mbedtls_ctr_drbg_random(&ctr_drbg, (UINT8*) num, len) == 0, STATUS_WSS_GENERATE_RANDOM_NUM_ERROR);

CleanUp:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    WSS_CLIENT_EXIT();
    return 0;
}

STATUS wssClientGenerateClientKey(PCHAR buf, UINT32 bufLen)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 olen = 0;
    UINT8 randomNum[WSS_CLIENT_RANDOM_SEED_LEN + 1];
    MEMSET(randomNum, 0, WSS_CLIENT_RANDOM_SEED_LEN + 1);

    CHK_STATUS(wssClientGenerateRandomNumber(randomNum, WSS_CLIENT_RANDOM_SEED_LEN));
    CHK(mbedtls_base64_encode((UINT8*) buf, bufLen, (VOID*) &olen, (UINT8*) randomNum, WSS_CLIENT_RANDOM_SEED_LEN) == 0,
        STATUS_WSS_GENERATE_CLIENT_KEY_ERROR);
    CHK(olen == WSS_CLIENT_BASED64_RANDOM_SEED_LEN, STATUS_WSS_GENERATE_CLIENT_KEY_ERROR);

CleanUp:
    WSS_CLIENT_EXIT();
    return retStatus;
}

STATUS wssClientGenerateAcceptKey(PCHAR clientKey, UINT32 clientKeyLen, PCHAR acceptKey, UINT32 acceptKeyLen)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 bufLen = WSS_CLIENT_BASED64_RANDOM_SEED_LEN + WSS_CLIENT_RFC6455_UUID_LEN + 1;
    UINT8 buf[bufLen];
    UINT8 obuf[WSS_CLIENT_SHA1_RANDOM_SEED_W_UUID_LEN + 1];
    UINT32 olen = 0;
    MEMSET(buf, 0, bufLen);
    MEMSET(obuf, 0, WSS_CLIENT_SHA1_RANDOM_SEED_W_UUID_LEN + 1);
    MEMCPY(buf, clientKey, STRLEN(clientKey));
    MEMCPY(buf + STRLEN(clientKey), WSS_CLIENT_RFC6455_UUID, STRLEN(WSS_CLIENT_RFC6455_UUID));
    mbedtls_sha1(buf, STRLEN((PCHAR) buf), obuf);

    CHK(mbedtls_base64_encode((UINT8*) acceptKey, acceptKeyLen, (VOID*) &olen, (UINT8*) obuf, 20) == 0, STATUS_WSS_GENERATE_ACCEPT_KEY_ERROR);
    CHK(olen == WSS_CLIENT_ACCEPT_KEY_LEN, STATUS_WSS_GENERATE_ACCEPT_KEY_ERROR);

CleanUp:
    WSS_CLIENT_EXIT();
    return retStatus;
}

STATUS wssClientValidateAcceptKey(PCHAR clientKey, UINT32 clientKeyLen, PCHAR acceptKey, UINT32 acceptKeyLen)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    UINT8 tmpKey[WSS_CLIENT_ACCEPT_KEY_LEN + 1];
    MEMSET(tmpKey, 0, WSS_CLIENT_ACCEPT_KEY_LEN + 1);

    CHK_STATUS(wssClientGenerateAcceptKey(clientKey, clientKeyLen, (PCHAR) tmpKey, WSS_CLIENT_ACCEPT_KEY_LEN + 1));
    CHK(MEMCMP(tmpKey, acceptKey, WSS_CLIENT_ACCEPT_KEY_LEN) == 0, STATUS_WSS_VALIDATE_ACCEPT_KEY_ERROR);

CleanUp:
    WSS_CLIENT_EXIT();
    return 0;
}

INT32 wssClientSocketSend(WssClientContext* pWssCtx, const UINT8* data, SIZE_T len, INT32 flags)
{
    return networkSend(pWssCtx->pNetworkContext, data, len);
}

INT32 wssClientSocketRead(WssClientContext* pWssCtx, UINT8* data, SIZE_T len, INT32 flags)
{
    return networkRecv(pWssCtx->pNetworkContext, data, len);
}

SSIZE_T wslay_send_callback(wslay_event_context_ptr ctx, const UINT8* data, SIZE_T len, INT32 flags, VOID* user_data)
{
    WssClientContext* pWssCtx = (WssClientContext*) user_data;
    SSIZE_T r = wssClientSocketSend(pWssCtx, data, len, flags);
    if (r == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
        } else {
            wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        }
    }
    return r;
}

SSIZE_T wslay_recv_callback(wslay_event_context_ptr ctx, UINT8* data, SIZE_T len, INT32 flags, VOID* user_data)
{
    WssClientContext* pWssCtx = (WssClientContext*) user_data;
    SSIZE_T r = wssClientSocketRead(pWssCtx, data, len, flags);
    if (r == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
        } else {
            wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        }
    } else if (r == 0) {
        wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        r = -1;
    }
    return r;
}

INT32 wslay_genmask_callback(wslay_event_context_ptr ctx, UINT8* buf, SIZE_T len, VOID* user_data)
{
    WssClientContext* pWssCtx = (WssClientContext*) user_data;
    wssClientGenerateRandomNumber(buf, len);
    return 0;
}

VOID wslay_msg_recv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg* arg, VOID* user_data)
{
    WssClientContext* pWssCtx = (WssClientContext*) user_data;
    if (!wslay_is_ctrl_frame(arg->opcode)) {
        pWssCtx->messageHandler(pWssCtx->pUserData, arg->msg, arg->msg_length);
    } else {
        pWssCtx->ctrlMessageHandler(pWssCtx->pUserData, arg->opcode, arg->msg, arg->msg_length);
        if (arg->opcode == WSLAY_PONG) {
            pWssCtx->pingCounter = 0;
        }
    }
}

INT32 wssClientWantRead(WssClientContext* pWssCtx)
{
    WSS_CLIENT_ENTER();
    INT32 retStatus = TRUE;
    CLIENT_LOCK(pWssCtx);
    retStatus = wslay_event_want_read(pWssCtx->event_ctx);
    CLIENT_UNLOCK(pWssCtx);
    WSS_CLIENT_EXIT();
    return retStatus;
}

STATUS wssClientOnReadEvent(WssClientContext* pWssCtx)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    CLIENT_LOCK(pWssCtx);
    if (wslay_event_get_read_enabled(pWssCtx->event_ctx) == 1) {
        retStatus = wslay_event_recv(pWssCtx->event_ctx) == 0 ? STATUS_SUCCESS : STATUS_WSS_CLIENT_RECV_FAILED;
    }
    CLIENT_UNLOCK(pWssCtx);
    WSS_CLIENT_EXIT();
    return retStatus;
}

static STATUS wssClientSend(WssClientContext* pWssCtx, struct wslay_event_msg* arg)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    CLIENT_LOCK(pWssCtx);
    // #YC_TBD, wslay will memcpy this message buffer, so we can release the message buffer.
    // But this is a tradeoff. We can evaluate this design later.
    if (wslay_event_get_write_enabled(pWssCtx->event_ctx) == 1) {
        // send the message out immediately.
        CHK(wslay_event_queue_msg(pWssCtx->event_ctx, arg) == WSLAY_SUCCESS, STATUS_WSS_CLIENT_SEND_FAILED);
        CHK(wslay_event_send(pWssCtx->event_ctx) == WSLAY_SUCCESS, STATUS_WSS_CLIENT_SEND_FAILED);
    }

CleanUp:
    CLIENT_UNLOCK(pWssCtx);
    WSS_CLIENT_EXIT();
    return retStatus;
}

STATUS wssClientSendText(WssClientContext* pWssCtx, UINT8* buf, UINT32 len)
{
    struct wslay_event_msg arg;
    arg.opcode = WSLAY_TEXT_FRAME;
    arg.msg = buf;
    arg.msg_length = len;
    return wssClientSend(pWssCtx, &arg);
}

STATUS wssClientSendBinary(WssClientContext* pWssCtx, UINT8* buf, UINT32 len)
{
    struct wslay_event_msg arg;
    arg.opcode = WSLAY_BINARY_FRAME;
    arg.msg = buf;
    arg.msg_length = len;
    return wssClientSend(pWssCtx, &arg);
}

STATUS wssClientSendPing(WssClientContext* pWssCtx)
{
    struct wslay_event_msg arg;
    MEMSET(&arg, 0, sizeof(arg));
    arg.opcode = WSLAY_PING;
    arg.msg_length = 0;
    DLOGD("ping ==>");
    return wssClientSend(pWssCtx, &arg);
}

VOID wssClientCreate(WssClientContext** ppWssClientCtx, NetworkContext_t* pNetworkContext, PVOID arg, MessageHandlerFunc pFunc,
                     CtrlMessageHandlerFunc pCtrlFunc)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    WssClientContext* pWssCtx = NULL;
    struct wslay_event_callbacks callbacks = {
        wslay_recv_callback,    /* wslay_event_recv_callback */
        wslay_send_callback,    /* wslay_event_send_callback */
        wslay_genmask_callback, /* wslay_event_genmask_callback */
        NULL,                   /* wslay_event_on_frame_recv_start_callback */
        NULL,                   /* wslay_event_on_frame_recv_chunk_callback */
        NULL,                   /* wslay_event_on_frame_recv_end_callback */
        wslay_msg_recv_callback /* wslay_event_on_msg_recv_callback */
    };

    *ppWssClientCtx = NULL;
    CHK(NULL != (pWssCtx = (WssClientContext*) MEMCALLOC(1, SIZEOF(WssClientContext))), STATUS_NOT_ENOUGH_MEMORY);

    pWssCtx->event_callbacks = callbacks;
    pWssCtx->pNetworkContext = pNetworkContext;
    pWssCtx->pUserData = arg;
    pWssCtx->messageHandler = pFunc;
    pWssCtx->ctrlMessageHandler = pCtrlFunc;

    // the initialization of the mutex
    pWssCtx->clientLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pWssCtx->clientLock), STATUS_INVALID_OPERATION);
    pWssCtx->listenerLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pWssCtx->listenerLock), STATUS_INVALID_OPERATION);

    pWssCtx->pingCounter = 0;

    wslay_event_context_client_init(&pWssCtx->event_ctx, &pWssCtx->event_callbacks, pWssCtx);
    *ppWssClientCtx = pWssCtx;
CleanUp:
    WSS_CLIENT_EXIT();
    return;
}

PVOID wssClientStart(WssClientContext* pWssCtx)
{
    WSS_CLIENT_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = NULL;
    // for the inteface of socket.
    INT32 nfds = 0;
    INT32 retval;
    fd_set rfds;
    struct timeval tv;
    // for ping-pong.
    UINT32 counter = 0;

    // Mark as started
    pSignalingClient = (PSignalingClient) pWssCtx->pUserData;

    LISTENER_LOCK(pWssCtx);

    wslay_event_config_set_callbacks(pWssCtx->event_ctx, &pWssCtx->event_callbacks);
    mbedtls_ssl_conf_read_timeout(&(pWssCtx->pNetworkContext->conf), WSS_CLIENT_POLLING_INTERVAL);

    nfds = pWssCtx->pNetworkContext->server_fd.fd;
    FD_ZERO(&rfds);

    // check the wss client want to read or write or not.
    while (wssClientWantRead(pWssCtx)) {
        // need to setup the timeout of epoll in order to let the wss cleint thread to write the buffer out.
        FD_SET(nfds, &rfds);
        // #YC_TBD, this may need to be modified.
        tv.tv_sec = 0;
        tv.tv_usec = WSS_CLIENT_POLLING_INTERVAL * 1000;
        retval = select(nfds + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            DLOGE("select() failed with errno %s", getErrorString(getErrorCode()));
            continue;
        }

        if (FD_ISSET(nfds, &rfds)) {
            if (wssClientOnReadEvent(pWssCtx)) {
                DLOGE("on read event failed");
            }
        }
        // for ping-pong
        if (pWssCtx->pingCounter > WSS_CLIENT_PING_MAX_ACC_NUM) {
            pWssCtx->ctrlMessageHandler(pWssCtx->pUserData, WSLAY_CONNECTION_CLOSE, "connection lost", STRLEN("connection lost"));
        }

        counter++;
        if (counter == WSS_CLIENT_PING_PONG_COUNTER) {
            CHK_STATUS(wssClientSendPing(pWssCtx));
            pWssCtx->pingCounter++;
            counter = 0;
        }
    }

CleanUp:

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    LISTENER_UNLOCK(pWssCtx);
    WSS_CLIENT_EXIT();
    return (PVOID)(ULONG_PTR) retStatus;
}

VOID wssClientClose(WssClientContext* pWssCtx)
{
    INT32 retStatus = 0;

    if (IS_VALID_MUTEX_VALUE(pWssCtx->clientLock)) {
        CLIENT_LOCK(pWssCtx);
        wslay_event_shutdown_read(pWssCtx->event_ctx);
        wslay_event_shutdown_write(pWssCtx->event_ctx);
        wslay_event_context_free(pWssCtx->event_ctx);
        CLIENT_UNLOCK(pWssCtx);
    }

    if (IS_VALID_MUTEX_VALUE(pWssCtx->listenerLock)) {
        LISTENER_LOCK(pWssCtx);
        MUTEX_FREE(pWssCtx->listenerLock);
    }
    if (IS_VALID_MUTEX_VALUE(pWssCtx->clientLock)) {
        MUTEX_FREE(pWssCtx->clientLock);
    }

    if (pWssCtx->pNetworkContext != NULL) {
        disconnectFromServer(pWssCtx->pNetworkContext);
        terminateNetworkContext(pWssCtx->pNetworkContext);
        MEMFREE(pWssCtx->pNetworkContext);
    }

    MEMFREE(pWssCtx);
    return;
}