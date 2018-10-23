#pragma once
#include <obs-module.h>
#include <util/array-serializer.h>
#include "obs-output-ver.h"
#include "flv-stream.h"
#include "librtmp/amf.h"

static inline AVal *flv_str(AVal *out, const char *str)
{
	out->av_val = (char*)str;
	out->av_len = (int)strlen(str);
	return out;
}

static inline void enc_str(char **enc, char *end, const char *str)
{
	AVal s;
	*enc = AMF_EncodeString(*enc, end, flv_str(&s, str));
}

static inline void enc_num_val(char **enc, char *end, const char *name,
	double val)
{
	AVal s;
	*enc = AMF_EncodeNamedNumber(*enc, end, flv_str(&s, name), val);
}

static inline void enc_str_val(char **enc, char *end, const char *name,
	const char *val)
{
	AVal s1, s2;
	*enc = AMF_EncodeNamedString(*enc, end,
		flv_str(&s1, name),
		flv_str(&s2, val));
}

static inline void enc_bool_val(char **enc, char *end, const char *name,
	bool val)
{
	AVal s;
	*enc = AMF_EncodeNamedBoolean(*enc, end, flv_str(&s, name), val);
}


//----------------------------------------------------------------------------------------//
static inline double encoder_bitrate(obs_encoder_t *encoder)
{
	obs_data_t *settings = obs_encoder_get_settings(encoder);
	double bitrate = obs_data_get_double(settings, "bitrate");

	obs_data_release(settings);
	return bitrate;
}



static bool build_flv_meta_data(obs_output_t *context,
	uint8_t **output, size_t *size, size_t a_idx)
{
	obs_encoder_t *vencoder = obs_output_get_video_encoder(context);
	obs_encoder_t *aencoder = obs_output_get_audio_encoder(context, a_idx);
	video_t       *video = obs_encoder_video(vencoder);
	audio_t       *audio = obs_encoder_audio(aencoder);
	char buf[4096];
	char *enc = buf;
	char *end = enc + sizeof(buf);
	struct dstr encoder_name = { 0 };

	if (a_idx > 0 && !aencoder)
		return false;

	enc_str(&enc, end, "onMetaData");

	*enc++ = AMF_ECMA_ARRAY;
	enc = AMF_EncodeInt32(enc, end, a_idx == 0 ? 14 : 9);

	enc_num_val(&enc, end, "duration", 0.0);
	enc_num_val(&enc, end, "fileSize", 0.0);

	if (a_idx == 0) {
		enc_num_val(&enc, end, "width",
			(double)obs_encoder_get_width(vencoder));
		enc_num_val(&enc, end, "height",
			(double)obs_encoder_get_height(vencoder));

		enc_str_val(&enc, end, "videocodecid", "avc1");
		enc_num_val(&enc, end, "videodatarate",
			encoder_bitrate(vencoder));
		enc_num_val(&enc, end, "framerate",
			video_output_get_frame_rate(video));
	}

	enc_str_val(&enc, end, "audiocodecid", "mp4a");
	enc_num_val(&enc, end, "audiodatarate", encoder_bitrate(aencoder));
	enc_num_val(&enc, end, "audiosamplerate",
		(double)obs_encoder_get_sample_rate(aencoder));
	enc_num_val(&enc, end, "audiosamplesize", 16.0);
	enc_num_val(&enc, end, "audiochannels",
		(double)audio_output_get_channels(audio));

	enc_bool_val(&enc, end, "stereo",
		audio_output_get_channels(audio) == 2);

	dstr_printf(&encoder_name, "%s (libobs version ",
		MODULE_NAME);

#ifdef HAVE_OBSCONFIG_H
	dstr_cat(&encoder_name, OBS_VERSION);
#else
	dstr_catf(&encoder_name, "%d.%d.%d",
		LIBOBS_API_MAJOR_VER,
		LIBOBS_API_MINOR_VER,
		LIBOBS_API_PATCH_VER);
#endif

	dstr_cat(&encoder_name, ")");

	enc_str_val(&enc, end, "encoder", encoder_name.array);
	dstr_free(&encoder_name);

	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	*size = enc - buf;
	*output = bmemdup(buf, *size);
	return true;
}



static bool get_flv_meta_data(obs_output_t *context, uint8_t **output, size_t *size, bool write_header, size_t audio_idx)
{
	struct array_output_data data;
	struct serializer s;
	uint8_t *meta_data = NULL;
	size_t  meta_data_size;
	uint32_t start_pos;

	array_output_serializer_init(&s, &data);

	if (!build_flv_meta_data(context, &meta_data, &meta_data_size,
		audio_idx)) {
		bfree(meta_data);
		return false;
	}

	if (write_header) {
		s_write(&s, "FLV", 3);
		s_w8(&s, 1);
		s_w8(&s, 5);
		s_wb32(&s, 9);
		s_wb32(&s, 0);
	}

	start_pos = serializer_get_pos(&s);

	s_w8(&s, RTMP_PACKET_TYPE_INFO);

	s_wb24(&s, (uint32_t)meta_data_size);
	s_wb32(&s, 0);
	s_wb24(&s, 0);

	s_write(&s, meta_data, meta_data_size);

	s_wb32(&s, (uint32_t)serializer_get_pos(&s) - start_pos - 1);

	*output = data.bytes.array;
	*size = data.bytes.num;

	bfree(meta_data);
	return true;
}


//extern bool send_flv_stream(struct flv_stream *stream, char* data, int len);
//
//bool sent_flv_meta_data(struct flv_stream *stream, size_t idx, bool *next)
//{
//	uint8_t *meta_data;
//	size_t  meta_data_size;
//	bool    success = true;
//
//	*next = get_flv_meta_data(stream->output, &meta_data, &meta_data_size, true, idx);
//
//	if (*next) {
//		success = send_flv_stream(stream, (char*)meta_data, (int)meta_data_size);
//		bfree(meta_data);
//	}
//
//	return success;
//}

bool get_meta_data(struct flv_stream *stream, uint8_t **data, size_t *size, size_t idx)
{
	return  get_flv_meta_data(stream->output, data, size, true, idx);
}