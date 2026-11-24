#pragma once

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace wb {

// -info <model>: text dump of bones, surfaces, materials and bounds.
bool PrintModelInfo(const std::string& name, const std::vector<std::filesystem::path>& roots);

// -info <xanim>: frames, framerate, bone names and notetracks with times.
bool PrintAnimInfo(const std::string& name, const std::vector<std::filesystem::path>& roots);

// -info <weapon>: view models, hands, ADS FOV, hideTags, attachments, state/override table.
bool PrintWeaponInfo(const std::string& name, const std::vector<std::filesystem::path>& roots);

// -validate: parse every .xmb in every root plus its .xsb surface sets and material
// json. Prints one line per failure and a summary. Returns the number of failures.
int RunValidate(const std::vector<std::filesystem::path>& roots);

// -roundtrip: read and write every .xmb in every root into outputDir and byte-compare the
// result. Prints one line per mismatch and a summary. Returns errors + mismatches.
int RunRoundTrip(const std::vector<std::filesystem::path>& roots, const std::filesystem::path& outputDir,
                 std::FILE* out);

} // namespace wb
