# m3sp_set_warnings(<target>)
#
# Applies a strict warning set to <target>, but only when M3SP_ENABLE_WARNINGS is
# ON. It defaults to OFF because the existing sources produce a large number of
# -Wsign-compare hits (the loops index containers with `int`), which would bury
# any genuinely new warning. Turn it on when working through that cleanup:
#
#     cmake -B build -DM3SP_ENABLE_WARNINGS=ON
#
# Warnings are never promoted to errors here; add -Werror deliberately if wanted.

function(m3sp_set_warnings target)
    if(NOT M3SP_ENABLE_WARNINGS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endif()
endfunction()
