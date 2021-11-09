/*
 *  RTP streaming  (only video track)
 *  support file: .h264 .ps .ts
 *  Eg. ./a.out  ./test.h264 rtp://127.0.0.1:5002
 *
 */
#include <stdio.h>
#include <string.h>

#include <libavutil/avassert.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#define _TRACE
//#define _LOOP

int main(int argc, char *argv[])
{
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    const char *in_filename, *out_filename;
    int ret;
    int i;
    int idx_video = -1;
    AVStream *in_video_strm = NULL, *out_video_strm = NULL;

    if (argc < 3)
    {
        fprintf(stderr, "Usage:%s input output\n", argv[0]);
        return 1;
    }
    in_filename = argv[1];
    out_filename = argv[2];

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0)
    {
        fprintf(stderr, "Could not open input file '%s'", in_filename);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
    {
        fprintf(stderr, "Failed to retrieve input stream information");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream *st = ifmt_ctx->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            idx_video = i;
            in_video_strm = st;
        }
        printf("%d %s\n", i, av_get_media_type_string(st->codecpar->codec_type));
    }

    if (idx_video < 0)
    {
        fprintf(stderr, "Input file no video trace\n");
        goto end;
    }

    // setup video out fromat
    if (idx_video >= 0)
    {
        ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtp", NULL);
        if (ret < 0)
        {
            fprintf(stderr, "Could not create output context, ret:%d\n", ret);
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
        AVCodecParameters *in_codecpar = in_video_strm->codecpar;
        if (!out_stream)
        {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to copy codec parameters\n");
            goto end;
        }
        out_stream->codecpar->codec_tag = 0;
        out_video_strm = out_stream;
        av_dump_format(ofmt_ctx, 0, out_filename, 1);
        ofmt = ofmt_ctx->oformat;

        if (!(ofmt->flags & AVFMT_NOFILE))
        {
            ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
            if (ret < 0)
            {
                fprintf(stderr, "Could not open output file '%s'", out_filename);
                goto end;
            }
        }
    }

    //av_dict_set(&opt, "-fflags", "+genpts", 0);
    AVDictionary *opt = NULL;
    // rtp payload_type
    //av_dict_set_int(&opt, "payload_type", 97, 0);
    // rtp ssrc
    //av_dict_set_int(&opt, "ssrc", 0x88, 0);
    ret = avformat_write_header(ofmt_ctx, &opt);
    if (ret < 0)
    {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }

    // loop read data
    AVPacket pkt;
    while (1)
    //while (av_read_frame(ifmt_ctx, &pkt) >= 0)
    {
        if (av_read_frame(ifmt_ctx, &pkt) < 0)
        {
#ifdef _LOOP
            // Repeat again...
            avio_seek(ifmt_ctx->pb, 0, SEEK_SET);
            //avformat_seek_file(ifmt_ctx, idx_video, 0, 0, in_video_strm->duration, 0);
            printf("repeat\n");
#else
        printf("finished.\n");
        break;
#endif
        }
        if (pkt.stream_index == idx_video)
        {
#ifdef _TRACE
            printf("pts = %ld, dts= %ld, duration:%ld\n", pkt.pts, pkt.dts, pkt.duration);
            printf("in_timebase:%d/%d in_framerate:%.2f, out_timebase:%d/%d %.2f\n",
                   in_video_strm->time_base.num,
                   in_video_strm->time_base.den,
                   av_q2d(in_video_strm->r_frame_rate),
                   out_video_strm->time_base.num,
                   out_video_strm->time_base.den,
                   av_q2d(out_video_strm->r_frame_rate));
#endif

            //if (pkt.pts == AV_NOPTS_VALUE)
            {
                /*
                 * NO PTS  or Loop,  need generate new PTS
                 *
                 */
                static int64_t frame_index = 0;
                pkt.pts = pkt.dts = frame_index++ * pkt.duration;
            }

            // delay
            {
                AVRational fr = av_inv_q(in_video_strm->r_frame_rate);
                int64_t delay = av_rescale_q(1, fr, AV_TIME_BASE_Q);
                av_usleep(delay);
            }

            //update pts/dts
            pkt.pts = av_rescale_q(pkt.pts, in_video_strm->time_base, out_video_strm->time_base);
            pkt.dts = av_rescale_q(pkt.dts, in_video_strm->time_base, out_video_strm->time_base);

            av_interleaved_write_frame(ofmt_ctx, &pkt);
        }
        av_packet_unref(&pkt);
    }

    // flush
    av_interleaved_write_frame(ofmt_ctx, NULL);
    av_write_trailer(ofmt_ctx);
end:
    av_usleep(1000 * 100); // 100ms
    // close input
    if (ifmt_ctx)
    {
        avformat_close_input(&ifmt_ctx);
    }

    /* free output */
    if (ofmt_ctx)
    {
        if (!(ofmt->flags & AVFMT_NOFILE))
            avio_closep(&ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
    }

    return 0;
}

