foreach(kind null data heap)
    execute_process(COMMAND "${EXECUTABLE}" --reject-${kind}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result STREQUAL "0" OR NOT "${output}${error}" MATCHES "Invalid function pointer")
        message(FATAL_ERROR "${kind} callback did not fail with the required diagnostic: ${result}: ${output}${error}")
    endif()
endforeach()
