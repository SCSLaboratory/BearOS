
set(UTILS_DIR ${BEAR_SOURCE_DIR}/sys/utils)
set(USR_SRC_DIR ${BEAR_SOURCE_DIR}/usr/src)
set(USR_SBIN_DIR ${BEAR_SOURCE_DIR}/usr/sbin)


include_directories (
		    ${BEAR_SOURCE_DIR}/sys/include
		    ${BEAR_SOURCE_DIR}/usr/include
)

add_subdirectory(utils)
add_subdirectory(boot)
add_subdirectory(hypv)
add_subdirectory(kernel)
