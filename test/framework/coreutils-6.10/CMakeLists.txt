cmake_minimum_required(VERSION 2.8.7)

project(coreutils-6.10)

include(ExternalProject)

ExternalProject_Add(
	coreutils-6.10
	URL http://ftp.gnu.org/gnu/coreutils/coreutils-6.10.tar.gz
	SOURCE_DIR coreutils-6.10
	BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/coreutils-6.10/configure CFLAGS=-O0
	BUILD_COMMAND make -j7
	INSTALL_COMMAND ""
)
