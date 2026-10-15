!#/bin/bash

echo "Formatting code..."
find . -type d \( -name build -o -name benchmarks \) -prune -o \
  -type f \( \
    -name '*.c' -o \
    -name '*.h' -o \
    -name '*.cpp' -o \
    -name '*.hpp' -o \
    -name '*.cc' -o \
    -name '*.hh' \
  \) -exec clang-format -i {} +
echo "Code formatting completed."
