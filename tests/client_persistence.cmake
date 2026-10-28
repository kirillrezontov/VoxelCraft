if(NOT DEFINED CLIENT OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "Client persistence test requires CLIENT and TEST_ROOT")
endif()
file(REMOVE_RECURSE "${TEST_ROOT}")
execute_process(COMMAND "${CLIENT}" --world-dir "${TEST_ROOT}" --seed 42 --min-y -35 --max-y 12
    --radius 1 --frames 12 --require-validation
    RESULT_VARIABLE first OUTPUT_VARIABLE first_out ERROR_VARIABLE first_log TIMEOUT 25)
if(NOT first EQUAL 0 OR NOT first_log MATCHES "Saved world revision 1" OR
    NOT first_log MATCHES "0 errors, 0 warnings")
    message(FATAL_ERROR "New world run failed (${first}): ${first_log}")
endif()
execute_process(COMMAND "${CLIENT}" --world-dir "${TEST_ROOT}" --radius 1 --frames 12 --require-validation
    RESULT_VARIABLE second OUTPUT_VARIABLE second_out ERROR_VARIABLE second_log TIMEOUT 25)
if(NOT second EQUAL 0 OR NOT second_log MATCHES "Loaded world revision 1" OR
    NOT second_log MATCHES "Saved world revision 2" OR NOT second_log MATCHES "Seed 42" OR
    NOT second_log MATCHES "0 errors, 0 warnings")
    message(FATAL_ERROR "Reopen world run failed (${second}): ${second_log}")
endif()
execute_process(COMMAND "${CLIENT}" --world-dir "${TEST_ROOT}" --seed 99 --frames 1
    RESULT_VARIABLE mismatch OUTPUT_VARIABLE mismatch_out ERROR_VARIABLE mismatch_log TIMEOUT 10)
if(mismatch EQUAL 0 OR NOT mismatch_log MATCHES "differs from the saved world")
    message(FATAL_ERROR "Conflicting world settings were not rejected: ${mismatch_log}")
endif()
execute_process(COMMAND "${CLIENT}" --world-dir "${TEST_ROOT}" --radius 1 --frames 1000000
    RESULT_VARIABLE interrupted OUTPUT_VARIABLE interrupted_out ERROR_VARIABLE interrupted_log TIMEOUT 1)
if(interrupted EQUAL 0)
    message(FATAL_ERROR "Crash-recovery phase unexpectedly exited normally")
endif()
execute_process(COMMAND "${CLIENT}" --world-dir "${TEST_ROOT}" --radius 1 --frames 3 --require-validation
    RESULT_VARIABLE recovery OUTPUT_VARIABLE recovery_out ERROR_VARIABLE recovery_log TIMEOUT 20)
if(NOT recovery EQUAL 0 OR NOT recovery_log MATCHES "Loaded world revision 2" OR
    NOT recovery_log MATCHES "Saved world revision 3" OR NOT recovery_log MATCHES "0 errors, 0 warnings")
    message(FATAL_ERROR "Recovery after process termination failed (${recovery}): ${recovery_log}")
endif()
set(survival_root "${TEST_ROOT}-survival")
file(REMOVE_RECURSE "${survival_root}")
execute_process(COMMAND "${CLIENT}" --world-dir "${survival_root}" --survival --radius 1 --frames 12 --require-validation
    RESULT_VARIABLE survival OUTPUT_VARIABLE survival_out ERROR_VARIABLE survival_log TIMEOUT 25)
if(NOT survival EQUAL 0 OR NOT survival_log MATCHES "Mode: survival" OR NOT survival_log MATCHES "0 errors, 0 warnings")
    message(FATAL_ERROR "Survival create failed (${survival}): ${survival_log}")
endif()
execute_process(COMMAND "${CLIENT}" --world-dir "${survival_root}" --radius 1 --frames 12 --require-validation
    RESULT_VARIABLE survival_load OUTPUT_VARIABLE survival_load_out ERROR_VARIABLE survival_load_log TIMEOUT 25)
if(NOT survival_load EQUAL 0 OR NOT survival_load_log MATCHES "Mode: survival" OR NOT survival_load_log MATCHES "Loaded world revision 1")
    message(FATAL_ERROR "Survival mode was not restored (${survival_load}): ${survival_load_log}")
endif()
execute_process(COMMAND "${CLIENT}" --world-dir "${survival_root}" --fly --frames 1
    RESULT_VARIABLE flight OUTPUT_VARIABLE flight_out ERROR_VARIABLE flight_log TIMEOUT 10)
if(flight EQUAL 0 OR NOT flight_log MATCHES "Flight is disabled in survival")
    message(FATAL_ERROR "Saved survival mode permitted flight: ${flight_log}")
endif()
set(legacy_root "${TEST_ROOT}-legacy")
file(REMOVE_RECURSE "${legacy_root}")
execute_process(COMMAND "${CLIENT}" --world-dir "${legacy_root}" --generator-version 1 --radius 1 --frames 6 --require-validation
    RESULT_VARIABLE legacy OUTPUT_VARIABLE legacy_out ERROR_VARIABLE legacy_log TIMEOUT 25)
if(NOT legacy EQUAL 0 OR NOT legacy_log MATCHES "generator version 1")
    message(FATAL_ERROR "Legacy generator world could not be created: ${legacy_log}")
endif()
execute_process(COMMAND "${CLIENT}" --world-dir "${legacy_root}" --radius 1 --frames 6 --require-validation
    RESULT_VARIABLE legacy_load OUTPUT_VARIABLE legacy_load_out ERROR_VARIABLE legacy_load_log TIMEOUT 25)
if(NOT legacy_load EQUAL 0 OR NOT legacy_load_log MATCHES "generator version 1" OR NOT legacy_load_log MATCHES "0 errors, 0 warnings")
    message(FATAL_ERROR "Legacy generator version changed on reopen: ${legacy_load_log}")
endif()
message(STATUS "Client persisted/reloaded seed, bounds, generator, player and survival mode; conflicting seed/flight rejected; crash released lock and retained snapshot")
