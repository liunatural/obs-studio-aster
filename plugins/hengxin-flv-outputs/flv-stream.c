#include <obs-module.h>
#include "flv-stream.h"

static const char *hengxin_flv_stream_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("HengXinFlvStream");
}


static inline size_t num_buffered_packets(struct flv_stream *stream)
{
	return stream->packets.size / sizeof(struct encoder_packet);
}

static inline void free_packets(struct flv_stream *stream)
{
	size_t num_packets;

	pthread_mutex_lock(&stream->packets_mutex);

	num_packets = num_buffered_packets(stream);
	if (num_packets)
		info("Freeing %d remaining packets", (int)num_packets);

	while (stream->packets.size) {
		struct encoder_packet packet;
		circlebuf_pop_front(&stream->packets, &packet, sizeof(packet));
		obs_encoder_packet_release(&packet);
	}
	pthread_mutex_unlock(&stream->packets_mutex);
}

bool stopping(struct flv_stream *stream)
{
	return os_event_try(stream->stop_event) != EAGAIN;
}

bool connecting(struct flv_stream *stream)
{
	return os_atomic_load_bool(&stream->connecting);
}

bool active(struct flv_stream *stream)
{
	return os_atomic_load_bool(&stream->active);
}

bool disconnected(struct flv_stream *stream)
{
	return os_atomic_load_bool(&stream->disconnected);
}


bool disconnect_flv_stream(struct flv_stream *stream, bool val)
{
	return os_atomic_set_bool(&stream->disconnected, val);
}


static void hengxin_flv_stream_destroy(void *data)
{
	struct flv_stream *stream = data;

	if (stopping(stream) && !connecting(stream)) {
		pthread_join(stream->send_thread, NULL);

	}
	else if (connecting(stream) || active(stream)) {
		if (stream->connecting)
			pthread_join(stream->connect_thread, NULL);

		stream->stop_ts = 0;
		os_event_signal(stream->stop_event);

		if (active(stream)) {
			os_sem_post(stream->send_sem);
			obs_output_end_data_capture(stream->output);
			pthread_join(stream->send_thread, NULL);
		}
	}

	free_packets(stream);
	//dstr_free(&stream->path);
	//dstr_free(&stream->key);
	//dstr_free(&stream->username);
	//dstr_free(&stream->password);
	//dstr_free(&stream->encoder_name);
	//dstr_free(&stream->bind_ip);
	os_event_destroy(stream->stop_event);
	os_sem_destroy(stream->send_sem);
	pthread_mutex_destroy(&stream->packets_mutex);
	circlebuf_free(&stream->packets);
#ifdef TEST_FRAMEDROPS
	circlebuf_free(&stream->droptest_info);
#endif

	os_event_destroy(stream->buffer_space_available_event);
	os_event_destroy(stream->buffer_has_data_event);
	//os_event_destroy(stream->socket_available_event);
	os_event_destroy(stream->send_thread_signaled_exit);
	pthread_mutex_destroy(&stream->write_buf_mutex);

	if (stream->write_buf)
		bfree(stream->write_buf);
	bfree(stream);
}

