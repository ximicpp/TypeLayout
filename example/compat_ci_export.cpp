// Green-path signature export for the reference compatibility pipeline.
//
// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "compat_ci_types.hpp"

#include <boost/typelayout/tools/sig_export.hpp>

TYPELAYOUT_EXPORT_TYPES(
    PacketHeader,
    SharedMemRegion,
    FileHeader,
    SensorRecord,
    IpcCommand,
    MixedSafety
)
