mkdir -p build;
cd build ;
rm CMakeCache.txt
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..;
cmake .. -DCMAKE_BUILD_TYPE=DEBUG;
make -j6;
