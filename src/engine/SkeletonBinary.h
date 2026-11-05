// Binary skeleton format (.skel): a compact alternative to the JSON export.
//
// The layout follows the publicly documented Spine binary format (big-endian numbers, variable-length integers, a shared
// string table) in its 4.2 arrangement. It was written from the public format description, NOT checked against Esoteric
// Software's runtimes, so treat it as experimental when loading it with them. Osteon2D reads back what it writes
// (see tests/test_skeleton_binary.cpp), so a .skel can always be turned back into JSON.
//
// Not representable in the 4.2 binary format and therefore left out (a warning is reported for each): slider constraints
// (Spine 4.3), the property links of 4.3 transform constraints (the classic same-channel copy is written instead).
#pragma once
#include "SkeletonJson.h"
#include <cstdint>

namespace osteon {

struct BinaryWriteOptions {
    bool nonessential = true; // bone colours, edges, mesh sizes, fps, images path ...
    std::string version = "4.2.00";
};

/** Serialises `data` (apply scaleSkeletonData / cleanUpAnimations first if needed). `warnings` collects what was left out. */
std::vector<uint8_t> writeSkeletonBinary(const SkeletonData& data, const BinaryWriteOptions& opt = {}, std::vector<std::string>* warnings = nullptr);

/** Parses a .skel produced by writeSkeletonBinary (and files with the same layout). */
JsonResult readSkeletonBinary(const uint8_t* bytes, size_t size, SkeletonData& out);

} // namespace osteon
