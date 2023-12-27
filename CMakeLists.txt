cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

project(SDL_Player VERSION 0.0.1 LANGUAGES C CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message("Setting default build type to Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_definitions(__MY_DEBUG__)
endif()

# =======================================================
# include directoies recursivly.
MACRO(HEADER_DIRECTORIES return_list)
    FILE(GLOB_RECURSE new_list *.h)
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        SET(dir_list ${dir_list} ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list})
ENDMACRO()

HEADER_DIRECTORIES(header_dir_list)

list(LENGTH header_dir_list header_dir_list_count)
message(STATUS "[INFO] Found ${header_dir_list_count} header directories.")
# ==========================================================

# include_directories(${PROJECT_SOURCE_DIR}/include/)
include_directories(${header_dir_list})

# add_subdirectory(./src)

add_subdirectory(./test)

add_subdirectory(./example)


# 遍历源文件
file(GLOB_RECURSE CPP_SOURCES "src/*.cpp")
FOREACH(file_path ${CPP_SOURCES})
    message(STATUS "[INFO] Found ${file_path} source files.")
ENDFOREACH()

# 排除 main.cpp 文件
# list(FILTER CPP_SOURCES EXCLUDE REGEX "src/main.cpp")

# 生成可执行文件
add_executable(${PROJECT_NAME} ${CPP_SOURCES})

# =======================================================
find_package(SDL2 REQUIRED)

# 检查是否找到了 SDL2
if(SDL2_FOUND)
    # 包含 SDL2 头文件路径
    include_directories(${SDL2_INCLUDE_DIRS})
    message(STATUS "[INFO] Found ${SDL2_INCLUDE_DIRS} header directories.")
    # 链接 SDL2 库
    # target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif()


set(ffmpeg_libs_DIR /usr/local/ffmpeg/lib)
set(ffmpeg_headers_DIR /usr/local/ffmpeg/include)

#对于find_package找不到的外部依赖库，可以用add_library添加
# SHARED表示添加的是动态库
# IMPORTED表示是引入已经存在的动态库
add_library( avcodec SHARED IMPORTED )
#指定所添加依赖库的导入路径
set_target_properties( avcodec PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavcodec.so )

add_library( avfilter SHARED IMPORTED )
set_target_properties( avfilter PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavfilter.so )

add_library( swresample SHARED IMPORTED )
set_target_properties( swresample PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libswresample.so )

add_library( swscale SHARED IMPORTED )
set_target_properties( swscale PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libswscale.so )

add_library( avformat SHARED IMPORTED )
set_target_properties( avformat PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavformat.so )

add_library( avutil SHARED IMPORTED )
set_target_properties( avutil PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavutil.so )

include_directories( ${ffmpeg_headers_DIR} )
message(STATUS "[INFO] Found ${ffmpeg_headers_DIR} header directories.")

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} avcodec avformat avutil swresample swscale swscale avfilter)


