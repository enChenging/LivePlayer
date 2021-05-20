#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <unistd.h>
#include <err.h>
#include "native-lib.h"
#include <locale>
#include <codecvt>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

const char *TAG = __FILE__;

jobject gCallback;
jmethodID gVideoCallback, gStatusCallBack;

volatile bool isStop = false;

extern "C"
jint Java_com_example_liveplayer_rtsp_Client_init(JNIEnv* env,jobject,jobject callback) {
    isStop = false;
    gCallback = env->NewGlobalRef(callback);
    jclass clz = env->GetObjectClass(gCallback);
    if (clz == nullptr) {
        return JNI_ERR;
    } else {
        gVideoCallback = env->GetMethodID(clz, "onFrame", "([BIII)V");
        gStatusCallBack = env->GetMethodID(clz, "onStatusBack", "(ILjava/lang/String;)V");
        env->DeleteLocalRef(clz);
        return JNI_OK;
    }
}

static AVPacket *pAvPacket;
static AVCodecContext *pCodecCtx;
struct SwsContext *pSwsCtx;
static AVFormatContext *pFormatCtx;
static AVCodec *pCodec = nullptr;
static AVDictionary *pAvDic = nullptr;
static const char *pRtspUrl;
extern "C"
jint Java_com_example_liveplayer_rtsp_Client_play(
        JNIEnv *env, jobject, jstring URL) {
    try {
        /*
         * 获取RTSP URL地址
         * */
        pRtspUrl = env->GetStringUTFChars(URL, JNI_FALSE);
        /*
         * 初始化
         * */
        pFormatCtx = avformat_alloc_context();
        pCodecCtx = avcodec_alloc_context3(nullptr);
        av_register_all();
        avformat_network_init();
        /*
         * 分配packet空间
         * */
        pAvPacket = (AVPacket *) av_malloc(sizeof(AVPacket));
        status_back(env, INFO_LOG, "packet数据空间初始化成功");
        /*
         * 设置网络参数
         * */
        av_dict_set(&pAvDic, "buffer_size", "1024000", 0);
        av_dict_set(&pAvDic, "max_delay", "500000", 0);
        av_dict_set(&pAvDic, "stimeout", "20000000", 0);
        av_dict_set(&pAvDic, "rtsp_transport", "tcp", 0);
        //av_dict_set(&pAvDic, "rtsp_transport", "udp", 0); //以udp方式打开，如果以tcp方式打开将udp替换为tcp
        /*
         * 打开网络流或文件
         * */
        if (int err_code = avformat_open_input(&pFormatCtx, pRtspUrl, nullptr, &pAvDic)) {
            status_back(env, OPEN_INPUT_ERR, "视频流打开失败,请检查网络");
            char errors[1024];
            av_strerror(err_code, errors, 1024);
            __android_log_print(ANDROID_LOG_ERROR, TAG, "无法打开输入流 %s, 错误码: %s",
                                pRtspUrl,
                                errors);
            return JNI_ERR;
        }
        status_back(env, INFO_LOG, "视频流打开成功");
        /*
         * 部分资源释放
         * */
        env->ReleaseStringUTFChars(URL, pRtspUrl);
        av_dict_free(&pAvDic);
        /*
         * 获取视频流信息
         * */
        if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
            status_back(env, FIND_STREAM_INFO_ERR, "找不到视频流信息");
            return JNI_ERR;
        }
        status_back(env, INFO_LOG, "视频流信息打开成功");
        /*
         * 查找视频类型的流
         * */
        int video_stream_index = -1;
        for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
            if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
                //视频流从ifmt_ctx->streams[video_stream_index]取
                video_stream_index = i;
                break;
            }
        }
        if (video_stream_index == -1) {
            status_back(env, FIND_VIDEO_STREAM_ERR, "没有找到视频流");
            return JNI_ERR;
        }
        status_back(env, INFO_LOG, "查询到视频流");
        /*
         * 打开输出文件
         * */
        AVFormatContext *oc = avformat_alloc_context();
        AVStream *stream = nullptr;
        //av_read_play(pFormatCtx);
        pCodecCtx = pFormatCtx->streams[video_stream_index]->codec;
        /*
         * 使用流的解码器
         * AV_CODEC_ID_HEVC AV_CODEC_ID_H264
         * */
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
        if (!pCodec) {
            status_back(env, FIND_DECODER_ERR, "获取解码器失败");
            return JNI_ERR;
        }
        status_back(env, INFO_LOG, "获取解码器成功");
        /*
         * 打开解码器
         * */
        if (int err_code = avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
            status_back(env, OPEN_DECODER_ERR, "打开解码器失败");
            char error_s[1024];
            av_strerror(err_code, error_s, 1024);
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Cannot open codec error code: %s,%d",
                                error_s,
                                err_code);
            return JNI_ERR;
        }
        status_back(env, INFO_LOG, "打开解码器成功");
        /*
         * 设置数据转换参数
         * */
        pSwsCtx = sws_getContext(
                //源视频图像长宽以及数据格式
                pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                //目标视频图像长宽以及数据格式
                pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24,
                //算法类型 AV_PIX_FMT_YUV420P AV_PIX_FMT_RGB24
                SWS_BICUBIC, nullptr, nullptr, nullptr);

        /*
         * 为每一帧图像数据分配空间
         * */
        size_t size = (size_t) avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,
                                                  pCodecCtx->height);
        uint8_t *picture_buf = (uint8_t *) (av_malloc(size));
        AVFrame *pic = av_frame_alloc();
        AVFrame *picrgb = av_frame_alloc();
        size_t size2 = (size_t) avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
                                                   pCodecCtx->height);
        uint8_t *picture_buf2 = (uint8_t *) (av_malloc(size2));
        avpicture_fill((AVPicture *) pic, picture_buf, AV_PIX_FMT_YUV420P, pCodecCtx->width,
                       pCodecCtx->height);
        avpicture_fill((AVPicture *) picrgb, picture_buf2, AV_PIX_FMT_RGB24, pCodecCtx->width,
                       pCodecCtx->height);
        /*
         * 读取视频并且存入AVPacket结构中
         * */
        while (!isStop && av_read_frame(pFormatCtx, pAvPacket) >= 0) {
            if (pAvPacket->stream_index == video_stream_index) {
                if (stream == nullptr) {
                    stream = avformat_new_stream(oc,
                                                 pFormatCtx->streams[video_stream_index]->codec->codec);
                    avcodec_copy_context(stream->codec,
                                         pFormatCtx->streams[video_stream_index]->codec);
                    stream->sample_aspect_ratio = pFormatCtx->streams[video_stream_index]->codec->sample_aspect_ratio;
                }
                int check = 0;
                pAvPacket->stream_index = stream->id;
                avcodec_decode_video2(pCodecCtx, pic, &check, pAvPacket);
                sws_scale(pSwsCtx, (const uint8_t *const *) pic->data, pic->linesize, 0,
                          pCodecCtx->height, picrgb->data, picrgb->linesize);
                if (gCallback != nullptr) {
                    status_back(env, SUCCESS, "");
                    callback(env, picture_buf2, 3, pCodecCtx->width, pCodecCtx->height);
                }
            }
            av_init_packet(pAvPacket);
            av_packet_unref(pAvPacket);
        }
        /*
         * 释放资源
         * */
        av_packet_unref(pAvPacket);
        av_free(pic);
        av_free(picrgb);
        av_free(picture_buf);
        av_free(picture_buf2);
        av_read_pause(pFormatCtx);
        avio_close(oc->pb);
        avformat_free_context(oc);
        avformat_close_input(&pFormatCtx);
    } catch (std::exception &e) {
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }

    return isStop ? JNI_OK : JNI_ERR;
}

