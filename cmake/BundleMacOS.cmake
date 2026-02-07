if (NOT APPLE)
  return()
endif()

string(REPLACE "\\ " " " APP_BUNDLE "${APP_BUNDLE}")
string(REPLACE "\\ " " " APP_BIN "${APP_BIN}")
string(REPLACE "\\ " " " VULKAN_LIB "${VULKAN_LIB}")
string(REPLACE "\\ " " " MOLTENVK_LIB "${MOLTENVK_LIB}")

if (NOT DEFINED APP_BUNDLE OR NOT EXISTS "${APP_BUNDLE}")
  message(FATAL_ERROR "APP_BUNDLE is not set or does not exist: ${APP_BUNDLE}")
endif()
if (NOT DEFINED APP_BIN OR NOT EXISTS "${APP_BIN}")
  message(FATAL_ERROR "APP_BIN is not set or does not exist: ${APP_BIN}")
endif()
if (NOT DEFINED VULKAN_LIB OR NOT EXISTS "${VULKAN_LIB}")
  message(FATAL_ERROR "VULKAN_LIB is not set or does not exist: ${VULKAN_LIB}")
endif()
if (NOT DEFINED MOLTENVK_LIB OR NOT EXISTS "${MOLTENVK_LIB}")
  message(FATAL_ERROR "MOLTENVK_LIB is not set or does not exist: ${MOLTENVK_LIB}")
endif()

set(FRAMEWORKS_DIR "${APP_BUNDLE}/Contents/Frameworks")
set(VULKAN_DIR "${APP_BUNDLE}/Contents/Resources/vulkan")
set(ICD_DIR "${VULKAN_DIR}/icd.d")
set(VULKAN_BUNDLED "${FRAMEWORKS_DIR}/libvulkan.1.dylib")
set(MOLTENVK_BUNDLED "${FRAMEWORKS_DIR}/libMoltenVK.dylib")
set(MOLTENVK_ICD "${ICD_DIR}/MoltenVK_icd.json")

file(MAKE_DIRECTORY "${FRAMEWORKS_DIR}")
file(MAKE_DIRECTORY "${ICD_DIR}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy "${VULKAN_LIB}" "${VULKAN_BUNDLED}"
  RESULT_VARIABLE COPY_VULKAN_RESULT
)
if (NOT COPY_VULKAN_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to copy libvulkan.1.dylib into bundle")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy "${MOLTENVK_LIB}" "${MOLTENVK_BUNDLED}"
  RESULT_VARIABLE COPY_MOLTENVK_RESULT
)
if (NOT COPY_MOLTENVK_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to copy libMoltenVK.dylib into bundle")
endif()

execute_process(
  COMMAND /usr/bin/install_name_tool -id "@rpath/libvulkan.1.dylib" "${VULKAN_BUNDLED}"
  RESULT_VARIABLE VULKAN_ID_RESULT
)
if (NOT VULKAN_ID_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to update ID for libvulkan.1.dylib")
endif()

execute_process(
  COMMAND /usr/bin/install_name_tool -id "@rpath/libMoltenVK.dylib" "${MOLTENVK_BUNDLED}"
  RESULT_VARIABLE MOLTENVK_ID_RESULT
)
if (NOT MOLTENVK_ID_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to update ID for libMoltenVK.dylib")
endif()

execute_process(
  COMMAND /usr/bin/otool -L "${APP_BIN}"
  OUTPUT_VARIABLE APP_DEPS
  RESULT_VARIABLE OTOOL_RESULT
)
if (NOT OTOOL_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to inspect app dependencies")
endif()

string(FIND "${APP_DEPS}" "@rpath/libvulkan.1.dylib" HAS_RPATH_VULKAN)
if (NOT HAS_RPATH_VULKAN EQUAL -1)
  execute_process(
    COMMAND /usr/bin/install_name_tool
      -change "@rpath/libvulkan.1.dylib" "@executable_path/../Frameworks/libvulkan.1.dylib"
      "${APP_BIN}"
    RESULT_VARIABLE CHANGE_RESULT
  )
  if (NOT CHANGE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to rewrite app link to bundled libvulkan.1.dylib")
  endif()
endif()

file(WRITE "${MOLTENVK_ICD}" "{\n")
file(APPEND "${MOLTENVK_ICD}" "  \"file_format_version\": \"1.0.0\",\n")
file(APPEND "${MOLTENVK_ICD}" "  \"ICD\": {\n")
file(APPEND "${MOLTENVK_ICD}" "    \"library_path\": \"../../../Frameworks/libMoltenVK.dylib\",\n")
file(APPEND "${MOLTENVK_ICD}" "    \"api_version\": \"1.4.0\",\n")
file(APPEND "${MOLTENVK_ICD}" "    \"is_portability_driver\": true\n")
file(APPEND "${MOLTENVK_ICD}" "  }\n")
file(APPEND "${MOLTENVK_ICD}" "}\n")

execute_process(
  COMMAND /usr/bin/codesign --force --deep --sign - "${APP_BUNDLE}"
  RESULT_VARIABLE CODESIGN_RESULT
)
if (NOT CODESIGN_RESULT EQUAL 0)
  message(FATAL_ERROR "codesign failed for bundle: ${APP_BUNDLE}")
endif()
