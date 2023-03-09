#pragma once

#include <string>

// Helpers for determining the actual path to the executable
std::wstring GetExePath();
std::wstring FixPath(const std::wstring& relativeFilePath);
std::string WideToNarrow(const std::wstring& str);
std::wstring NarrowToWide(const std::string& str);