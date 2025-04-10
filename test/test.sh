# Test Command for this project

## build

cd ~/Projects/ZlibValidation
mkdir build
cd build
cmake .. -G Ninja
ninja
./zlibvalidation --help
./zlibvalidation --version

## mono sub command
cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
./zlibvalidation mono ~/examples/liberate_lv/LIBRARY/example.lib -s

## verilog sub command

cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
./zlibvalidation verilog ../pdk/tcbn65lpbc.lib -c 3 --cells DFQD1 FA1D0 INVD0

## spice sub command

cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
./zlibvalidation spice ../pdk/tcbn65lpbc.lib -c 3 --cells FA1D0 INVD0