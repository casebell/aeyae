# - Try to find FFTW
# Once done this will define
#  
#  FFTW_FOUND        - system has FFTW
#  FFTW_INCLUDE_DIR  - the FFTW include directory
#  FFTW_LIBRARIES    - multi/single-threaded double/single precision libraries
#  FFTW3_LIBRARIES   - multi/single-threaded double precision libraries
#  FFTW3F_LIBRARIES  - multi/single-threaded single precision libraries


FILE(TO_CMAKE_PATH "$ENV{FFTW_DIR}" FFTW_DIR)

FIND_LIBRARY(FFTW3_LIBRARY fftw3
  ${FFTW_DIR}/lib 
  DOC "FFTW3 double precision library"
)

FIND_LIBRARY(FFTW3_THREADS_LIBRARY fftw3_threads
  ${FFTW_DIR}/lib 
  DOC "FFTW3 double precision multi-threading library"
)

FIND_LIBRARY(FFTW3F_LIBRARY fftw3f
  ${FFTW_DIR}/lib 
  DOC "FFTW3 single precision library"
)

FIND_LIBRARY(FFTW3F_THREADS_LIBRARY fftw3f_threads
  ${FFTW_DIR}/lib 
  DOC "FFTW3 single precision multi-threading library"
)

FIND_PATH(FFTW_INCLUDE_DIR fftw3.h 
  ${FFTW_DIR}/include 
  DOC "Include for FFTW"
)

SET( FFTW_FOUND "NO" )
IF(FFTW_INCLUDE_DIR)
  IF(FFTW3_LIBRARY AND FFTW3_THREADS_LIBRARY)
    SET(FFTW3_LIBRARIES ${FFTW3_THREADS_LIBRARY} ${FFTW3_LIBRARY})
    SET(FFTW_LIBRARIES ${FFTW3_LIBRARIES} ${FFTW_LIBRARIES})
  ENDIF(FFTW3_LIBRARY AND FFTW3_THREADS_LIBRARY)
  
  IF(FFTW3F_LIBRARY AND FFTW3F_THREADS_LIBRARY)
    SET(FFTW3F_LIBRARIES  ${FFTW3F_THREADS_LIBRARY} ${FFTW3F_LIBRARY})
    SET(FFTW_LIBRARIES ${FFTW3F_LIBRARIES} ${FFTW_LIBRARIES})
  ENDIF(FFTW3F_LIBRARY AND FFTW3F_THREADS_LIBRARY)
  
  IF(FFTW3_LIBRARIES OR FFTW3F_LIBRARIES)
    SET(FFTW_FOUND "YES")
  ENDIF(FFTW3_LIBRARIES OR FFTW3F_LIBRARIES)
ENDIF(FFTW_INCLUDE_DIR)
