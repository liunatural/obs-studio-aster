project(image-source)

if(MSVC)
	set(hengxin_ue4capture_PLATFORM_DEPS
		w32-pthreads)
endif()

set(hengxin_ue4capture_SOURCES
	hengxin_ue4capture.c
	color-source.c
	obs-slideshow.c
	ImageClient.cpp)
	
set(hengxin_ue4capture_HEADERS
	ImageMessage.hpp
	ImageClient.h)	

add_library(hengxin_ue4capture MODULE
	${hengxin_ue4capture_SOURCES}
	${hengxin_ue4capture_HEADERS})
target_link_libraries(hengxin_ue4capture
	libobs
	${hengxin_ue4capture_PLATFORM_DEPS})

install_obs_plugin_with_data(hengxin_ue4capture data)
