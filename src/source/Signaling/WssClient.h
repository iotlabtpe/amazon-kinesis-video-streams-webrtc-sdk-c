#ifndef __KINESIS_VIDEO_WEBRTC_WSS_CLIENT_H__
#define __KINESIS_VIDEO_WEBRTC_WSS_CLIENT_H__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// wslay related lib.
//#include "wslay/wslay.h"

// SIGNALING_SERVICE_WSS_PING_PONG_INTERVAL_IN_SECONDS
#define WSS_CLIENT_RFC6455_UUID                "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WSS_CLIENT_RFC6455_UUID_LEN            STRLEN(WSS_CLIENT_RFC6455_UUID)
#define WSS_CLIENT_RANDOM_SEED_LEN             16
#define WSS_CLIENT_BASED64_RANDOM_SEED_LEN     24
#define WSS_CLIENT_SHA1_RANDOM_SEED_W_UUID_LEN 20
#define WSS_CLIENT_ACCEPT_KEY_LEN              28
#define WSS_CLIENT_POLLING_INTERVAL            100 // unit:ms.
#define WSS_CLIENT_PING_PONG_INTERVAL          10  // unit:sec.
#define WSS_CLIENT_PING_PONG_COUNTER           (WSS_CLIENT_PING_PONG_INTERVAL * 1000) / WSS_CLIENT_POLLING_INTERVAL
#define WSS_CLIENT_PING_MAX_ACC_NUM            1

typedef STATUS (*MessageHandlerFunc)(PSignalingClient, PCHAR, UINT32);
typedef STATUS (*CtrlMessageHandlerFunc)(PSignalingClient, UINT8, PCHAR, UINT32);

typedef struct {
    wslay_event_context_ptr event_ctx;            //!< the event context of wslay.
    struct wslay_event_callbacks event_callbacks; //!< the callback of event context.
    NetworkContext_t* pNetworkContext;
    UINT64 pingCounter;
    MUTEX clientLock;                          //!< the lock for the control of the whole wss client api.
    MUTEX listenerLock;                        //!< the lock for the listener thread.
    PVOID pUserData;                           //!< the arguments of the message handler. ref: PSignalingClient
    MessageHandlerFunc messageHandler;         //!< the handler of receive the non-ctrl messages.
    CtrlMessageHandlerFunc ctrlMessageHandler; //!< the handler of receive the ctrl messages.
} WssClientContext, *PWssClientContext;

STATUS wssClientGenerateRandomNumber(PUINT8 num, UINT32 len);
STATUS wssClientGenerateClientKey(PCHAR buf, UINT32 bufLen);
STATUS wssClientValidateAcceptKey(PCHAR clientKey, UINT32 clientKeyLen, PCHAR acceptKey, UINT32 acceptKeyLen);
VOID wssClientCreate(WssClientContext** ppWssClientCtx, NetworkContext_t* pNetworkContext, PVOID arg, MessageHandlerFunc pFunc,
                     CtrlMessageHandlerFunc pCtrlFunc);
PVOID wssClientStart(WssClientContext* pWssClientCtx);
STATUS wssClientSendText(WssClientContext* pCtx, UINT8* buf, UINT32 len);
STATUS wssClientSendBinary(WssClientContext* pCtx, UINT8* buf, UINT32 len);
STATUS wssClientSendPing(WssClientContext* pCtx);
VOID wssClientClose(WssClientContext* pWssClientCtx);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_WSS_CLIENT_H__ */