/*
 * @file CRtmpStream.h
 * @author Akagi201
 * @date 2015/02/10
 */

#ifndef CRTMP_STREAM_H_
#define CRTMP_STREAM_H_ (1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "librtmp/rtmp.h"
#include "librtmp/amf.h"
#include "librtmp/log.h"

#define DEFAULT_PORT (7)
#define SEND_BUF_SIZE (65525)
#define RECV_BUF_SIZE (256)

class CRtmpStream {
public:
    CRtmpStream(void);

    ~CRtmpStream(void);

    bool Connect(const char *url);

    void Close(void);

    int GetFlvHeader(unsigned char *buf);

    int GetFlvFrame(unsigned char *buf);

    int SendFlvHeader(unsigned char *buf, int size);

    int SendFlvFrame(unsigned char *buf, int size);

    bool SendFlvFile(const char *filename);

private:
    RTMP *m_pRtmp;
    FILE *m_pFid;
    int m_nDstSocket;
};

#endif // CRTMP_STREAM_H_