void callback(JNIEnv *env, uint8_t *buf, int nChannel, int width, int height) {
    try {
        int len = nChannel * width * height;
        jbyteArray gByteArray = env->NewByteArray(len);
        env->SetByteArrayRegion(gByteArray, 0, len, (jbyte *) buf);
        env->CallVoidMethod(gCallback, gVideoCallback, gByteArray, nChannel, width, height);
        env->DeleteLocalRef(gByteArray);
    } catch (std::exception &e) {
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
}

void status_back(JNIEnv *env, int code, const char *log) {
    try {
        __android_log_print(ANDROID_LOG_INFO, TAG, "%s", log);
        jstring logStr = env->NewStringUTF(log);
        env->CallVoidMethod(gCallback, gStatusCallBack, code, logStr);
    } catch (std::exception &e) {
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
}

extern "C"
void Java_com_example_liveplayer_rtsp_Client_stop(JNIEnv *env, jobject) {
    isStop = true;
}

extern "C"
void Java_com_example_liveplayer_rtsp_Client_dispose(JNIEnv *env, jobject) {
    try {
        env->DeleteGlobalRef(gCallback);
        gCallback = nullptr;
    } catch (std::exception &e) {
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
}

/*
static std::string UTF16StringToUTF8String(const char16_t* chars, size_t len) {
    std::u16string u16_string(chars, len);
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}
            .to_bytes(u16_string);
}

extern "C"
jstring Java_com_example_liveplayer_MainActivity_stringFromJNI(JNIEnv* env,jobject,
                                                               jstring msg) {
    std::string hello = "Hello from C++";
    const jchar *chars = env->GetStringChars(msg, nullptr);
    std::string u8_string = UTF16StringToUTF8String(
            reinterpret_cast<const char16_t*>(chars), env->GetStringLength(msg));
    env->ReleaseStringChars(msg, chars);

    hello.append(u8_string);
    return env->NewStringUTF(hello.c_str());
}

static std::u16string UTF8StringToUTF16String(const std::string& string) {
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}
            .from_bytes(string);
}

jstring StringToJavaString(JNIEnv* env,
                           const std::string& u8_string) {
    std::u16string u16_string = UTF8StringToUTF16String(u8_string);
    auto result =env->NewString(reinterpret_cast<const jchar*>(u16_string.data()),
                                u16_string.length());
    return result;
}

 */