set(MOVE_GENERATION_SOURCE_DIR "${PROJECT_SOURCE_DIR}/lib/")
include_directories("${MOVE_GENERATION_SOURCE_DIR}/include")

add_executable(sliding_pieces_test MoveGenerationTests/sliding_pieces.cpp)
add_executable(perft MoveGenerationTests/perft.cpp)

target_link_libraries(sliding_pieces_test PRIVATE MoveGeneration)
target_link_libraries(perft PRIVATE MoveGeneration)

set_target_properties(sliding_pieces_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/../app/bin/tests"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/../app/bin/tests"
)

set_target_properties(perft
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/../app/bin/tests"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/../app/bin/tests"
)

add_test(NAME movgen.sliding.random COMMAND sliding_pieces_test)

add_test(NAME movgen.perft.start_pos        COMMAND perft 0 5)
add_test(NAME movgen.perft.Kiwipete_pos     COMMAND perft 1 5)
add_test(NAME movgen.perft.pos_3            COMMAND perft 2 5)
add_test(NAME movgen.perft.pos_4            COMMAND perft 3 5)
add_test(NAME movgen.perft.pos_4_flipped    COMMAND perft 4 5)
add_test(NAME movgen.perft.pos_5            COMMAND perft 5 5)
