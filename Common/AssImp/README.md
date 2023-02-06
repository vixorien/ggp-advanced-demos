# AssImp
## The Open Asset Importer Library
Necessary files have already been compiled and included here.  However, should you be starting from scratch, here is what you need to do:

1. Download the latest [AssImp code from GitHub](https://github.com/assimp/assimp)
2. Use [CMake](https://cmake.org/) to create the Visual Studio project files necessary to build AssImp: 
	- `cmake CMakeLists.txt -DASSIMP_BUILD_ZLIB=ON`
	- The above command is what I used, as it needed to build the zlib compression library, too
3. Open the resulting .sln file and build in Debug, Release or both
4. Copy the following to your own project's folder (or a suitable subfolder):
	- The entire include\ folder
	- The resulting lib\Debug\*.lib file (and/or \Release version)
	- The resulting bin\Debug\*.dll file (and/or \Release version)