#!/bin/bash
cd $(dirname $0)

find src test examples \( -name '*.cpp' -or -name '*.hpp' \) -and -not \( -name catch.hpp -or -path '*/build/*' \) -exec clang-format -style=file -i '{}' +

cmake-format -i CMakeLists.txt
find examples -name CMakeLists.txt -exec cmake-format -i '{}' +
