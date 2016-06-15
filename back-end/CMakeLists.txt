cmake_minimum_required(VERSION 2.8.7)

project(back-end)

include(ExternalProject)

ExternalProject_Add(
	klee-qemu-1.0
	DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
	URL ${CMAKE_CURRENT_SOURCE_DIR}/klee
	UPDATE_COMMAND echo CXX.Flags+=-DCRETE_QEMU10=1 >> ${CMAKE_CURRENT_BINARY_DIR}/klee-qemu-1.0/Makefile.common
	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/klee-qemu-1.0
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/klee-qemu-1.0/configure --with-llvm=${CMAKE_BINARY_DIR}/lib/llvm/llvm-3.2-prefix/src/llvm-3.2 --with-stp=${CMAKE_BINARY_DIR}/bin/stp/stp-r940
	BUILD_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/klee-qemu-1.0/crete_make_klee.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
	INSTALL_COMMAND ln -sf ${CMAKE_CURRENT_BINARY_DIR}/klee-qemu-1.0/Release+Asserts/bin/klee ${CMAKE_BINARY_DIR}/bin/crete-klee-qemu-1.0
)

add_dependencies(klee-qemu-1.0 llvm-3.2 stp-r940 crete_test_case qemu-1.0)

add_subdirectory(manager)
add_subdirectory(llvm-translator)
