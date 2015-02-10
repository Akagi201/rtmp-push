/*
 * @file CRtmpStream.cpp
 * @author Akagi201
 * @date 2015/02/10
 */

#include <unistd.h>

#include "CRtmpStream.h"
#include "bytestream.h"

char *put_byte(char *output, uint8_t nVal) {
    output[0] = nVal;

    return output + 1;
}

char *put_be16(char *output, uint16_t nVal) {
    output[1] = nVal & 0xff;
    output[0] = nVal >> 8;

    return output + 2;
}

char *put_be24(char *output, uint32_t nVal) {
    output[2] = nVal & 0xff;
    output[1] = nVal >> 8;
    output[0] = nVal >> 16;

    return output + 3;
}

char *put_be32(char *output, uint32_t nVal) {
    output[3] = nVal & 0xff;
    output[2] = nVal >> 8;
    output[1] = nVal >> 16;
    output[0] = nVal >> 24;

    return output + 4;
}

char *put_be64(char *output, uint64_t nVal) {
    output = put_be32(output, nVal >> 32);
    output = put_be32(output, nVal);

    return output;
}

char *put_amf_string(char *c, const char *str) {
    uint16_t len = strlen(str);
    c = put_be16(c, len);
    memcpy(c, str, len);

    return c + len;
}

CRtmpStream::CRtmpStream(void) : m_pRtmp(NULL) {
    m_pRtmp = RTMP_Alloc();
    RTMP_Init(m_pRtmp);

    m_pFid = NULL;
    m_nDstSocket = -1;

    return;
}

CRtmpStream::~CRtmpStream(void) {
    return;
}

bool CRtmpStream::Connect(const char *url) {
    if (RTMP_SetupURL(m_pRtmp, (char *) url) < 0) {
        return FALSE;
    }

    RTMP_EnableWrite(m_pRtmp);

    if (RTMP_Connect(m_pRtmp, NULL) < 0) {
        return FALSE;
    }

    m_nDstSocket = m_pRtmp->m_sb.sb_socket;

    if (RTMP_ConnectStream(m_pRtmp, 0) < 0) {
        return FALSE;
    }

    return TRUE;
}

void CRtmpStream::Close(void) {
    if (m_pRtmp) {
        RTMP_Close(m_pRtmp);
        RTMP_Free(m_pRtmp);
        m_pRtmp = NULL;
    }

    if (m_pFid) {
        fclose(m_pFid);
        m_pFid = NULL;
    }

    return;
}

/*
 * @brief 获取flv文件头数据, 包括metadata数据和其后的previous tag size, 返回文件头字节大学
 */
int CRtmpStream::GetFlvHeader(unsigned char *buf) {
    // 读取flv文件头9 + 4
    fread(buf, 13, 1, m_pFid);

    // 读取tag header
    fread(buf + 13, 11, 1, m_pFid);

    unsigned char type = buf[13];
    unsigned int size = (unsigned char) buf[14] * 256 * 256 + (unsigned char) buf[15] * 256 + (unsigned char) buf[16];

    // 读取metadata数据, 包括其后的previous tag size
    if (0x12 == type) {
        fread(buf + 24, size + 4, 1, m_pFid);
        return size + 24 + 4;
    } else {
        printf("The first tag is not metadata tag!\n");
        return -1;
    }
}

/*
 * @brief 读取一帧flv数据到buf中, 包括其后的previous tag size
 */
int CRtmpStream::GetFlvFrame(unsigned char *buf) {
    unsigned int framesize = 0;
    unsigned int bodysize = 0;
    static int tagIndex = 0;

    // 读取tag header
    fread(buf, 11, 1, m_pFid);
    framesize += 11;

    // 获取tag的类型
    unsigned char type = buf[0];
    if ((0x08 == type) || (0x09 == type)) {
        ++tagIndex;
    } else {
        printf("Error while reading flv frame!\n");
        return -1;
    }

    // 获取bodysize
    bodysize = (unsigned char) buf[1] * 256 * 256 + (unsigned char) buf[2] * 256 + (unsigned char) buf[3];
    fread(buf + 11, bodysize, 1, m_pFid);
    framesize += bodysize;
    printf("\n%s Tag%d Size: %d\n", (type == 0x08) ? "Audio" : ((type == 0x09) ? "Video" : "Metadata"), tagIndex, framesize);

    // 将4个字节的previous size读取到buf中
    fread(buf + framesize, 4, 1, m_pFid);
    framesize += 4;

    return framesize;
}

