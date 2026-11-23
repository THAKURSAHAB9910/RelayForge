add_executable(test_storage test_storage.cpp)
target_link_libraries(test_storage PRIVATE relayforge_core GTest::gtest GTest::gtest_main)
add_test(NAME test_storage COMMAND test_storage)

add_executable(test_scheduler test_scheduler.cpp)
target_link_libraries(test_scheduler PRIVATE relayforge_core GTest::gtest GTest::gtest_main)
add_test(NAME test_scheduler COMMAND test_scheduler)

add_executable(test_worker_pool test_worker_pool.cpp)
target_link_libraries(test_worker_pool PRIVATE relayforge_core GTest::gtest GTest::gtest_main)
add_test(NAME test_worker_pool COMMAND test_worker_pool)

add_executable(test_crash_recovery test_crash_recovery.cpp)
target_link_libraries(test_crash_recovery PRIVATE relayforge_core GTest::gtest GTest::gtest_main)
add_test(NAME test_crash_recovery COMMAND test_crash_recovery)

add_executable(test_full_engine test_full_engine.cpp)
target_link_libraries(test_full_engine PRIVATE relayforge_core GTest::gtest GTest::gtest_main)
add_test(NAME test_full_engine COMMAND test_full_engine)
