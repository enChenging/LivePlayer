#ifndef IRRIGATEDAREA_NATIVE_LIB_H
#define IRRIGATEDAREA_NATIVE_LIB_H

void callback(JNIEnv *env, uint8_t *buf, int channel, int width, int height);

void status_back(JNIEnv *env, int code, const char* log);

enum RTSP_Status {
    INFO_LOG = 0,
    SUCCESS = 1,
    OPEN_INPUT_ERR = -1,
    FIND_STREAM_INFO_ERR = -2,
    FIND_VIDEO_STREAM_ERR = -3,
    FIND_DECODER_ERR = -4,
    OPEN_DECODER_ERR = -5

};
#endif //IRRIGATEDAREA_NATIVE_LIB_H
