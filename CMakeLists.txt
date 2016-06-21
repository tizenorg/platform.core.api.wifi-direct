CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(wifi-direct C)

#SET(PREFIX $(CMAKE_INSTALL_PREFIX))
SET(prefix "/usr")
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")

SET(dependents "capi-base-common")
SET(pc_dependents "capi-base-common")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC -Wall -Werror -fprofile-arcs -ftest-coverage")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC -Wall -Werror")

ADD_SUBDIRECTORY(src)
#ADD_SUBDIRECTORY(test)
