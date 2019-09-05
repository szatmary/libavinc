#!/bin/bash
cd "$(dirname "$0")"
find . -type f \( -name '*.cpp' -o -name '*.hpp' \) -exec clang-format -i -style=WebKit {} \;
