/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Creates instances of @ref tracing_data.
class CAF_CORE_EXPORT tracing_data_factory {
public:
  virtual ~tracing_data_factory();

  /// Deserializes tracing data from `source` and either overrides the content
  /// of `dst` or allocates a new object if `dst` is null.
  /// @returns the result of `source`.
  virtual bool deserialize(deserializer& source,
                           std::unique_ptr<tracing_data>& dst) const = 0;

  /// @copydoc deserialize
  virtual bool deserialize(binary_deserializer& source,
                           std::unique_ptr<tracing_data>& dst) const = 0;
};

} // namespace caf
