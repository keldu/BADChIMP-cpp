set(scripts
	geometry_demo.py
	vtklb.py
	vtklbregular.py
	writeGeoFile.py
	writeLatticeFile.py
)

set(script_install_dir "python")

install(FILES ${scripts} DESTINATION ${script_install_dir})
