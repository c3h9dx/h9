file(GLOB DEVS_SOURCES "*.cc" "*.h")

add_library(DevsLib OBJECT ${DEVS_SOURCES})
target_include_directories(DevsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
message(${CMAKE_CURRENT_SOURCE_DIR}/../)
target_link_libraries(DevsLib PUBLIC ${CONFUSE_LIBRARIES} PRIVATE ${ATOMIC_LINK} fmt::fmt nlohmann_json::nlohmann_json spdlog::spdlog Python3::Python)
