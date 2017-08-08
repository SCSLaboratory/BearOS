set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/usr.test.bin)

# build the executable in the test directory

# basic printf test
add_executable(tprinter tprinter.c)

# command line argument test
add_executable(tcmdln tcmdln.c)

# exit options
add_executable(texit texit.c)

# environment test
add_executable(tenv tenv.c)

# scanf test
add_executable(tscanf tscanf.c)

# inf loop to help test job control 
add_executable(tjobs tjobs.c)

# basic malloc test
add_executable(tmalloc tmalloc.c
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
)

# tq1 -- basic operations: qopen/qget/qput/qclose/apply
add_executable(tq1 tq1.c 
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)
# tq2 + but using large structures
add_executable(tq2 tq2.c 
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)
# tq3 + multiple queues & complex operations: qsearch/qremove/qconcat
add_executable(tq3 tq3.c 
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# tq4 -- tests the queue free list
add_executable(tq4 tq4.c 
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# thash -- tests the hash table
add_executable(thash thash.c 
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/hash.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# tshash -- tests the sequenced hash table
add_executable(tshash tshash.c 
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/shash.c
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

add_executable(tnetfns tnetfns.c)

# UDP network test
add_executable(t6 t6.c)

# TCP network test
add_executable(t7 t7.c)

# test kernel refresh
add_executable(trefresh trefresh.c)

# smp core test
add_executable(t12 t12.c)

# cause an exeception
add_executable(t13 t13.c)

# echo test -- just sits around waiting to be killed from the shell
add_executable(echo echo.c)


# daemond -- Generic Daemon Test
add_executable(tdaemond tdaemond.c 
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
)

# piped -- pipe Daemon Test
add_executable(tpiped tpiped.c
  ${BEAR_SOURCE_DIR}/usr/test/tutils.c
)


# tfile -- newlib file operations
add_executable(tfile tfile.c)

# --- bug reports ---
add_executable(tbug tbug.c 
  ${BEAR_SOURCE_DIR}/usr/src/utils/queue.c
)

# environment test
add_executable(tfork tfork.c)

# dot -- a program to fork
add_executable(dot dot.c)

# dot -- a program to fork
add_executable(foo foo.c)

add_executable(talarm talarm.c)

add_executable(aim9 aim9.c)

# Note libsyscall.a cannot be first in the list of libs
target_link_libraries(tprinter ${NEWLIB_LIBS} libpiped_if.a ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tcmdln ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tenv ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tscanf ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tjobs ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tfork ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(texit ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tmalloc ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tq1 ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tq2 ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tq3 ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tq4 ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(thash ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tshash ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tfile ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tdaemond ${NEWLIB_LIBS} libdaemond_if.a ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tpiped ${NEWLIB_LIBS} libpiped_if.a ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tnetfns ${NEWLIB_LIBS} libsocket.a ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(echo ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(t6 ${NEWLIB_LIBS} libsocket.a ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(t7 ${NEWLIB_LIBS} libsocket.a ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(t12 ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(t13 ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(tbug ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(dot ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(foo ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(trefresh ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(talarm ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})
target_link_libraries(aim9 ${NEWLIB_LIBS} ${NEWLIB_LIBS} ${NEWLIB_LIBS})

