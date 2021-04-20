#ifndef __KINESIS_VIDEO_WEBRTC_HTTP_API_H_
#define __KINESIS_VIDEO_WEBRTC_HTTP_API_H_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

STATUS httpApiCreateChannl(PSignalingClient pSignalingClient, UINT64 time);
STATUS httpApiDescribeChannel(PSignalingClient pSignalingClient, UINT64 time);
STATUS httpApiGetChannelEndpoint(PSignalingClient pSignalingClient, UINT64 time);
STATUS httpApiGetIceConfig(PSignalingClient pSignalingClient, UINT64 time);
STATUS httpApiDeleteChannl(PSignalingClient pSignalingClient, UINT64 time);
STATUS httpApiRspCreateChannel(const CHAR* pResponseStr, UINT32 resultLen, PSignalingClient pSignalingClient);
STATUS httpApiRspDeleteChannel(const CHAR* pResponseStr, UINT32 resultLen, PSignalingClient pSignalingClient);
STATUS httpApiRspDescribeChannel(const CHAR* pResponseStr, UINT32 resultLen, PSignalingClient pSignalingClient);
STATUS httpApiRspGetChannelEndpoint(const CHAR* pResponseStr, UINT32 resultLen, PSignalingClient pSignalingClient);
STATUS httpApiRspGetIceConfig(const CHAR* pResponseStr, UINT32 resultLen, PSignalingClient pSignalingClient);
#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_HTTP_API_H_ */