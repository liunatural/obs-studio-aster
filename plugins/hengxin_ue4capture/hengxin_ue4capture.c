#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>

#define blog(log_level, format, ...) \
	blog(log_level, "[image_source: '%s'] " format, \
			obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) \
	blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) \
	blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) \
	blog(LOG_WARNING, format, ##__VA_ARGS__)

int			image_connect(const char* ip, const char* port);
void		image_disconnect(int netid);
bool		image_readbuffer(int netid, int* width, int* height);
void		image_fillbuffer(int netid, char* bits, int pitch);
void		output_text(const char* fps);

struct gs_ue4_image{
	gs_texture_t *texture;
	enum gs_color_format format;
	uint32_t cx;
	uint32_t cy;
	bool frame_updated;
	bool loaded;
	uint8_t *texture_data;
};
typedef struct gs_ue4_image     gs_ue4_image_t;

struct image_source {
	obs_source_t *source;
	bool        persistent;
	time_t      file_timestamp;
	float       update_time_elapsed;
	int			fps;
	uint64_t    last_time;
	bool        active;
	char		IP[32];
	char		Port[16];
	gs_ue4_image_t image;
	int			netid;
};


static time_t get_modified_timestamp(const char *filename)
{
	struct stat stats;
	if (os_stat(filename, &stats) != 0)
		return -1;
	return stats.st_mtime;
}

static const char *hengxin_ue4capture_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ImageInput");
}

//static void hengxin_ue4capture_unload(struct image_source *context)
//{
//	obs_enter_graphics();
//	gs_texture_destroy(image->texture);
//	//gs_image_file_free(&context->image);
//	obs_leave_graphics();
//}

static void hengxin_ue4capture_update(void *data, obs_data_t *settings)
{
	struct image_source *context = data;
	obs_enter_graphics();
	const char *IP = obs_data_get_string(settings, "IP");
	const char *Port = obs_data_get_string(settings, "Port");

	memset(context->IP, 0, sizeof(context->IP));
	memset(context->Port, 0, sizeof(context->Port));

	bool bReconnect = false;
	if (!(stricmp(context->IP, IP) == 0 && stricmp(context->Port, Port) == 0) || context->netid == 0) {
		bReconnect = true;
	}

	if (bReconnect) 
	{
		if (context->netid > 0)
		{
			image_disconnect(context->netid);
		}
		if (IP && Port)
		{
			strncpy(context->IP, IP, 31);
			strncpy(context->Port, Port, 15);
			context->netid = image_connect(context->IP, context->Port);
		}
	}

	if (!context->image.texture)
	{
		context->image.cx = 1024;
		context->image.cy = 1024;
		context->image.format = GS_BGRA;
		context->image.frame_updated = false;
		context->image.loaded = true;
		context->image.texture_data = (uint8_t*)bmalloc(context->image.cx*context->image.cy * 4);
		for (int i = 0; i < context->image.cx*context->image.cy * 4; i++) {
			context->image.texture_data[i] = rand()%255;
		}
		context->image.texture = gs_texture_create(context->image.cx, context->image.cy, GS_BGRA, 1, (uint8_t**)&context->image.texture_data, GS_DYNAMIC);
		//gs_texture_set_image(context->image.texture, context->image.texture_data, context->image.cx * 4, false);
	}
	
	obs_leave_graphics();
	
}

static void hengxin_ue4capture_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "unload", false);
	obs_data_set_default_string(settings, "IP", "127.0.0.1");
	obs_data_set_default_string(settings, "Port", "12345");
}

static void hengxin_ue4capture_show(void *data)
{
	struct image_source *context = data;
}

static void hengxin_ue4capture_hide(void *data)
{
	struct image_source *context = data;
}

static obs_properties_t *hengxin_ue4capture_properties(void *data)
{
	struct image_source *s = data;
	struct dstr path = { 0 };

	obs_properties_t *props = obs_properties_create();
	obs_properties_add_text(props, "IP", "ue4 rendering server IP", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "Port", "ue4 rendering server port", OBS_TEXT_DEFAULT);
	//dstr_free(&path);

	return props;
}




static void hengxin_ue4capture_destroy(void *data)
{
	struct image_source *context = data;
	if (context->image.texture) {
		gs_texture_destroy(context->image.texture);
	}
	if (context->image.texture_data)
		bfree(context->image.texture_data);
	if (context->netid > 0)
		image_disconnect(context->netid);
	bfree(context);
}

static uint32_t hengxin_ue4capture_getwidth(void *data)
{
	struct image_source *context = data;
	return context->image.cx;
}

static uint32_t hengxin_ue4capture_getheight(void *data)
{
	struct image_source *context = data;
	return context->image.cy;
}

