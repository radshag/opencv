# ----------------------------------------------------------------------------
#  CMake file for java support
# ----------------------------------------------------------------------------
if(IOS OR NOT PYTHON_EXECUTABLE OR NOT (JNI_FOUND OR (ANDROID AND ANDROID_NATIVE_API_LEVEL GREATER 7)))
  ocv_module_disable(java)
endif()

if(NOT ANDROID)
  # disable java by default because java support on desktop is experimental
  set(BUILD_opencv_java_INIT OFF)
endif()

set(the_description "The java bindings")
ocv_add_module(java BINDINGS opencv_core opencv_imgproc OPTIONAL opencv_objdetect opencv_features2d opencv_video opencv_highgui opencv_ml opencv_calib3d opencv_photo opencv_nonfree opencv_contrib)
ocv_module_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/generator/src/cpp")

if(NOT ANDROID)
  include_directories(${JNI_INCLUDE_DIRS})
endif()

# get list of modules to wrap
string(REPLACE "opencv_" "" OPENCV_JAVA_MODULES "${OPENCV_MODULE_${the_module}_REQ_DEPS};${OPENCV_MODULE_${the_module}_OPT_DEPS}")
foreach(module ${OPENCV_JAVA_MODULES})
  if(NOT HAVE_opencv_${module})
    list(REMOVE_ITEM OPENCV_JAVA_MODULES ${module})
  endif()
endforeach()

set(GEN_JAVA "${CMAKE_CURRENT_SOURCE_DIR}/generator/gen_java.py")
set(HDR_PARSER "${CMAKE_CURRENT_SOURCE_DIR}/../python/src2/hdr_parser.py")
set(GEN_JAVADOC "${CMAKE_CURRENT_SOURCE_DIR}/generator/gen_javadoc.py")
set(RST_PARSER "${CMAKE_CURRENT_SOURCE_DIR}/generator/rst_parser.py")

# add dependencies to cmake (we should rerun cmake if any of these scripts is modified)
configure_file("${GEN_JAVA}" "${OpenCV_BINARY_DIR}/junk/gen_java.junk" COPYONLY)
configure_file("${HDR_PARSER}" "${OpenCV_BINARY_DIR}/junk/hdr_parser.junk" COPYONLY)

set(java_hdr_deps "")
set(generated_cpp_sources "")
set(generated_java_sources "")

# setup raw java and cpp files generation (without javadoc and at temporary location)
foreach(module ${OPENCV_JAVA_MODULES})
  # get list of module headers
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generator/config/${module}.filelist")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/generator/config/${module}.filelist" module_headers)
    ocv_list_add_prefix(module_headers "${OPENCV_MODULE_opencv_${module}_LOCATION}/")
  else()
    set(module_headers "${OPENCV_MODULE_opencv_${module}_HEADERS}")
  endif()

  # C headers must go first
  set(module_headers_cpp ${module_headers})
  ocv_list_filterout(module_headers_cpp "\\\\.h$")
  if(module_headers_cpp)
    list(REMOVE_ITEM module_headers ${module_headers_cpp})
    list(APPEND module_headers ${module_headers_cpp})
  endif()
  unset(module_headers_cpp)

  # add dependencies to cmake (we should rerun cmake if any of these headers is modified)
  foreach(header ${module_headers})
    get_filename_component(header_name "${header}" NAME_WE)
    configure_file("${header}" "${OpenCV_BINARY_DIR}/junk/${header_name}.junk" COPYONLY)
  endforeach()

  # first run (to get list of generated files)
  if(module_headers)
    file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/gen_java_out/")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen_java_out")
    execute_process(COMMAND ${PYTHON_EXECUTABLE} "${GEN_JAVA}" "${HDR_PARSER}" ${module} ${module_headers}
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen_java_out"
                    OUTPUT_QUIET ERROR_QUIET)
    file(GLOB_RECURSE ${module}_generated_java_sources RELATIVE "${CMAKE_CURRENT_BINARY_DIR}/gen_java_out/" "${CMAKE_CURRENT_BINARY_DIR}/gen_java_out/*.java")
    ocv_list_add_prefix(${module}_generated_java_sources "${CMAKE_CURRENT_BINARY_DIR}/")

    # second run (at build time)
    add_custom_command(OUTPUT ${${module}_generated_java_sources} "${CMAKE_CURRENT_BINARY_DIR}/${module}.cpp"
                       COMMAND ${PYTHON_EXECUTABLE} "${GEN_JAVA}" "${HDR_PARSER}" ${module} ${module_headers}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       DEPENDS "${GEN_JAVA}" "${HDR_PARSER}" ${module_headers})

    list(APPEND java_hdr_deps ${module_headers})
    list(APPEND generated_cpp_sources "${CMAKE_CURRENT_BINARY_DIR}/${module}.cpp")
    list(APPEND generated_java_sources ${${module}_generated_java_sources})
  endif()
