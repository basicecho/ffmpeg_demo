#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/pixfmt.h"

int main(int argc, char** argv) {
    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* codec_ctx = NULL;
    const AVCodec* codec = NULL;
    struct SwsContext* sws_ctx = NULL;
    AVFrame* frame = NULL;
    AVFrame* frame_yuv = NULL;
    AVPacket* pkt = NULL;
    const char* in_filename = "/dev/video0";
    const char* out_filename = "output.yuv";
    const char* fmt = "video4linux2";
    int video_stream = -1;
    int ret = 0;

    avdevice_register_all();

    const AVInputFormat* ifmt = av_find_input_format(fmt);
    if (!ifmt) {
        fprintf(stderr, "Could not find input format\n");
        goto end;
    }

    ret = avformat_open_input(&fmt_ctx, in_filename, ifmt, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input %s\n", in_filename);
        goto end;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to find stream information\n");
        goto end;
    }

    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
        fprintf(stderr, "Could find video stream\n");
        goto end;
    }

    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream]->codecpar;
    codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Could not find decoder\n");
        goto end;
    }

    

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Failed to allocate AVCodecContext\n");
        goto end;
    }

    ret = avcodec_parameters_to_context(codec_ctx, codecpar);

    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Failed to allocate frame\n");
        goto end;
    }

    frame_yuv = av_frame_alloc();
    if (!frame_yuv) {
        fprintf(stderr, "Failed to allocate frame_yuv\n");
        goto end;
    }

    frame_yuv->format = AV_PIX_FMT_YUV420P;
    frame_yuv->width = 640;
    frame_yuv->height = 480;
    ret = av_frame_get_buffer(frame_yuv, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not get buffer for frame_yuv\n");
        goto end;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Failed to allocate packet\n");
        goto end;
    }

    sws_ctx = sws_getContext(codecpar->width, codecpar->height, codecpar->format,
                             frame_yuv->width, frame_yuv->height, AV_PIX_FMT_YUV420P,
                             SWS_BICUBIC, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "Could not get SwsContext\n");
        goto end;
    }

    FILE* fp = fopen(out_filename, "wb+");
    if (!fp) {
        fprintf(stderr, "Could not open output file\n");
        goto end;
    }

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream) {
            ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to decoder\n");
                goto end;
            }

            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret < 0) {
                fprintf(stderr, "Error receiving frame from decoder\n");
                goto end;
            }

            sws_scale(sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0,
                      codecpar->height, frame_yuv->data, frame_yuv->linesize);

            int size = 640 * 480;
            fwrite(frame_yuv->data[0], 1, size, fp);
            fwrite(frame_yuv->data[1], 1, size / 4, fp);
            fwrite(frame_yuv->data[2], 1, size / 4, fp);
            break;
        }
        av_packet_unref(pkt);
    }

end:
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&frame_yuv);
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&codec_ctx);
    sws_freeContext(sws_ctx);
    fclose(fp);

    return ret;
}
