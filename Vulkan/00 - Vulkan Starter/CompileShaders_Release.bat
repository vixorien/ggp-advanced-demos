forfiles /M *.vert /C "cmd /c \"\"%VULKAN_SDK%\Bin\glslc.exe\" @file -o x64/Release/@file.spv\""
forfiles /M *.frag /C "cmd /c \"\"%VULKAN_SDK%\Bin\glslc.exe\" @file -o x64/Release/@file.spv\""