endforeach()

# get handwritten files used for wrappers generation
file(GLOB handwrittren_h_sources "${CMAKE_CURRENT_SOURCE_DIR}/generator/src/cpp/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/generator/src/cpp/*.hpp")
file(GLOB handwrittren_cpp_sources "${CMAKE_CURRENT_SOURCE_DIR}/generator/src/cpp/*.cpp")
file(GLOB handwrittren_java_sources "${CMAKE_CURRENT_SOURCE_DIR}/generator/src/java/*.java")
file(GLOB handwrittren_aidl_sources  "${CMAKE_CURRENT_SOURCE_DIR}/generator/src/java/*.aidl")

# remove handwritten java files for disabled modules
foreach(jfile ${handwrittren_java_sources})
  string(REGEX REPLACE "^.*/([^+]+)\\+.*\\.java$" "\\1" jmodname "${jfile}")
  if(DEFINED HAVE_opencv_${jmodname} AND NOT HAVE_opencv_${jmodname})
    list(REMOVE_ITEM handwrittren_java_sources "${jfile}")
  endif()
endforeach()

# remove VideoCapture wrapper if highgui is disabled
if(NOT HAVE_opencv_highgui)
  list(REMOVE_ITEM handwrittren_cpp_sources "${CMAKE_CURRENT_SOURCE_DIR}/generator/src/cpp/VideoCapture.cpp")
endif()

# create list of javadoc documented files
unset(documented_java_files)
foreach(java_file ${handwrittren_java_sources} ${generated_java_sources})
  get_filename_component(java_file_name "${java_file}" NAME_WE)
  list(APPEND documented_java_files "${CMAKE_CURRENT_BINARY_DIR}/${java_file_name}-jdoc.java")
endforeach()

# generate javadoc files
file(GLOB_RECURSE refman_rst_headers "${CMAKE_CURRENT_SOURCE_DIR}/../*.rst")
set(java_documented_headers_deps ${handwrittren_java_sources} ${generated_java_sources} ${java_hdr_deps} ${refman_rst_headers}
  "${GEN_JAVADOC}" "${RST_PARSER}" "${GEN_JAVA}" "${HDR_PARSER}")

string(REPLACE ";" "," OPENCV_JAVA_MODULES_STR "${OPENCV_JAVA_MODULES}")
add_custom_command(
    OUTPUT ${documented_java_files}
    COMMAND ${PYTHON_EXECUTABLE} "${GEN_JAVADOC}" --modules ${OPENCV_JAVA_MODULES_STR} "${CMAKE_CURRENT_SOURCE_DIR}/generator/src/java" "${CMAKE_CURRENT_BINARY_DIR}" 2>"${CMAKE_CURRENT_BINARY_DIR}/get_javadoc_errors.log"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${java_documented_headers_deps}
)

# copy generated java files to the final location
set(JAVA_OUTPUT_DIR "src/org/opencv")
set(JAVA_INSTALL_ROOT "sdk/java")
set(JNI_INSTALL_ROOT "sdk/native")

# copy each documented header to the final destination
set(java_files "")
set(source_java_files ${documented_java_files} ${handwrittren_aidl_sources})
if(NOT ANDROID)
  ocv_list_filterout(source_java_files "/(engine|android)\\\\+")
endif()

foreach(java_file ${source_java_files})
  get_filename_component(java_file_name "${java_file}" NAME)
  string(REPLACE "-jdoc.java" ".java" java_file_name "${java_file_name}")
  string(REPLACE "+" "/" java_file_name "${java_file_name}")

  add_custom_command(
        OUTPUT "${OpenCV_BINARY_DIR}/${JAVA_OUTPUT_DIR}/${java_file_name}"
        COMMAND ${CMAKE_COMMAND} -E copy "${java_file}" "${OpenCV_BINARY_DIR}/${JAVA_OUTPUT_DIR}/${java_file_name}"
        MAIN_DEPENDENCY "${java_file}"
        DEPENDS ${java_documented_headers_deps}
        COMMENT "Generating ${JAVA_OUTPUT_DIR}/${java_file_name}"
        )
  list(APPEND java_files "${OpenCV_BINARY_DIR}/${JAVA_OUTPUT_DIR}/${java_file_name}")

  if(ANDROID)
    get_filename_component(install_subdir "${java_file_name}" PATH)
    install(FILES "${OpenCV_BINARY_DIR}/${JAVA_OUTPUT_DIR}/${java_file_name}" DESTINATION ${JAVA_INSTALL_ROOT}/${JAVA_OUTPUT_DIR}/${install_subdir} COMPONENT main)
  endif()
