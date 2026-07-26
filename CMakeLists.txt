cmake_minimum_required(VERSION 3.21)

project(
	StarfrostWidgets
	LANGUAGES CXX
	VERSION 1.1.0
	DESCRIPTION "Real-time HUD widgets for Starfrost survival needs and Blade & Blunt injuries"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static CRT globally to match vcpkg x64-windows-static triplet
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- CommonLibSSE-NG ----
set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLib tests" FORCE)
# The overlay draws to the flat swap chain, which VR does not present to.
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "No VR support" FORCE)
add_subdirectory(extern/CommonLibSSE-NG CommonLibSSE EXCLUDE_FROM_ALL)

# ---- Dependencies ----
find_package(imgui CONFIG REQUIRED)

# ---- Plugin Sources ----
set(SOURCES
	src/PCH.h
	src/Main.cpp
	src/Settings.h
	src/Settings.cpp
	src/SurvivalData.h
	src/SurvivalData.cpp
	src/Overlay.h
	src/Overlay.cpp
	src/Input.h
	src/Input.cpp
	src/Widgets.h
	src/Widgets.cpp
)

source_group(
	TREE "${CMAKE_CURRENT_SOURCE_DIR}"
	FILES ${SOURCES}
)

# ---- Build DLL ----
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_compile_features(${PROJECT_NAME}
	PRIVATE
		cxx_std_23
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME}
	PRIVATE
		UNICODE
		_UNICODE
		NOMINMAX
		WIN32_LEAN_AND_MEAN
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		CommonLibSSE::CommonLibSSE
		imgui::imgui
		d3d11.lib
		dxgi.lib
)

target_precompile_headers(${PROJECT_NAME}
	PRIVATE
		src/PCH.h
)

# A Release PDB for crash loggers; /OPT:REF /OPT:ICF undo what /DEBUG disables.
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
	target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()

# ---- Output ----
if(DEFINED ENV{SKYRIM_MODS_FOLDER})
	set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/StarfrostWidgets/SKSE/Plugins")
endif()

if(DEFINED OUTPUT_FOLDER)
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${OUTPUT_FOLDER}"
		VERBATIM
	)

	# No config check: every config emits a PDB, and the VS generator is multi-config.
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${OUTPUT_FOLDER}"
		VERBATIM
	)

	# copy_if_different, so a locally edited ini survives a rebuild.
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/SKSE/Plugins/StarfrostWidgets.ini" "${OUTPUT_FOLDER}"
		VERBATIM
	)
endif()
