#include "reproc/error.hpp"

namespace reproc {

#include "reproc/error.h"

unsigned int system_error() { return reproc_system_error(); }

std::string error_to_string(error error)
{
  if (error == reproc::unknown_error) {
    return "reproc => unknown error. system error = " +
           std::to_string(system_error());
  }
  return reproc_error_to_string(static_cast<REPROC_ERROR>(error));
}

} // namespace reproc
