// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "operations/Operations.h"

namespace ux3d::operations::advanced {

Operation paste(const nlohmann::json& linearizedGltfSchema);
Operation cutAndPaste(const nlohmann::json& linearizedGltfSchema);

}
