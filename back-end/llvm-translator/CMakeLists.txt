cmake_minimum_required(VERSION 2.8.7)

project(translator)

include(ExternalProject)

ExternalProject_Add(
	qemu-1.0-llvm-translator
    DOWNLOAD_COMMAND ""
	UPDATE_COMMAND ""
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/qemu-1.0-llvm-translator
	BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/qemu-1.0-llvm-translator/crete_configure.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
	BUILD_COMMAND make -j7
    INSTALL_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/qemu-1.0-llvm-translator/crete_gen_links.sh ${CMAKE_BINARY_DIR}
	)

add_dependencies(qemu-1.0-llvm-translator llvm-3.2)
