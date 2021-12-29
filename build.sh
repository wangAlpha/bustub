mkdir -p build;
fd CMakeCache.txt build|xargs rm
cd build ;
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..;
cmake .. -DCMAKE_BUILD_TYPE=DEBUG;
make -j;
