apptainer: build/build.sif
	apptainer run --cleanenv build/build.sif ${PWD}
build/build.sif: build.def
	mkdir -p build/
	rm -f build/build.sif
	apptainer build build/build.sif build.def
clean:
	rm -rf build
