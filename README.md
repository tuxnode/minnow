Stanford CS 144 Networking Lab
==============================

These labs are open to the public under the (friendly, but also mandatory)
condition that to preserve their value as a teaching tool, solutions not
be posted publicly by anybody.

Website: https://cs144.stanford.edu

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target check[checkpoint_num]`

or `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`

*当前已经全部通过Lab0 - Lab6的测试*

[学习路线](./README_Net.md)

个人实现[博客](https://tuxnode.fun)
