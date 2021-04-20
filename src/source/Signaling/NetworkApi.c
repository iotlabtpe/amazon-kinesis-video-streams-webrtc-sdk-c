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

#define LOG_CLASS "NetworkApi"
#include "../Include_i.h"

//#include <mbedtls/net.h>
//#include <mbedtls/ctr_drbg.h>
//#include <mbedtls/entropy.h>

//#include <sys/socket.h>

//#include "network_api.h"
//#include "mbedtls/debug.h"
/*-----------------------------------------------------------*/
#define mbedtls_fprintf fprintf
static VOID my_debug(PVOID ctx, INT32 level, const PCHAR file, INT32 line, const PCHAR str)
{
    ((VOID) level);

    mbedtls_fprintf((FILE*) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE*) ctx);
}

STATUS initNetworkContext(NetworkContext_t* pNetworkContext)
{
    STATUS retStatus = STATUS_SUCCESS;

    if (pNetworkContext == NULL) {
        retStatus = STATUS_INVALID_ARG;
    } else {
        MEMSET(pNetworkContext, 0, sizeof(NetworkContext_t));

        mbedtls_net_init(&(pNetworkContext->server_fd));

        mbedtls_ssl_init(&(pNetworkContext->ssl));

        mbedtls_ssl_config_init(&(pNetworkContext->conf));

        mbedtls_ctr_drbg_init(&(pNetworkContext->ctr_drbg));

        mbedtls_entropy_init(&(pNetworkContext->entropy));

        if (mbedtls_ctr_drbg_seed(&(pNetworkContext->ctr_drbg), mbedtls_entropy_func, &(pNetworkContext->entropy), NULL, 0) != 0) {
            retStatus = STATUS_INIT_NETWORK_FAILED;
        } else if ((pNetworkContext->pHttpSendBuffer = (UINT8*) MEMALLOC(MAX_HTTP_SEND_BUFFER_LEN)) == NULL) {
            DLOGE("OOM: pHttpSendBuffer");
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
        } else if ((pNetworkContext->pHttpRecvBuffer = (UINT8*) MEMALLOC(MAX_HTTP_RECV_BUFFER_LEN)) == NULL) {
            DLOGE("OOM: pHttpRecvBuffer");
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
        } else {
            pNetworkContext->uHttpSendBufferLen = MAX_HTTP_SEND_BUFFER_LEN;
            pNetworkContext->uHttpRecvBufferLen = MAX_HTTP_RECV_BUFFER_LEN;
        }

        if (retStatus != STATUS_SUCCESS) {
            terminateNetworkContext(pNetworkContext);
        }
        // mbedtls_debug_set_threshold( 0xf );
        // mbedtls_ssl_conf_dbg( &pNetworkContext->conf, my_debug, stdout );
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

VOID terminateNetworkContext(NetworkContext_t* pNetworkContext)
{
    if (pNetworkContext != NULL) {
        // DLOGD("Terminate network context");

        mbedtls_net_free(&(pNetworkContext->server_fd));
        mbedtls_ssl_free(&(pNetworkContext->ssl));
        mbedtls_ssl_config_free(&(pNetworkContext->conf));

        if (pNetworkContext->pRootCA != NULL) {
            mbedtls_x509_crt_free(pNetworkContext->pRootCA);
            MEMFREE(pNetworkContext->pRootCA);
            pNetworkContext->pRootCA = NULL;
        }

        if (pNetworkContext->pClientCert != NULL) {
            mbedtls_x509_crt_free(pNetworkContext->pClientCert);
            MEMFREE(pNetworkContext->pClientCert);
            pNetworkContext->pClientCert = NULL;
        }

        if (pNetworkContext->pPrivateKey != NULL) {
            mbedtls_pk_free(pNetworkContext->pPrivateKey);
            MEMFREE(pNetworkContext->pPrivateKey);
            pNetworkContext->pPrivateKey = NULL;
        }

        MEMFREE(pNetworkContext->pHttpSendBuffer);
        pNetworkContext->pHttpSendBuffer = NULL;
        pNetworkContext->uHttpSendBufferLen = 0;

        MEMFREE(pNetworkContext->pHttpRecvBuffer);
        pNetworkContext->pHttpRecvBuffer = NULL;
        pNetworkContext->uHttpRecvBufferLen = 0;
    }
}

/*-----------------------------------------------------------*/

static STATUS _connectToServer(NetworkContext_t* pNetworkContext, const PCHAR pServerHost, const PCHAR pServerPort, const PCHAR pRootCA,
                               const PCHAR pCertificate, const PCHAR pPrivateKey)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 ret = 0;
    BOOL bHasX509Certificate = FALSE;

    if (pNetworkContext == NULL) {
        DLOGE("Invalid Arg: network context");
        retStatus = STATUS_INVALID_ARG;
    } else if (pRootCA != NULL && pCertificate != NULL && pPrivateKey != NULL) {
        bHasX509Certificate = TRUE;

        if ((pNetworkContext->pRootCA = (mbedtls_x509_crt*) MEMALLOC(sizeof(mbedtls_x509_crt))) == NULL) {
            DLOGE("OOM: pRootCA");
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
        } else if ((pNetworkContext->pClientCert = (mbedtls_x509_crt*) MEMALLOC(sizeof(mbedtls_x509_crt))) == NULL) {
            DLOGE("OOM: pClientCert");
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
        } else if ((pNetworkContext->pPrivateKey = (mbedtls_pk_context*) MEMALLOC(sizeof(mbedtls_pk_context))) == NULL) {
            DLOGE("OOM: pPrivateKey");
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
        } else {
            mbedtls_x509_crt_init(pNetworkContext->pRootCA);
            mbedtls_x509_crt_init(pNetworkContext->pClientCert);
            mbedtls_pk_init(pNetworkContext->pPrivateKey);
        }
    }

    if (retStatus == STATUS_SUCCESS) {
        if ((ret = mbedtls_net_connect(&(pNetworkContext->server_fd), pServerHost, pServerPort, MBEDTLS_NET_PROTO_TCP)) != 0) {
            DLOGE("net connect err (%d)", ret);
            retStatus = STATUS_SOCKET_CONNECT_FAILED;
        }
    }

    if (retStatus == STATUS_SUCCESS) {
        mbedtls_ssl_set_bio(&(pNetworkContext->ssl), &(pNetworkContext->server_fd), mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

        if ((ret = mbedtls_ssl_config_defaults(&(pNetworkContext->conf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                               MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
            DLOGE("ssl config err (%d)", ret);
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
        } else {
            mbedtls_ssl_conf_rng(&(pNetworkContext->conf), mbedtls_ctr_drbg_random, &(pNetworkContext->ctr_drbg));

            if (bHasX509Certificate) {
                if ((ret = mbedtls_x509_crt_parse(pNetworkContext->pRootCA, (VOID*) pRootCA, strlen(pRootCA) + 1)) != 0) {
                    DLOGE("x509 Root CA parse err (%d)", ret);
                    retStatus = STATUS_NETWORK_SETUP_ERROR;
                } else if ((ret = mbedtls_x509_crt_parse(pNetworkContext->pClientCert, (VOID*) pCertificate, strlen(pCertificate) + 1)) != 0) {
                    DLOGE("x509 client cert parse err (%d)", ret);
                    retStatus = STATUS_NETWORK_SETUP_ERROR;
                } else if ((ret = mbedtls_pk_parse_key(pNetworkContext->pPrivateKey, (VOID*) pPrivateKey, strlen(pPrivateKey) + 1, NULL, 0)) != 0) {
                    DLOGE("x509 priv key parse err (%d)", ret);
                    retStatus = STATUS_NETWORK_SETUP_ERROR;
                } else {
                    mbedtls_ssl_conf_authmode(&(pNetworkContext->conf), MBEDTLS_SSL_VERIFY_REQUIRED);

                    mbedtls_ssl_conf_ca_chain(&(pNetworkContext->conf), pNetworkContext->pRootCA, NULL);

                    if ((ret = mbedtls_ssl_conf_own_cert(&(pNetworkContext->conf), pNetworkContext->pClientCert, pNetworkContext->pPrivateKey)) !=
                        0) {
                        DLOGE("ssl conf cert err (%d)", ret);
                        retStatus = STATUS_NETWORK_SETUP_ERROR;
                    }
                }
            } else {
                mbedtls_ssl_conf_authmode(&(pNetworkContext->conf), MBEDTLS_SSL_VERIFY_OPTIONAL);
            }
        }
    }

    if (retStatus == STATUS_SUCCESS) {
        if ((ret = mbedtls_ssl_setup(&(pNetworkContext->ssl), &(pNetworkContext->conf))) != 0) {
            DLOGE("ssl setup err (%d)", ret);
            retStatus = STATUS_NETWORK_SETUP_ERROR;
        } else if ((ret = mbedtls_ssl_handshake(&(pNetworkContext->ssl))) != 0) {
            DLOGE("ssl handshake err (%d)", ret);
            retStatus = STATUS_NETWORK_SSL_HANDSHAKE_ERROR;
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

STATUS connectToServer(NetworkContext_t* pNetworkContext, const PCHAR pServerHost, const PCHAR pServerPort)
{
    return _connectToServer(pNetworkContext, pServerHost, pServerPort, NULL, NULL, NULL);
}

/*-----------------------------------------------------------*/

STATUS connectToServerWithX509Cert(NetworkContext_t* pNetworkContext, const PCHAR pServerHost, const PCHAR pServerPort, const PCHAR pRootCA,
                                   const PCHAR pCertificate, const PCHAR pPrivateKey)
{
    return _connectToServer(pNetworkContext, pServerHost, pServerPort, pRootCA, pCertificate, pPrivateKey);
}

/*-----------------------------------------------------------*/

STATUS disconnectFromServer(NetworkContext_t* pNetworkContext)
{
    mbedtls_ssl_close_notify(&(pNetworkContext->ssl));

    return 0;
}

/*-----------------------------------------------------------*/

INT32 networkSend(NetworkContext_t* pNetworkContext, const PVOID pBuffer, SIZE_T uBytesToSend)
{
    INT32 retStatus = 0;
    PUINT8 pIndex = (PUINT8) pBuffer;
    INT32 uBytesRemaining = (INT32) uBytesToSend; // It should be safe because we won't send data larger than 2^31-1 bytes
    INT32 n = 0;

    if (pNetworkContext == NULL || pBuffer == NULL) {
        retStatus = -1;
    } else {
        while (uBytesRemaining > 0UL) {
            // DLOGD("try to send %d bytes", uBytesRemaining);
            n = mbedtls_ssl_write(&(pNetworkContext->ssl), pIndex, uBytesRemaining);

            if (n < 0 || n > uBytesRemaining) {
                DLOGW("ssl send err (%d)", n);
                retStatus = -1;
                break;
            } else {
                // DLOGD("sent %d bytes", n);
                uBytesRemaining -= n;
                pIndex += n;
            }
        }
    }

    if (retStatus == STATUS_SUCCESS) {
        return uBytesToSend;
    } else {
        return retStatus;
    }
}

/*-----------------------------------------------------------*/

INT32 isRecvDataAvailable(NetworkContext_t* pNetworkContext)
{
    INT32 retStatus = STATUS_SUCCESS;
    struct timeval tv = {0};
    fd_set read_fds = {0};
    INT32 fd = 0;

    if (pNetworkContext == NULL) {
        retStatus = STATUS_INVALID_ARG;
    } else {
        fd = pNetworkContext->server_fd.fd;
        if (fd < 0) {
            retStatus = STATUS_NETWORK_SOCKET_NOT_CONNECTED;
        } else {
            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);

            tv.tv_sec = 0;
            tv.tv_usec = 0;

            if (select(fd + 1, &read_fds, NULL, NULL, &tv) >= 0) {
                if (FD_ISSET(fd, &read_fds)) {
                    /* We have available receiving data. */
                } else {
                    retStatus = STATUS_NETWORK_NO_AVAILABLE_RECV_DATA;
                }
            } else {
                retStatus = STATUS_NETWORK_SELECT_ERROR;
            }
        }
    }

    return retStatus;
}

/*-----------------------------------------------------------*/

INT32 networkRecv(NetworkContext_t* pNetworkContext, PVOID pBuffer, SIZE_T uBytesToRecv)
{
    INT32 retStatus = 0;
    INT32 n = 0;

    if (pNetworkContext == NULL || pBuffer == NULL) {
        DLOGE("Invalid Arg in networkRecv");
        retStatus = -1;
    } else {
        n = mbedtls_ssl_read(&(pNetworkContext->ssl), pBuffer, uBytesToRecv);

        if (n < 0 || n > uBytesToRecv) {
            DLOGW("ssl read err (%d)", n);
            retStatus = n;
        } else {
            DLOGD("ssl read %d bytes", n);
        }
    }

    if (retStatus == STATUS_SUCCESS) {
        return n;
    } else {
        return retStatus;
    }
}