set( headers
	LBSOLVER.h
	IO.h
)

add_library( BdChmp_Core INTERFACE ${headers})
set_target_properties( BdChmp_Core
	PROPERTIES
		CXX_STANDARD 17
		PUBLIC_HEADER "${headers}"
		EXPORT_NAME BdChmp_Core
)

add_subdirectory( lbsolver/ )
add_subdirectory( io/       )
