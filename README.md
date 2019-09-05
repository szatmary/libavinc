# libavinc
libav* Modern C++ API

libavinc is an experiment in using some modern c++ (c++11) features
with the libav* libraries from the ffmpeg project. libavinc's goal
is not to replace or wrap the av* functions in c++ code, but to
offer an API similar to the C API, but enable some of the features
that make modern c++ development great.

Guiding principals of this project:
* Any av* object/context that must be freed or urefrenced should be replaced
with a std::unique_ptr for that type. Eg. AVPacket should become
std::unique_ptr<::AVPacket, void (*)(::AVPacket*)>

* std::shared_ptr SHOULD NOT be used. av* has a good refrence counting system, use it.

* A libavinc object SHOULD NOT be extended via methods or members,
with some exceptions; particularly when adding functionality enables
modern c++ features. For example, ranged for loops, initialization_list, etc.
Or if extending an object simplifies a common use cases (eg. AVFormatContext.open_streams)

* Not all av* C function require a libavinc counterpart.
There is nothing wrong with unique_ptr<T>::get()

* Usage of av* C functions SHOULD be prefixed with a double colon (::) to indentify
it as part of the global name space. Eg. ::av_dump_format()

* All timestamps SHOULD be represented with std::chrono::duration. Spacifically the 'flicks' type

=====
Enableing a debug build via `cmake -DCMAKE_BUILD_TYPE=Debug .` will turn on address satanitaion and will report memory leaks
