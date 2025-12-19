#include <iostream>
#include <fstream>
#include <string>

extern "C" 
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/avutil.h>
    #include <libswscale/swscale.h>
}
using namespace std;
static void save_pgm(const AVFrame* frame, int width, int height, int index) 
{
    string filename = "frame_" + to_string(index) + ".pgm";
    ofstream ofs(filename, ios::binary);
    ofs << "P5" << endl << width << " " << height << endl << "255" << endl;
    for (int y = 0; y < height; y++) {
        ofs.write(reinterpret_cast<const char*>(frame->data[0] + y * frame->linesize[0]),
                  width);
    }

    ofs.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input video>" << endl;
        return 1;
    }

    const char* input_filename = argv[1];
    AVFormatContext* fmt_ctx = nullptr;

    if (avformat_open_input(&fmt_ctx, input_filename, nullptr, nullptr) < 0) {
        cerr << "Could not open input file" << endl;
        return 1;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        cerr << "Could not find stream information" << endl;
        return 1;
    }

    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        cerr << "No video stream found" << endl;
        return 1;
    }

    AVStream* video_stream = fmt_ctx->streams[video_stream_index];
    AVCodecParameters* codecpar = video_stream->codecpar;
    string codec_name = avcodec_get_name(codecpar->codec_id);
    cout << "Resolution: " << codecpar->width << "x" << codecpar->height << endl;
    cout << "Pixel format: " << av_get_pix_fmt_name((AVPixelFormat)codecpar->format) << endl;
    cout << "Frame rate: " << av_q2d(video_stream->avg_frame_rate) << " fps" << endl;
    cout << "Codec: " << codec_name << endl;

    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        cerr << "Decoder not found" << endl;
        return 1;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec_ctx, codecpar);

    if (avcodec_open2(codec_ctx, decoder, nullptr) < 0) {
        cerr << "Could not open codec" << endl;
        return 1;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* gray_frame = av_frame_alloc();

    SwsContext* sws_ctx = sws_getContext(
        codec_ctx->width,
        codec_ctx->height,
        codec_ctx->pix_fmt,
        codec_ctx->width,
        codec_ctx->height,
        AV_PIX_FMT_GRAY8,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr
    );

    int gray_buf_size = av_image_alloc(
        gray_frame->data,
        gray_frame->linesize,
        codec_ctx->width,
        codec_ctx->height,
        AV_PIX_FMT_GRAY8,
        1
    );

    gray_frame->width = codec_ctx->width;
    gray_frame->height = codec_ctx->height;
    gray_frame->format = AV_PIX_FMT_GRAY8;

    int frame_count = 0;

    while (av_read_frame(fmt_ctx, packet) >= 0 && frame_count < 50) {
        if (packet->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, packet) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    sws_scale(
                        sws_ctx,
                        frame->data,
                        frame->linesize,
                        0,
                        codec_ctx->height,
                        gray_frame->data,
                        gray_frame->linesize
                    );

                    save_pgm(gray_frame,
                             codec_ctx->width,
                             codec_ctx->height,
                             frame_count);

                    frame_count++;
                    if (frame_count >= 50)
                        break;
                }
            }
        }
        av_packet_unref(packet);
    }

    cout << "Saved 50 frames as PGM images" << endl;

    av_freep(&gray_frame->data[0]);
    av_frame_free(&gray_frame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}