static void hengxin_ue4capture_render(void *data, gs_effect_t *effect)
{
	struct image_source *context = data;
	if (!context->image.texture)
		return;
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->image.texture);
	//gs_enable_color(true, true, true, true);
	gs_draw_sprite(context->image.texture, 0, context->image.cx, context->image.cy);
}
double		m_beginTime = 0;
int			m_numRender = 0;

static void hengxin_ue4capture_tick(void *data, float seconds)
{
	struct image_source *context = data;
	uint64_t frame_time = obs_get_video_frame_time();
	if (context->netid > 0) {
		int width = 0, height = 0;
		//for (int i = 0; i < context->image.cx*context->image.cy * 4; i++) {
		//	context->image.texture_data[i] = rand() % 255;
		//}
		//obs_enter_graphics();
		//gs_texture_set_image(context->image.texture, context->image.texture_data, context->image.cx * 4, false);
		//obs_leave_graphics();
		if (image_readbuffer(context->netid, &width, &height)) {
			if (context->image.cx != width || context->image.cy != height) {
				if (context->image.texture) {
					obs_enter_graphics();
					gs_texture_destroy(context->image.texture);
					obs_leave_graphics();
				}
				context->image.texture = 0;
				context->image.cx = width;
				context->image.cy = height;
			}
			if (context->image.texture == 0) {
				obs_enter_graphics();
				context->image.texture = gs_texture_create(context->image.cx, context->image.cy, GS_BGRA, 1, 0, GS_DYNAMIC);
				obs_leave_graphics();
			}
			obs_enter_graphics();
			uint8_t *ptr;
			uint32_t linesize_out;
			if (gs_texture_map(context->image.texture, &ptr, &linesize_out))
			{
				image_fillbuffer(context->netid, (char*)ptr, linesize_out);
				for (int y = 0; y < height; y++) {
					for (int x = 0; x < linesize_out; x += 4) {
						ptr[y*linesize_out + x + 3] = 255;
					}
				}
				gs_texture_unmap(context->image.texture);
			}
			obs_leave_graphics();
		}
	}
	context->update_time_elapsed += seconds;
	context->fps = (int)(1.0f / seconds);
	//char fps[128];
	//sprintf(fps, "%d", context->fps);
	//output_text(fps);
	if (context->update_time_elapsed >= 1.0f) {
		context->update_time_elapsed = 0.0f;
	}

	if (obs_source_active(context->source)) {
		if (!context->active) {
			//if (context->image.is_animated_gif)
			//	context->last_time = frame_time;
			context->active = true;
		}

	} else {
		if (context->active) {
			//if (context->image.is_animated_gif) {
			//	context->image.cur_frame = 0;
			//	context->image.cur_loop = 0;
			//	context->image.cur_time = 0;

			//	obs_enter_graphics();
			//	gs_image_file_update_texture(&context->image);
			//	obs_leave_graphics();
			//}

			context->active = false;
		}

		return;
	}

	//if (context->last_time && context->image.is_animated_gif) {
	//	uint64_t elapsed = frame_time - context->last_time;
	//	bool updated = gs_image_file_tick(&context->image, elapsed);

	//	if (updated) {
	//		obs_enter_graphics();
	//		gs_image_file_update_texture(&context->image);
	//		obs_leave_graphics();
	//	}
	//}

	context->last_time = frame_time;
}


static void *hengxin_ue4capture_create(obs_data_t *settings, obs_source_t *source)
{
	struct image_source *context = bzalloc(sizeof(struct image_source));
	memset(context, 0, sizeof(struct image_source));
	context->source = source;
	context->netid = 0;
	
	hengxin_ue4capture_update(context, settings);
	return context;
}


static const char *image_filter =
	"All formats (*.bmp *.tga *.png *.jpeg *.jpg *.gif);;"
	"BMP Files (*.bmp);;"
	"Targa Files (*.tga);;"
	"PNG Files (*.png);;"
	"JPEG Files (*.jpeg *.jpg);;"
	"GIF Files (*.gif)";



static struct obs_source_info hengxin_ue4capture_info = {
	.id             = "hengxin_ue4capture",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_VIDEO,
	.get_name       = hengxin_ue4capture_get_name,
	.create         = hengxin_ue4capture_create,
	.destroy        = hengxin_ue4capture_destroy,
	.update         = hengxin_ue4capture_update,
	.get_defaults   = hengxin_ue4capture_defaults,
	.show           = hengxin_ue4capture_show,
	.hide           = hengxin_ue4capture_hide,
	.get_width      = hengxin_ue4capture_getwidth,
	.get_height     = hengxin_ue4capture_getheight,
	.video_render   = hengxin_ue4capture_render,
	.video_tick     = hengxin_ue4capture_tick,
	.get_properties = hengxin_ue4capture_properties
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("hengxin_ue4capture", "en-US")

extern struct obs_source_info slideshow_info;

bool obs_module_load(void)
{
	obs_register_source(&hengxin_ue4capture_info);
	obs_register_source(&slideshow_info);
	return true;
}
