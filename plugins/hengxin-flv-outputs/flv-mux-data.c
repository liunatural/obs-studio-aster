#include <obs-module.h>
#include <util/array-serializer.h>
#include "flv-stream.h"

static void flv_video(struct serializer *s, int32_t dts_offset,
	struct encoder_packet *packet, bool is_header)
{
	int64_t offset = packet->pts - packet->dts;
	int32_t time_ms = get_ms_time(packet, packet->dts) - dts_offset;

	if (!packet->data || !packet->size)
		return;

	s_w8(s, RTMP_PACKET_TYPE_VIDEO);

#ifdef DEBUG_TIMESTAMPS
	blog(LOG_DEBUG, "Video: %lu", time_ms);

	if (last_time > time_ms)
		blog(LOG_DEBUG, "Non-monotonic");

	last_time = time_ms;
#endif

	s_wb24(s, (uint32_t)packet->size + 5);
	s_wb24(s, time_ms);
	s_w8(s, (time_ms >> 24) & 0x7F);
	s_wb24(s, 0);

	/* these are the 5 extra bytes mentioned above */
	s_w8(s, packet->keyframe ? 0x17 : 0x27);
	s_w8(s, is_header ? 0 : 1);
	s_wb24(s, get_ms_time(packet, offset));
	s_write(s, packet->data, packet->size);

	/* write tag size (starting byte doesn't count) */
	s_wb32(s, (uint32_t)serializer_get_pos(s) - 1);
}

static void flv_audio(struct serializer *s, int32_t dts_offset,
	struct encoder_packet *packet, bool is_header)
{
	int32_t time_ms = get_ms_time(packet, packet->dts) - dts_offset;

	if (!packet->data || !packet->size)
		return;

	s_w8(s, RTMP_PACKET_TYPE_AUDIO);

#ifdef DEBUG_TIMESTAMPS
	blog(LOG_DEBUG, "Audio: %lu", time_ms);

	if (last_time > time_ms)
		blog(LOG_DEBUG, "Non-monotonic");

	last_time = time_ms;
#endif

	s_wb24(s, (uint32_t)packet->size + 2);
	s_wb24(s, time_ms);
	s_w8(s, (time_ms >> 24) & 0x7F);
	s_wb24(s, 0);

	/* these are the two extra bytes mentioned above */
	s_w8(s, 0xaf);
	s_w8(s, is_header ? 0 : 1);
	s_write(s, packet->data, packet->size);

	/* write tag size (starting byte doesn't count) */
	s_wb32(s, (uint32_t)serializer_get_pos(s) - 1);
}


static void flv_packet_mux(struct encoder_packet *packet, int32_t dts_offset,
	uint8_t **output, size_t *size, bool is_header)
{
	struct array_output_data data;
	struct serializer s;

	array_output_serializer_init(&s, &data);

	if (packet->type == OBS_ENCODER_VIDEO)
		flv_video(&s, dts_offset, packet, is_header);
	else
		flv_audio(&s, dts_offset, packet, is_header);

	*output = data.bytes.array;
	*size = data.bytes.num;
}

extern bool send_flv_stream(struct flv_stream *stream, char* data, int len, double dTimer);

int send_packet(struct flv_stream *stream,
	struct encoder_packet *packet, bool is_header, size_t idx)
{
	uint8_t *data;
	size_t  size;
	int     recv_size = 0;
	int     ret = 0;


	flv_packet_mux(packet, is_header ? 0 : stream->start_dts_offset,
		&data, &size, is_header);

#ifdef TEST_FRAMEDROPS
	droptest_cap_data_rate(stream, size);
#endif

	ret = send_flv_stream(stream, (char*)data, (int)size, packet->dTimer);
	bfree(data);

	if (is_header)
		bfree(packet->data);
	else
		obs_encoder_packet_release(packet);

	stream->total_bytes_sent += size;
	return ret;
}


//static bool send_audio_header(struct flv_stream *stream, size_t idx,
//	bool *next)
//{
//	obs_output_t  *context = stream->output;
//	obs_encoder_t *aencoder = obs_output_get_audio_encoder(context, idx);
//	uint8_t       *header;
//
//	struct encoder_packet packet = {
//		.type = OBS_ENCODER_AUDIO,
//		.timebase_den = 1
//	};
//
//	if (!aencoder) {
//		*next = false;
//		return true;
//	}
//
//	obs_encoder_get_extra_data(aencoder, &header, &packet.size);
//	packet.data = bmemdup(header, packet.size);
//	return send_packet(stream, &packet, true, idx) >= 0;
//}
//
//
//static bool send_video_header(struct flv_stream *stream)
//{
//	obs_output_t  *context = stream->output;
//	obs_encoder_t *vencoder = obs_output_get_video_encoder(context);
//	uint8_t       *header;
//	size_t        size;
//
//	struct encoder_packet packet = {
//		.type = OBS_ENCODER_VIDEO,
//		.timebase_den = 1,
//		.keyframe = true
//	};
//
//	obs_encoder_get_extra_data(vencoder, &header, &size);
//	packet.size = obs_parse_avc_header(&packet.data, header, size);
//	return send_packet(stream, &packet, true, 0) >= 0;
//}
//
//bool send_flv_headers(struct flv_stream *stream)
//{
//	size_t i = 0;
//	bool next = true;
//
//	if (!send_audio_header(stream, i++, &next))
//		return false;
//	if (!send_video_header(stream))
//		return false;
//
//	while (next) {
//		if (!send_audio_header(stream, i++, &next))
//			return false;
//	}
//
//	stream->sent_headers = true;
//	return true;
//}

bool get_audio_header(struct flv_stream *stream, size_t idx, bool *next, uint8_t **data, size_t *size)
{
	obs_output_t  *context = stream->output;
	obs_encoder_t *aencoder = obs_output_get_audio_encoder(context, idx);
	uint8_t       *header;

	struct encoder_packet packet = {
		.type = OBS_ENCODER_AUDIO,
		.timebase_den = 1
	};

	if (NULL == aencoder) {
		*next = false;
		return false;
	}

	obs_encoder_get_extra_data(aencoder, &header, &packet.size);
	packet.data = bmemdup(header, packet.size);


	flv_packet_mux(&packet, 0, data, size, 1);

	bfree(packet.data);
	stream->total_bytes_sent += *size;

	return true;
}

bool get_video_header(struct flv_stream *stream, uint8_t **data, size_t *size)
{
	obs_output_t  *context = stream->output;
	obs_encoder_t *vencoder = obs_output_get_video_encoder(context);
	uint8_t       *header;
	size_t        temp_size;

	struct encoder_packet packet = {
		.type = OBS_ENCODER_VIDEO,
		.timebase_den = 1,
		.keyframe = true
	};

	obs_encoder_get_extra_data(vencoder, &header, &temp_size);
	packet.size = obs_parse_avc_header(&packet.data, header, temp_size);

	flv_packet_mux(&packet, 0, data, size, 1);

	bfree(packet.data);
	stream->total_bytes_sent += *size;

	return true;
}