endforeach()

# custom target for java API
set(api_target ${the_module}_api)
add_custom_target(${api_target} DEPENDS ${java_files} ${documented_java_files} ${java_documented_headers_deps})

# add opencv_java library
add_library(${the_module} SHARED ${handwrittren_h_sources} ${handwrittren_cpp_sources} ${generated_cpp_sources})
if(BUILD_FAT_JAVA_LIB)
  set(__deps ${OPENCV_MODULE_${the_module}_DEPS} ${OPENCV_MODULES_BUILD})
  list(REMOVE_ITEM __deps ${the_module} opencv_ts)
  ocv_list_unique(__deps)
  set(__extradeps ${__deps})
  ocv_list_filterout(__extradeps "^opencv_")
  if(__extradeps)
    list(REMOVE_ITEM __deps ${__extradeps})
  endif()
  target_link_libraries(${the_module} -Wl,-whole-archive ${__deps} -Wl,-no-whole-archive ${__extradeps} ${OPENCV_LINKER_LIBS})
else()
  target_link_libraries(${the_module} ${OPENCV_MODULE_${the_module}_DEPS} ${OPENCV_LINKER_LIBS})
endif()
add_dependencies(${the_module} ${api_target})

# Additional target properties
set_target_properties(${the_module} PROPERTIES
    OUTPUT_NAME "${the_module}"
    ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
    INSTALL_NAME_DIR ${OPENCV_LIB_INSTALL_PATH}
    LINK_INTERFACE_LIBRARIES ""
    )

install(TARGETS ${the_module}
        LIBRARY DESTINATION ${OPENCV_LIB_INSTALL_PATH} COMPONENT main
        ARCHIVE DESTINATION ${OPENCV_LIB_INSTALL_PATH} COMPONENT main)

