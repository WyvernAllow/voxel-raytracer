cmake_minimum_required(VERSION 3.8)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("voxel-raytracer" VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory(deps/glm)
add_subdirectory(deps/spdlog)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME}
	"src/main.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	glm::glm
	spdlog::spdlog
	Vulkan::Vulkan
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

set(SHADER_SOURCES
	"res/shaders/main.vert"
	"res/shaders/main.frag"
)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/res/shaders")

file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND glslc ${CMAKE_SOURCE_DIR}/${SHADER} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER} to ${SHADER_OUTPUT}"
        VERBATIM
    )

    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(${PROJECT_NAME} compile_shaders)