/*
 * @brief 发送flv文件头
 */
int CRtmpStream::SendFlvHeader(unsigned char *buf, int size) {
    uint32_t timestamp;
    int ret = 0;
    int pktSize, pktType;
    unsigned char *pHeader = buf + 13;

    // 获取metadata tag的头信息(11个字节)
    pktType = bytestream_get_byte((const unsigned char **) &pHeader);
    pktSize = bytestream_get_be24((const unsigned char **) &pHeader);
    timestamp = bytestream_get_be24((const unsigned char **) &pHeader);
    timestamp |= bytestream_get_byte((const unsigned char **) &pHeader) << 24;
    bytestream_get_be24((const unsigned char **) &pHeader);

    // 构造RTMP包
    RTMPPacket packet;
    RTMPPacket_Reset(&packet);
    ret = RTMPPacket_Alloc(&packet, size + 16);
    if (!ret) {
        return -1;
    }

    packet.m_packetType = pktType;
    packet.m_nBodySize = pktSize + 16;
    packet.m_nTimeStamp = timestamp;
    packet.m_nChannel = 4;
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_nInfoField2 = m_pRtmp->m_stream_id;

    uint8_t *body = (uint8_t *) packet.m_body;
    put_amf_string((char *) body, "@setDataFrame");
    memcpy(body, buf + 24, pktSize);

    ret = RTMP_SendPacket(m_pRtmp, &packet, 0);

    return ret;
}

/*
 * @brief 发送flv一帧
 */
int CRtmpStream::SendFlvFrame(unsigned char *buf, int size) {
    uint32_t timestamp;
    int ret = 0;
    int pktSize, pktType;
    unsigned char *pHeader = buf;

    // 获取tag header信息(11个字节)
    pktType = bytestream_get_byte((const unsigned char **) &pHeader);
    pktSize = bytestream_get_be24((const unsigned char **) &pHeader);
    timestamp = bytestream_get_be24((const unsigned char **) &pHeader);
    timestamp |= bytestream_get_byte((const unsigned char **) &pHeader) << 24;
    bytestream_get_be24((const unsigned char **) &pHeader);

    // 构造RTMP包
    RTMPPacket packet;
    RTMPPacket_Reset(&packet);
    ret = RTMPPacket_Alloc(&packet, size);
    if (!ret) {
        return -1;
    }

    packet.m_packetType = pktType;
    packet.m_nBodySize = pktSize;
    packet.m_nTimeStamp = timestamp;
    packet.m_nChannel = 4;
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_nInfoField2 = m_pRtmp->m_stream_id;

    memcpy(packet.m_body, buf + 11, pktSize);

    ret = RTMP_SendPacket(m_pRtmp, &packet, 0);

    return ret;
};

/*
 * @brief 发送flv文件
 */
bool CRtmpStream::SendFlvFile(const char *filename) {
    int size = 0;
    unsigned char buf[SEND_BUF_SIZE] = {0};

    m_pFid = fopen(filename, "rb");
    if (NULL == m_pFid) {
        printf("Can net open the file: %s\n", filename);
        return FALSE;
    }

    size = GetFlvHeader(buf);
    SendFlvHeader(buf, size);

    // 循环发送每一帧数据
    while (!feof(m_pFid)) {
        memset(buf, 0, SEND_BUF_SIZE);
        size = GetFlvFrame(buf);
        if (size > 0) {
            SendFlvFrame(buf, size);
            sleep(40);
        }
    }

    return TRUE;
}
