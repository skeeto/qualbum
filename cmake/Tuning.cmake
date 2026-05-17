add_library(qualbum_tuning INTERFACE)

# Release defaults: -O2 (not -O3 — marginal gain, larger code on Pi 4)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(qualbum_tuning INTERFACE
        $<$<CONFIG:Release>:-O2 -fno-plt>
        $<$<CONFIG:RelWithDebInfo>:-O2 -fno-plt -g>
    )
endif()

# ARM tuning. Pi 4 has Cortex-A72 (ARMv8-A, NEON mandatory).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        # Only apply Pi 4 specific tuning when the host actually looks like a Pi.
        # On Apple Silicon and other ARMv8 hosts, fall through to the compiler
        # defaults — the compiler already targets the host CPU.
        if(EXISTS "/proc/device-tree/model")
            file(READ "/proc/device-tree/model" _pi_model)
            if(_pi_model MATCHES "Raspberry Pi 4")
                target_compile_options(qualbum_tuning INTERFACE
                    -mcpu=cortex-a72 -mtune=cortex-a72)
            endif()
        endif()
    endif()
endif()
