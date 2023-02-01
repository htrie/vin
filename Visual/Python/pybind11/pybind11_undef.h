#pragma once

// #undef the #defines made in pybind11_inc.h
// please #include this at the *BOTTOM* of any file that #includes pybind11_inc.h
// so that unity builds can function without macro poisoning

#undef write
#undef read
#undef func
#undef method
#undef pyarg
#undef class_const_ptr
#undef class1
#undef class2
#undef implements