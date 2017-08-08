set(USER_FLAGS "${USER_FLAGS} -Wall")
set(CMAKE_C_FLAGS ${USER_FLAGS})

add_executable(shell shell.c jobs.c
  ${BEAR_SOURCE_DIR}/usr/sbin/daemond/daemond_utils.c
  ${BEAR_SOURCE_DIR}/usr/sbin/daemond/daemond_if.c
)

add_executable(slash slash.c)

add_executable(ls ls.c)

add_executable(reboot reboot.c)

add_executable(shutdown shutdown.c)

add_executable(ifconfig ifconfig.c)

add_executable(cp cp.c)

add_executable(mkdir mkdir.c)

add_executable(rmdir rmdir.c)

add_executable(mv mv.c)

add_executable(rm rm.c)

add_executable(touch touch.c)

add_executable(ps ps.c)

target_link_libraries(reboot ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(shutdown ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(ifconfig ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(ls ${NEWLIB_LIBS}  ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(cp ${NEWLIB_LIBS}  ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(mkdir ${NEWLIB_LIBS}  ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(rmdir ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(mv ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(rm ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(touch ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(ps ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(shell ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})

# standalone shell (does not interface with NFSD)
target_link_libraries(slash ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})