set(lib_target ${the_module}_library)
if(ANDROID)
  target_link_libraries(${the_module} jnigraphics) # for Mat <=> Bitmap converters

  # force strip library after the build command
  # because samples and tests will make a copy of the library before install
  get_target_property(__opencv_java_location ${the_module} LOCATION)
  # Turn off stripping in debug build
  if ( NOT (CMAKE_BUILD_TYPE MATCHES "Debug"))
    add_custom_command(TARGET ${the_module} POST_BUILD COMMAND ${CMAKE_STRIP} --strip-unneeded "${__opencv_java_location}")
  endif()

  set(lib_proj_files "")

  # manifest, jni, Eclipse project
  file(GLOB_RECURSE android_lib_project_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/android_lib/" "${CMAKE_CURRENT_SOURCE_DIR}/android_lib/*")
  list(REMOVE_ITEM android_lib_project_files "${ANDROID_MANIFEST_FILE}")
  foreach(f ${android_lib_project_files})
    if(NOT f MATCHES "\\.svn")
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/android_lib/${f}" "${OpenCV_BINARY_DIR}/${f}")
      list(APPEND lib_proj_files "${OpenCV_BINARY_DIR}/${f}")

      if(NOT f MATCHES "jni/.+")
        get_filename_component(install_subdir "${f}" PATH)
        install(FILES "${OpenCV_BINARY_DIR}/${f}" DESTINATION "${JAVA_INSTALL_ROOT}/${install_subdir}" COMPONENT main)
      endif()
    endif()
  endforeach()

  # library project jni sources
  foreach(jni_file ${handwrittren_cpp_sources} ${handwrittren_h_sources} ${generated_cpp_sources})
    get_filename_component(jni_file_name "${jni_file}" NAME)
    add_custom_command(
            OUTPUT "${OpenCV_BINARY_DIR}/jni/${jni_file_name}"
            COMMAND ${CMAKE_COMMAND} -E copy "${jni_file}" "${OpenCV_BINARY_DIR}/jni/${jni_file_name}"
            DEPENDS "${jni_file}" ${java_hdr_deps}
            COMMENT "Generating jni/${jni_file_name}"
            )
    list(APPEND lib_proj_files "${OpenCV_BINARY_DIR}/jni/${jni_file_name}")
  endforeach()

  # create Android library project in build folder
  if(ANDROID_EXECUTABLE)
    set(lib_target_files ${ANDROID_LIB_PROJECT_FILES})
    ocv_list_add_prefix(lib_target_files "${OpenCV_BINARY_DIR}/")

    android_get_compatible_target(lib_target_sdk_target ${ANDROID_NATIVE_API_LEVEL} ${ANDROID_SDK_TARGET})

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/android_lib/${ANDROID_MANIFEST_FILE}"  "${CMAKE_CURRENT_BINARY_DIR}/${ANDROID_MANIFEST_FILE}")

    add_custom_command(
        OUTPUT ${lib_target_files} "${OpenCV_BINARY_DIR}/${ANDROID_MANIFEST_FILE}"
        COMMAND ${CMAKE_COMMAND} -E remove ${lib_target_files}
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${ANDROID_MANIFEST_FILE}" "${OpenCV_BINARY_DIR}/${ANDROID_MANIFEST_FILE}"
        COMMAND ${ANDROID_EXECUTABLE} --silent create lib-project --path \"${OpenCV_BINARY_DIR}\" --target \"${lib_target_sdk_target}\" --name OpenCV --package org.opencv 2>\"${CMAKE_CURRENT_BINARY_DIR}/create_lib_project.log\"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${ANDROID_MANIFEST_FILE}" "${OpenCV_BINARY_DIR}/${ANDROID_MANIFEST_FILE}"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_BINARY_DIR}/${ANDROID_MANIFEST_FILE}"
        DEPENDS ${lib_proj_files}
        COMMENT "Generating OpenCV Android library project. SDK target: ${lib_target_sdk_target}"
        )

    install(FILES "${OpenCV_BINARY_DIR}/${ANDROID_PROJECT_PROPERTIES_FILE}" DESTINATION ${JAVA_INSTALL_ROOT} COMPONENT main)
    install(FILES "${OpenCV_BINARY_DIR}/${ANDROID_MANIFEST_FILE}" DESTINATION ${JAVA_INSTALL_ROOT} COMPONENT main)
    # creating empty 'gen' and 'res' folders
    install(CODE "MAKE_DIRECTORY(\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/sdk/java/gen\")" COMPONENT main)
    install(CODE "MAKE_DIRECTORY(\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/sdk/java/res\")" COMPONENT main)

    if(ANT_EXECUTABLE AND ANDROID_TOOLS_Pkg_Revision GREATER 13)
      # build the library project
      # normally we should do this after a native part, but for a library project we can build the java part first
      add_custom_command(
        OUTPUT "${OpenCV_BINARY_DIR}/bin/classes.jar" "${OpenCV_BINARY_DIR}/bin/.classes.jar.dephelper"
        COMMAND ${ANT_EXECUTABLE} -q -noinput -k debug
        COMMAND ${CMAKE_COMMAND} -E touch "${OpenCV_BINARY_DIR}/bin/.classes.jar.dephelper" # can not rely on classes.jar because different versions of SDK update timestamp at different times
        WORKING_DIRECTORY "${OpenCV_BINARY_DIR}"
        DEPENDS ${lib_proj_files} ${lib_target_files} ${java_files}
        COMMENT "Building OpenCV Android library project"
        )
      #install(FILES "${OpenCV_BINARY_DIR}/bin/classes.jar" "${OpenCV_BINARY_DIR}/bin/jarlist.cache" "${OpenCV_BINARY_DIR}/bin/build.prop" DESTINATION bin COMPONENT main)
      #install(DIRECTORY "${OpenCV_BINARY_DIR}/bin/res" "${OpenCV_BINARY_DIR}/bin/classes" DESTINATION bin COMPONENT main)
      list(APPEND lib_target_files "${OpenCV_BINARY_DIR}/bin/classes.jar")
    endif()

    add_custom_target(${lib_target} SOURCES ${lib_proj_files} ${lib_target_files} "${OpenCV_BINARY_DIR}/${ANDROID_MANIFEST_FILE}")
  endif()
else(ANDROID)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/jar/build.xml" "${OpenCV_BINARY_DIR}/build.xml" IMMEDIATE @ONLY)
  set(JAR_NAME opencv-${OPENCV_VERSION_MAJOR}.${OPENCV_VERSION_MINOR}.${OPENCV_VERSION_PATCH}.jar)

  add_custom_command(
    OUTPUT "${OpenCV_BINARY_DIR}/bin/${JAR_NAME}" "${OpenCV_BINARY_DIR}/bin/.${JAR_NAME}.dephelper"
    COMMAND ${ANT_EXECUTABLE} -q -noinput -k jar
    COMMAND ${CMAKE_COMMAND} -E touch "${OpenCV_BINARY_DIR}/bin/.${JAR_NAME}.dephelper"
    WORKING_DIRECTORY "${OpenCV_BINARY_DIR}"
    DEPENDS "${OpenCV_BINARY_DIR}/build.xml" ${java_files}
    COMMENT "Generating ${JAR_NAME}"
    )

  add_custom_target(${lib_target} SOURCES "${OpenCV_BINARY_DIR}/bin/${JAR_NAME}")
endif(ANDROID)

add_dependencies(${lib_target} ${api_target})
add_dependencies(${the_module} ${lib_target})

#android test project
if(ANDROID AND BUILD_TESTS)
  add_subdirectory(android_test)
endif()
