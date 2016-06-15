cmake_minimum_required(VERSION 2.8.7)

project(front-end)

include(ExternalProject)

ExternalProject_Add(
	qemu-1.0
	DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
	URL ${CMAKE_CURRENT_SOURCE_DIR}/qemu-1.0
	UPDATE_COMMAND ""
	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/qemu-1.0
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/qemu-1.0/crete_configure.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
	BUILD_COMMAND make -j7
	INSTALL_COMMAND ln -sf ${CMAKE_CURRENT_BINARY_DIR}/qemu-1.0/i386-softmmu/qemu-system-i386 ${CMAKE_BINARY_DIR}/bin/crete-qemu-1.0-system-i386 && ln -sf ${CMAKE_CURRENT_BINARY_DIR}/qemu-1.0/runtime-dump/template_main_x86.ll ${CMAKE_BINARY_DIR}/bin/crete-qemu-1.0-template-main-i386.ll && ln -sf ${CMAKE_CURRENT_BINARY_DIR}/qemu-1.0/runtime-dump/template_main_x64.ll ${CMAKE_BINARY_DIR}/bin/crete-qemu-1.0-template-main-x64.ll
	)

add_dependencies(qemu-1.0 crete_test_case llvm-3.2)
