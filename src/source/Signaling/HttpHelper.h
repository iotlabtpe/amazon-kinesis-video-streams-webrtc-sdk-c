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

#ifndef __KINESIS_VIDEO_WEBRTC_HTTP_HELPER_H__
#define __KINESIS_VIDEO_WEBRTC_HTTP_HELPER_H__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PCHAR field;
    UINT32 fieldLen;
    PCHAR value;
    UINT32 valueLen;
    struct list_head list;
} HttpField, *PHttpField;

typedef struct {
    UINT32 httpStatusCode;
    UINT32 httpBodyLen;
    PCHAR phttpBodyLoc;
    HttpField curField;
    struct list_head* requiredHeader;
} HttpResponseContext, *PHttpResponseContext;

INT32 httpParserAddRequiredHeader(struct list_head* head, PCHAR field, UINT32 fieldLen, PCHAR value, UINT32 valudLen);
PHttpField httpParserGetValueByField(struct list_head* head, PCHAR field, UINT32 fieldLen);
UINT32 httpParserGetHttpStatusCode(PHttpResponseContext pHttpRspCtx);
PCHAR httpParserGetHttpBodyLocation(PHttpResponseContext pHttpRspCtx);
UINT32 httpParserGetHttpBodyLength(PHttpResponseContext pHttpRspCtx);
STATUS httpParserStart(PHttpResponseContext* ppHttpRspCtx, PCHAR pBuf, UINT32 uLen, struct list_head* requiredHeader);
STATUS httpParserDetroy(PHttpResponseContext pHttpRspCtx);
STATUS httpPackSendBuf(PRequestInfo pRequestInfo, PCHAR pVerb, PCHAR pHost, UINT32 hostLen, PCHAR outputBuf, UINT32 bufLen, BOOL bWss,
                       PCHAR clientKey);
#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_HTTP_HELPER_H__ */