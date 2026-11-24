#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace wb::commands {
class Workbench;
}

namespace wb {

// The root/viewmodel bones compared between two weapons by `-posedump`.
std::vector<std::string> DefaultPoseDumpBones();

// Text dump of the posed (animated) hands+gun skeleton: world positions in viewmodel
// space, local offsets, the animation channels that drive these bones and the gun bounds.
// Used for the M4 crossbow diagnosis and the fit report; it is plain text so it can be
// committed under review/.
void PrintPoseDump(const commands::Workbench& workbench, const std::vector<std::string>& names,
                   int width, int height, std::FILE* out);

} // namespace wb
