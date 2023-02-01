#pragma once

#include "Visual/Python/Interpreter.h"

#if PYTHON_ENABLED
#include "Visual/Python/pybind11/pybind11_inc.h"

// Functions flagged with this will generate module code
#define GENERATE

#endif