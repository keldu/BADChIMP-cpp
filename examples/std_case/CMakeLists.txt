set( MAINSRC
	main.cpp
)

option(BDCHMP_EX_STD "Build example std_case" ON)

set(executable bdchmp_std )
add_executable(${executable} ${MAINSRC})
target_include_directories(${executable}
	PUBLIC
	"${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(${executable} lbsolver io ${MPI_LIBRARIES})

install(
	TARGETS ${executable}
	EXPORT "${PROJECT_NAME}TARGETS"
)
