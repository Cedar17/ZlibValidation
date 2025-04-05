# Test Command for this project



## spice sub command
cd ~/Projects/ZlibValidation
mkdir build
cd build
cmake .. -G Ninja
ninja
./zlibvalidation --help
./zlibvalidation --version
./zlibvalidation clear
./zlibvalidation spice ../pdk/tcbn65lpbc.lib -c 3 --cells FA1D0 INVD0