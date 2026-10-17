add_executable(PortableCoreCheck hashes.cpp)
target_link_libraries(PortableCoreCheck PRIVATE WowPortableCore)
add_test(NAME portable-core-hashes COMMAND PortableCoreCheck)
set_tests_properties(portable-core-hashes PROPERTIES TIMEOUT 30)
