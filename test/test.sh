# Test Command for this project

## build

cd ~/Projects/ZlibValidation
mkdir build
cd build
cmake .. -G Ninja
ninja
./zlibvalidation --help
./zlibvalidation --version

## parse sub command

cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
./zlibvalidation parse ../pdk/tcbn65lpbc.lib ../pdk/tcbn65lplt.lib ../pdk/tcbn65lpml.lib

## mono sub command

cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
# ./zlibvalidation mono ~/examples/liberate_lv/LIBRARY/example.lib -s
./zlibvalidation mono ../pdk/tcbn65lpbc.lib ../pdk/tcbn65lplt.lib ../pdk/tcbn65lpml.lib -s

## supercell sub command

cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
./zlibvalidation supercell ~/examples/liberate_lv/LIBRARY/example.lib -c 3

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

## compare sub command

cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
./zlibvalidation compare --comp ../pdk/tcbn65lpbc.ski.lib --ref ../pdk/tcbn65lpbc.nldm.pt.lib

## func sub command

cd ~/Projects/ZlibValidation/build
./zlibvalidation clear
./zlibvalidation -h
./zlibvalidation func --ref ../pdk/tcbn65lpbc.lib --comp ../pdk/tcbn65lp.v --cells IAO21D0 CMPE42D1