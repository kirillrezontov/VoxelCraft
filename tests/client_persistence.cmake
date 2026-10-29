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
if(MODS_ROOT)
    set(modded_root "${TEST_ROOT}-modded")
    file(REMOVE_RECURSE "${modded_root}")
    foreach(visit RANGE 1 2)
        execute_process(COMMAND "${CLIENT}" --world-dir "${modded_root}" --mods-dir "${MODS_ROOT}"
            --radius 1 --frames 6 --require-validation
            RESULT_VARIABLE modded OUTPUT_VARIABLE modded_out ERROR_VARIABLE modded_log TIMEOUT 25)
        if(NOT modded EQUAL 0 OR NOT modded_log MATCHES "Loaded 1 mods" OR
            NOT modded_log MATCHES "demo: Glowstone ready - visit ${visit}" OR
            NOT modded_log MATCHES "0 errors, 0 warnings")
            message(FATAL_ERROR "Modded world visit ${visit} failed (${modded}): ${modded_log}")
        endif()
    endforeach()
    execute_process(COMMAND "${CLIENT}" --world-dir "${modded_root}" --no-mods --radius 1 --frames 6 --require-validation
        RESULT_VARIABLE missing OUTPUT_VARIABLE missing_out ERROR_VARIABLE missing_log TIMEOUT 25)
    if(NOT missing EQUAL 0 OR NOT missing_log MATCHES "Missing saved mod demo" OR
        NOT missing_log MATCHES "0 errors, 0 warnings")
        message(FATAL_ERROR "Missing-mod retention failed (${missing}): ${missing_log}")
    endif()
    execute_process(COMMAND "${CLIENT}" --world-dir "${modded_root}" --mods-dir "${MODS_ROOT}"
        --radius 1 --frames 6 --require-validation
        RESULT_VARIABLE restored OUTPUT_VARIABLE restored_out ERROR_VARIABLE restored_log TIMEOUT 25)
    if(NOT restored EQUAL 0 OR NOT restored_log MATCHES "demo: Glowstone ready - visit 3" OR
        NOT restored_log MATCHES "0 errors, 0 warnings")
        message(FATAL_ERROR "Reinstalled mod lost its state (${restored}): ${restored_log}")
    endif()
    set(upgraded_mods "${TEST_ROOT}-mods-upgraded")
    file(REMOVE_RECURSE "${upgraded_mods}")
    file(COPY "${MODS_ROOT}/demo" DESTINATION "${upgraded_mods}")
    file(WRITE "${upgraded_mods}/demo/mod.json" "{\"id\":\"demo\",\"version\":\"2.0.0\",\"api\":1,\"state_version\":2}")
    file(WRITE "${upgraded_mods}/demo/init.lua" [=[
local v=voxelcraft
return {
  on_migrate=function(e)
    assert(e.from_state_version == 1 and e.to_state_version == 2)
    assert(v.set_state("visits_total",v.get_state("visits")))
    assert(v.set_state("visits",nil))
  end,
  on_load=function()
    local total=tonumber(v.get_state("visits_total"))+1
    assert(v.set_state("visits_total",tostring(total)))
    v.message("Upgraded visits "..total)
  end
}
]=])
    execute_process(COMMAND "${CLIENT}" --world-dir "${modded_root}" --mods-dir "${upgraded_mods}"
        --radius 1 --frames 6 --require-validation
        RESULT_VARIABLE upgrade OUTPUT_VARIABLE upgrade_out ERROR_VARIABLE upgrade_log TIMEOUT 25)
    if(NOT upgrade EQUAL 0 OR NOT upgrade_log MATCHES "Migrated mod demo state 1 -> 2" OR
        NOT upgrade_log MATCHES "demo: Upgraded visits 4" OR NOT upgrade_log MATCHES "0 errors, 0 warnings")
        message(FATAL_ERROR "Mod state migration failed (${upgrade}): ${upgrade_log}")
    endif()
    execute_process(COMMAND "${CLIENT}" --world-dir "${modded_root}" --mods-dir "${MODS_ROOT}"
        --radius 1 --frames 6 --require-validation
        RESULT_VARIABLE downgrade OUTPUT_VARIABLE downgrade_out ERROR_VARIABLE downgrade_log TIMEOUT 25)
    if(downgrade EQUAL 0 OR NOT downgrade_log MATCHES "downgrade refused")
        message(FATAL_ERROR "Mod state downgrade was not rejected: ${downgrade_log}")
    endif()
    execute_process(COMMAND "${CLIENT}" --world-dir "${modded_root}" --mods-dir "${upgraded_mods}"
        --radius 1 --frames 6 --require-validation
        RESULT_VARIABLE reopen_upgrade OUTPUT_VARIABLE reopen_upgrade_out ERROR_VARIABLE reopen_upgrade_log TIMEOUT 25)
    if(NOT reopen_upgrade EQUAL 0 OR NOT reopen_upgrade_log MATCHES "demo: Upgraded visits 5" OR
        reopen_upgrade_log MATCHES "Migrated mod" OR NOT reopen_upgrade_log MATCHES "0 errors, 0 warnings")
        message(FATAL_ERROR "Migrated state changed after refused downgrade: ${reopen_upgrade_log}")
    endif()
endif()
message(STATUS "Client persisted/reloaded world, player, survival and mod state; conflicts rejected; crash recovery, missing-mod retention and transactional state migration verified")
