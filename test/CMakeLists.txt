add_executable(test_client test_client.cpp)
target_include_directories(test_client PRIVATE ../src)

target_link_libraries(test_client
    PRIVATE
    ${Boost_LIBRARIES}
)
