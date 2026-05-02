// Copyright (c) 2018 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port/port_stdcxx.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#if (defined(__x86_64__) || defined(__i386__)) && defined(__GNUC__)
#include <cpuid.h>
#endif

namespace leveldb {
namespace port {

bool HasAcceleratedCRC32C() {
#if (defined(__x86_64__) || defined(__i386__)) && defined(__GNUC__)
  unsigned int eax, ebx, ecx, edx;
  __get_cpuid(1, &eax, &ebx, &ecx, &edx);
  return (ecx & (1 << 20)) != 0;
#else
  return false;
#endif
}

}  // namespace port
}  // namespace leveldb
