#include "FileSequence.h"
#include "pathutils.h"


FileSequence::FileSequence(std::filesystem::path filepath) {
    auto [p, b, e, c] = scan_for_sequence(filepath);
    pattern = p;
    first_frame = b;
    last_frame = e;
}

std::filesystem::path FileSequence::item(int F)
{
    if (pattern.empty()) return "";
    return sequence_item(pattern, F);
}