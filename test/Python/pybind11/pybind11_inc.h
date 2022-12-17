#pragma once

#include "pybind11/complex.h"
#include "pybind11/functional.h"
#include "pybind11/operators.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

/// GGG Note: if you include this file, please also #include pybind11_undef.h
/// at the *BOTTOM* of your file to prevent macro poisoning in unity builds

namespace py = pybind11;

#define write( cls, x ) def_readwrite( #x, &cls::x )
#define read( cls, x ) def_readonly( #x, &cls::x )
#define func( x, ... ) def( #x, &x, __VA_ARGS__ )
#define method( cls, x, ... ) def( #x, &cls::x, __VA_ARGS__ )
#define pyarg( a ) py::arg( #a )
#define class_const_ptr( cls ) py::class_<cls, std::unique_ptr<cls,py::nodelete>>( mod, #cls )
#define class1( cls, ... ) py::class_<cls>( mod, #cls ).def( py::init<__VA_ARGS__>() )
#define class2( cls1, cls2, ... ) \
	py::class_<cls1, cls2>( mod, #cls1, py::dynamic_attr() ).def( py::init<__VA_ARGS__>() )
#define implements( ret, cls, func, ... )                      \
	override                                                   \
	{                                                          \
		PYBIND11_OVERLOAD( ret, cls, func, __VA_ARGS__ );      \
	}