/*
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef __KINESIS_VIDEO_WEBRTC_NETWORK_API_H__
#define __KINESIS_VIDEO_WEBRTC_NETWORK_API_H__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//#include <mbedtls/net.h>
//#include <mbedtls/ctr_drbg.h>
//#include <mbedtls/entropy.h>

/* The size of HTTP send buffer */
#define MAX_HTTP_SEND_BUFFER_LEN (2048)

/* The size of HTTP receive buffer */
#define MAX_HTTP_RECV_BUFFER_LEN (2048)

struct tls_context {
    // net related.
    mbedtls_net_context socket;
    // tls library related.
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_ssl_config sslConfig;
    mbedtls_ssl_context sslCtx;
    mbedtls_x509_crt cacert;
} tls_context_t;

struct NetworkContext {
    /* Basic ssl connection parameters */
    mbedtls_net_context server_fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    /* Variables for IoT credential provider. It's optional feature so we declare them as pointers. */
    mbedtls_x509_crt* pRootCA;
    mbedtls_x509_crt* pClientCert;
    mbedtls_pk_context* pPrivateKey;

    /* HTTP send and receive buffer */
    uint8_t* pHttpSendBuffer;
    size_t uHttpSendBufferLen;
    uint8_t* pHttpRecvBuffer;
    size_t uHttpRecvBufferLen;
};
typedef struct NetworkContext NetworkContext_t;

/**
 * @brief Initialize network context
 *
 * Initialize ssl variables and http buffers.
 *
 * @param pNetworkContext Network context
 *
 * @return KVS error code
 */
STATUS initNetworkContext(NetworkContext_t* pNetworkContext);

/**
 * @brief Terminate network context
 *
 * Free and terminate ssl variables, X509 certificates, and http buffers.
 *
 * @param pNetworkContext Network context
 */
VOID terminateNetworkContext(NetworkContext_t* pNetworkContext);

/**
 * @breif Connect to server with or without X509 certificates
 *
 * It's a more general API to connect to server. If X509 variables are not NULL, then it connect to server with X509
 * certificates.  It X509 is NULL, then it connect to server with optional verification.
 *
 * @param pNetworkContext Network context
 * @param pServerHost Server host
 * @param pServerPort Server port
 * @param pRootCA ROOT CA of X509 certificates
 * @param pCertificate Client certificate of X509 certificates
 * @param pPrivateKey Client private key of X509 certificates
 *
 * @return KVS error code
 */
STATUS connectToServerWithX509Cert(NetworkContext_t* pNetworkContext, const PCHAR pServerHost, const PCHAR pServerPort, const PCHAR pRootCA,
                                   const PCHAR pCertificate, const PCHAR pPrivateKey);

/**
 * @brief Connect to server without X509 certificates
 *
 * @param pNetworkContext Network context
 * @param pServerHost Server host
 * @param pServerPort Server port
 *
 * @return KVS error code
 */
STATUS connectToServer(NetworkContext_t* pNetworkContext, const PCHAR pServerHost, const PCHAR pServerPort);

/**
 * @brief Disconnect from Server
 *
 * @param pNetworkContext Network context
 *
 * @return KVS error code
 */
STATUS disconnectFromServer(NetworkContext_t* pNetworkContext);

/**
 * @brief Send data
 *
 * This API is compatible to TransportSend_t and can be an sending adapter of coreHTTP.
 *
 * @param pNetworkContext Network context
 * @param pBuffer Data buffer
 * @param uBytesToSend Data size
 *
 * @return The number of bytes sent or a negative KVS error code.
 */
INT32 networkSend(NetworkContext_t* pNetworkContext, const PVOID pBuffer, SIZE_T uBytesToSend);

/**
 * @brief Non-blocking API to check if any incoming data avaiable.
 *
 * @param pNetworkContext Network context
 *
 * @return KVS error code
 */
int32_t isRecvDataAvailable(NetworkContext_t* pNetworkContext);

/**
 * @brief Receive data
 *
 * This API is compatible to TransportRecv_t and can be an receiving adapter of coreHTTP.
 *
 * @param pNetworkContext Network context
 * @param pBuffer Data buffer
 * @param uBytesToRecv Data size
 *
 * @return The number of bytes received or a negative KVS error code.
 */
INT32 networkRecv(NetworkContext_t* pNetworkContext, PVOID pBuffer, SIZE_T uBytesToRecv);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_NETWORK_API_H__ */