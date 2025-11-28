#pragma once

// For surppress 'allocator' warning in boost/asio.hpp while compiled by Android
// NDK.

// Using this header in your source files other than directly using
// boost/asio.hpp

#if defined(__ANDROID__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include "boost/asio.hpp"
#pragma clang diagnostic pop
#else
#include "boost/asio.hpp"
#endif
