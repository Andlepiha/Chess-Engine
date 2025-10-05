add_executable(chess
	WIN32
	main.cpp
	src/draw_board.cpp
	src/game.cpp
	${WIN32_RESOURCES}
)

#set_target_properties(chess PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
#set_target_properties(chess PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")

#Create a symlink in app directory for easier access
# add_custom_command(TARGET chess POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E create_symlink
#     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/chess.exe
#     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/../chess.exe
#     COMMENT "Creating symbolic link to chess.exe"
# )

find_package(NanoSVG 1.0 CONFIG REQUIRED)
target_link_libraries(chess PRIVATE NanoSVG::nanosvg NanoSVG::nanosvgrast)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(chess PRIVATE spdlog::spdlog)

#Include SFML
set(SFML_STATIC_LIBRARIES TRUE)
find_package(SFML 2.6.0
	COMPONENTS
	system window graphics audio
	CONFIG
	REQUIRED
)

target_compile_features(chess PRIVATE cxx_std_20)
target_compile_definitions(chess PRIVATE SFML_DYNAMIC)
target_link_libraries(chess PRIVATE sfml-window sfml-graphics sfml-system sfml-audio)

#Move generation library
target_link_libraries(chess PRIVATE MoveGeneration)
target_include_directories(chess PRIVATE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/include")

find_package(Boost 1.79.0 COMPONENTS process CONFIG REQUIRED)
target_link_libraries(chess PRIVATE Boost::process)
