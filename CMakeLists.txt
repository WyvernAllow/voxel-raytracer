cmake_minimum_required(VERSION 3.8)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("voxel-raytracer" VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory(deps/glfw)
add_subdirectory(deps/glm)
add_subdirectory(deps/spdlog)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME}
	"src/main.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	glfw
	glm::glm
	spdlog::spdlog
	Vulkan::Vulkan
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()