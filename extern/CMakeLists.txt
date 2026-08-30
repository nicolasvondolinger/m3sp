# Vendored third-party dependencies.
#
# Both are header-only, so they are INTERFACE targets: nothing is compiled here,
# the include directory is simply propagated to whoever links them. They are
# marked SYSTEM so that M3SP_ENABLE_WARNINGS does not report on vendor code.

# Mersenne Twister PRNG (Richard J. Wagner, after Matsumoto & Nishimura).
add_library(mtrand INTERFACE)
target_include_directories(mtrand SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/mtrand")

# Biased Random-Key Genetic Algorithm framework (Rodrigo Franco Toso).
# brkga.h drives decoding with OpenMP, so it carries the OpenMP requirement.
add_library(brkga INTERFACE)
target_include_directories(brkga SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/brkga")
# This copy is not pristine: it has been adapted to consult the m3sp globals
# `rng`, `evaluations` and `applyEliteLS`, so it needs both MTRand.h and the
# project's own include/ directory (supplied by m3sp_core, which every consumer
# of brkga also links).
target_link_libraries(brkga INTERFACE mtrand OpenMP::OpenMP_CXX)
