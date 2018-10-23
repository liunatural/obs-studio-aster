project(hengxin-flv-outputs)

if(MSVC)
	set(hengxin-flv-outputs_PLATFORM_DEPS
		w32-pthreads)
endif()

set(hengxin-flv-outputs_SOURCES
	hengxin-flv-outputs.c
	flv-meta-data.c
	flv-mux-data.c
	flv-stream.c
	flv-output-connect.cpp
	librtmp/log.c
	librtmp/amf.c)
	
set(hengxin-flv-outputs_HEADERS
	obs-output-ver.h
	flv-stream.h
	flv-mux.h)	

add_library(hengxin-flv-outputs MODULE
	${hengxin-flv-outputs_SOURCES}
	${hengxin-flv-outputs_HEADERS})

target_link_libraries(hengxin-flv-outputs
	libobs
	${hengxin-flv-outputs_PLATFORM_DEPS})


install_obs_plugin_with_data(hengxin-flv-outputs data)
