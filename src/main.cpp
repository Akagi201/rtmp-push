/*
 * @file main.cpp
 * @author Akagi201
 * @date 2015/02/10
 */

#include "CRtmpStream.h"

int main(int argc, char *argv[]) {
    bool ret = FALSE;
    const char *url = "rtmp://localhost/live/inrtmp";

    CRtmpStream rtmpStream;
    ret = rtmpStream.Connect(url);

    const char *filename = "input.flv";
    ret = rtmpStream.SendFlvFile(filename);

    return 0;
}