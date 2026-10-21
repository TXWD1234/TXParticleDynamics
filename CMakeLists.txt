cmake_minimum_required(VERSION 4.2)

set(libs "${CMAKE_SOURCE_DIR}/libs")
set(src "${CMAKE_SOURCE_DIR}/src")

project(TXProject)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON)

# install
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix to ~/.local" FORCE)
endif()

# Dependencies

# TXLib
include("${libs}/TXLib/add_txlib.cmake")
tx_add_txlib(
	VERSION "refactor/lib-structure"
	LOCAL_DIR /home/TX_Jerry/Programming/dev/libs/TXLib
	BUILD_DIR ./build
)

# Lua and sol2
find_package(Lua 5.4 EXACT REQUIRED)
include("${TXLib_SOURCE_DIR}/cmake/add_lib.cmake")
tx_add_lib(
	LIB_NAME sol2
	LOCAL_DIR /home/TX_Jerry/Programming/dev/libs/sol2
	BIN_DIR ./build
	VERSION v3.3.0
	REPOSITORY https://github.com/ThePhD/sol2.git
)

# Project

add_executable(TXProject 
	"${src}/main.cpp"
	"${src}/Project.hpp"
	"${src}/Application.hpp"
	"${src}/lua_application.hpp"
	"${src}/lua_application.cpp"
	"${src}/infrastructure.hpp"
	"${src}/render_engine.hpp"
)
target_include_directories(TXProject PRIVATE 
	"${CMAKE_SOURCE_DIR}"
	"${libs}"
	"${src}"
	${LUA_INCLUDE_DIR}
)

# project settings
target_compile_features(TXProject PUBLIC cxx_std_23)

# dependencies
target_link_libraries(TXProject PRIVATE
	TXLib
	${LUA_LIBRARIES}
	sol2::sol2
)

# optimization
include("${TXLib_SOURCE_DIR}/cmake/set_compile_flags.cmake")
tx_set_compile_flags(TXProject PRIVATE)

# build
set_target_properties(TXProject PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# executable environment
include("${TXLib_SOURCE_DIR}/cmake/link_dir_to_bin.cmake")
tx_link_dir_to_bin(TXProject "${CMAKE_SOURCE_DIR}/assets")
tx_link_dir_to_bin(TXProject "${CMAKE_SOURCE_DIR}/demo")
tx_link_dir_to_bin(TXProject "${CMAKE_SOURCE_DIR}/config")

install(TARGETS TXProject
    RUNTIME DESTINATION bin
)