static void *hengxin_flv_stream_create(obs_data_t *settings, obs_output_t *output)
{
	struct flv_stream *stream = bzalloc(sizeof(struct flv_stream));
	stream->output = output;
	pthread_mutex_init_value(&stream->packets_mutex);


	if (pthread_mutex_init(&stream->packets_mutex, NULL) != 0)
		goto fail;
	if (os_event_init(&stream->stop_event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;

	if (pthread_mutex_init(&stream->write_buf_mutex, NULL) != 0) {
		warn("Failed to initialize write buffer mutex");
		goto fail;
	}

	if (os_event_init(&stream->buffer_space_available_event,
		OS_EVENT_TYPE_AUTO) != 0) {
		warn("Failed to initialize write buffer event");
		goto fail;
	}
	if (os_event_init(&stream->buffer_has_data_event,
		OS_EVENT_TYPE_AUTO) != 0) {
		warn("Failed to initialize data buffer event");
		goto fail;
	}
	//if (os_event_init(&stream->socket_available_event,
	//	OS_EVENT_TYPE_AUTO) != 0) {
	//	warn("Failed to initialize socket buffer event");
	//	goto fail;
	//}
	if (os_event_init(&stream->send_thread_signaled_exit,
		OS_EVENT_TYPE_MANUAL) != 0) {
		warn("Failed to initialize socket exit event");
		goto fail;
	}

	UNUSED_PARAMETER(settings);
	return stream;

fail:
	hengxin_flv_stream_destroy(stream);
	return NULL;
}


static inline bool get_next_packet(struct flv_stream *stream,
	struct encoder_packet *packet)
{
	bool new_packet = false;

	pthread_mutex_lock(&stream->packets_mutex);
	if (stream->packets.size) {
		circlebuf_pop_front(&stream->packets, packet,
			sizeof(struct encoder_packet));
		new_packet = true;
	}
	pthread_mutex_unlock(&stream->packets_mutex);

	return new_packet;
}


static inline bool can_shutdown_stream(struct flv_stream *stream,
	struct encoder_packet *packet)
{
	uint64_t cur_time = os_gettime_ns();
	bool timeout = cur_time >= stream->shutdown_timeout_ts;

	if (timeout)
		info("Stream shutdown timeout reached (%d second(s))",
			stream->max_shutdown_time_sec);

	return timeout || packet->sys_dts_usec >= (int64_t)stream->stop_ts;
}

extern int send_packet(struct flv_stream *stream,
	struct encoder_packet *packet, bool is_header, size_t idx);


extern bool send_flv_headers(struct flv_stream *stream);

static void *send_thread(void *data)
{
	struct flv_stream *stream = data;

	os_set_thread_name("flv-stream: send_thread");

	while (os_sem_wait(stream->send_sem) == 0) {
		struct encoder_packet packet;

		if (stopping(stream) && stream->stop_ts == 0) {
			break;
		}

		if (!get_next_packet(stream, &packet))
			continue;

		if (stopping(stream)) {
			if (can_shutdown_stream(stream, &packet)) {
				obs_encoder_packet_release(&packet);
				break;
			}
		}
		//如果还没有发送过flv消息头， 则会此次flv流数据发送
		send_flv_headers(stream);

		send_packet(stream, &packet, false, packet.track_idx);

		//if (send_packet(stream, &packet, false, packet.track_idx) < 0) {
		//	os_atomic_set_bool(&stream->disconnected, true);
		//	break;
		//}
	}

	if (disconnected(stream)) 
	{
		info("Disconnected due to sending flv packet failed in send_thread");
	}
	else 
	{
		info("User stopped the stream");
	}

	//
	if (!stopping(stream))
	{
		pthread_detach(stream->send_thread);
		obs_output_signal_stop(stream->output, OBS_OUTPUT_DISCONNECTED);
	}
	else 
	{
		obs_output_end_data_capture(stream->output);
	}

	free_packets(stream);
	os_event_reset(stream->stop_event);  //设置stop_even为无信号状态
	os_atomic_set_bool(&stream->active, false); //设置FLV流为非活动状态
	stream->sent_headers = false;
	return NULL;

}

static inline bool reset_semaphore(struct flv_stream *stream)
{
	os_sem_destroy(stream->send_sem);
	return os_sem_init(&stream->send_sem, 0) == 0;
}


extern void create_flv_output_connect_thread(void *data);

void *connect_thread(void *data)
{
	struct flv_stream *stream = data;
	int ret;

	os_set_thread_name("flv-stream-output: connect_thread");

	ret = pthread_create(&stream->flv_output_connect_thread, NULL, create_flv_output_connect_thread, stream);
	if (ret != 0) {
		warn("Failed to create flv stream output thread");
		return OBS_OUTPUT_ERROR;
	}


	reset_semaphore(stream);

	ret = pthread_create(&stream->send_thread, NULL, send_thread, stream);
	if (ret != 0) {
		warn("Failed to create send thread");
		return OBS_OUTPUT_ERROR;
	}

	//if (!init_connect(stream)) {
	//	obs_output_signal_stop(stream->output, OBS_OUTPUT_BAD_PATH);
	//	return NULL;
	//}

	//ret = try_connect(stream);

	//if (ret != OBS_OUTPUT_SUCCESS) {
	//	obs_output_signal_stop(stream->output, ret);
	//	info("Connection to %s failed: %d", stream->path.array, ret);
	//}

	if (!stopping(stream))
		pthread_detach(stream->connect_thread);

	os_atomic_set_bool(&stream->connecting, false);

	os_atomic_set_bool(&stream->active, true);

	obs_output_begin_data_capture(stream->output, 0);

	return NULL;
}

static bool hengxin_flv_stream_start(void *data)
{
	struct flv_stream *stream = data;

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	os_atomic_set_bool(&stream->connecting, true);
	return pthread_create(&stream->connect_thread, NULL, connect_thread,
		stream) == 0;
}

static void hengxin_flv_stream_stop(void *data, uint64_t ts)
{
	struct flv_stream *stream = data;

	if (stopping(stream) && ts != 0)
		return;

	if (connecting(stream))
		pthread_join(stream->connect_thread, NULL);

	stream->stop_ts = ts / 1000ULL;

	if (ts)
		stream->shutdown_timeout_ts = ts +
		(uint64_t)stream->max_shutdown_time_sec * 1000000000ULL;

	if (active(stream)) {
		os_event_signal(stream->stop_event);
		if (stream->stop_ts == 0)
			os_sem_post(stream->send_sem);
	}
	else {
		obs_output_signal_stop(stream->output, OBS_OUTPUT_SUCCESS);
	}
}


static void drop_frames(struct flv_stream *stream, const char *name,
	int highest_priority, bool pframes)
{
	UNUSED_PARAMETER(pframes);

	struct circlebuf new_buf = { 0 };
	int              num_frames_dropped = 0;

#ifdef _DEBUG
	int start_packets = (int)num_buffered_packets(stream);
#else
	UNUSED_PARAMETER(name);
#endif

	circlebuf_reserve(&new_buf, sizeof(struct encoder_packet) * 8);

	while (stream->packets.size) {
		struct encoder_packet packet;
		circlebuf_pop_front(&stream->packets, &packet, sizeof(packet));

		/* do not drop audio data or video keyframes */
		if (packet.type == OBS_ENCODER_AUDIO ||
			packet.drop_priority >= highest_priority) {
			circlebuf_push_back(&new_buf, &packet, sizeof(packet));

		}
		else {
			num_frames_dropped++;
			obs_encoder_packet_release(&packet);
		}
	}

	circlebuf_free(&stream->packets);
	stream->packets = new_buf;

	if (stream->min_priority < highest_priority)
		stream->min_priority = highest_priority;
	if (!num_frames_dropped)
		return;

	stream->dropped_frames += num_frames_dropped;
#ifdef _DEBUG
	debug("Dropped %s, prev packet count: %d, new packet count: %d",
		name,
		start_packets,
		(int)num_buffered_packets(stream));
#endif
}

static bool find_first_video_packet(struct flv_stream *stream,
	struct encoder_packet *first)
{
	size_t count = stream->packets.size / sizeof(*first);

	for (size_t i = 0; i < count; i++) {
		struct encoder_packet *cur = circlebuf_data(&stream->packets,
			i * sizeof(*first));
		if (cur->type == OBS_ENCODER_VIDEO && !cur->keyframe) {
			*first = *cur;
			return true;
		}
	}

	return false;
}

static void check_to_drop_frames(struct flv_stream *stream, bool pframes)
{
	struct encoder_packet first;
	int64_t buffer_duration_usec;
	size_t num_packets = num_buffered_packets(stream);
	const char *name = pframes ? "p-frames" : "b-frames";
	int priority = pframes ?
		OBS_NAL_PRIORITY_HIGHEST : OBS_NAL_PRIORITY_HIGH;
	int64_t drop_threshold = pframes ?
		stream->pframe_drop_threshold_usec :
		stream->drop_threshold_usec;

	if (num_packets < 5) {
		if (!pframes)
			stream->congestion = 0.0f;
		return;
	}

	if (!find_first_video_packet(stream, &first))
		return;

	/* if the amount of time stored in the buffered packets waiting to be
	* sent is higher than threshold, drop frames */
	buffer_duration_usec = stream->last_dts_usec - first.dts_usec;

	if (!pframes) {
		stream->congestion = (float)buffer_duration_usec /
			(float)drop_threshold;
	}

	if (buffer_duration_usec > drop_threshold) {
		debug("buffer_duration_usec: %" PRId64, buffer_duration_usec);
		drop_frames(stream, name, priority, pframes);
	}
}
//double	GetTickTimer();
static inline bool add_packet(struct flv_stream *stream,
	struct encoder_packet *packet)
{
	circlebuf_push_back(&stream->packets, packet,
		sizeof(struct encoder_packet));
	return true;
}


static bool add_video_packet(struct flv_stream *stream,
	struct encoder_packet *packet)
{
	check_to_drop_frames(stream, false);
	check_to_drop_frames(stream, true);

	/* if currently dropping frames, drop packets until it reaches the
	* desired priority */
	if (packet->drop_priority < stream->min_priority) {
		stream->dropped_frames++;
		return false;
	}
	else {
		stream->min_priority = 0;
	}

	stream->last_dts_usec = packet->dts_usec;
	return add_packet(stream, packet);
}


static void hengxin_flv_stream_data(void *data, struct encoder_packet *packet)
{
	struct flv_stream    *stream = data;
	struct encoder_packet new_packet;
	bool                  added_packet = false;

	if (disconnected(stream) || !active(stream))
		return;

	if (packet->type == OBS_ENCODER_VIDEO) {
		if (!stream->got_first_video) {
			stream->start_dts_offset =
				get_ms_time(packet, packet->dts);
			stream->got_first_video = true;
		}

		obs_parse_avc_packet(&new_packet, packet);
	}
	else {
		obs_encoder_packet_ref(&new_packet, packet);
	}

	pthread_mutex_lock(&stream->packets_mutex);

	if (!disconnected(stream)) {
		added_packet = (packet->type == OBS_ENCODER_VIDEO) ?
			add_video_packet(stream, &new_packet) :
			add_packet(stream, &new_packet);
	}

	pthread_mutex_unlock(&stream->packets_mutex);

	if (added_packet)
		os_sem_post(stream->send_sem);
	else
		obs_encoder_packet_release(&new_packet);
}

struct obs_output_info hengxin_flv_output_info = {
	.id = "hengxin_flv_output",
	.flags = OBS_OUTPUT_AV |
	OBS_OUTPUT_ENCODED |
	//OBS_OUTPUT_SERVICE |
	OBS_OUTPUT_MULTI_TRACK,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "aac",
	.get_name = hengxin_flv_stream_getname,
	.create = hengxin_flv_stream_create,
	.destroy = hengxin_flv_stream_destroy,
	.start = hengxin_flv_stream_start,
	.stop = hengxin_flv_stream_stop,
	.encoded_packet = hengxin_flv_stream_data
};
