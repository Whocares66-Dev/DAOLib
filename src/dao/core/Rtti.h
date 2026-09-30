// MSVC's RTTI names a class in its decorated form, ".?AVFoo@Bar@@" for
// Bar::Foo: the parts between the '@'s, innermost first. The exe keeps
// about 2350 of them (tools/daoexe.py --rtti lists them), which is how a
// vtable, and so an object, is told apart at runtime (dao/RTTI.h) and in
// Ghidra. This is the pure part: text in, text out.
#pragma once

#include <string>
#include <string_view>

namespace dao::core
{

// ".?AVFoo@Bar@@" -> "Bar::Foo"; ".?AUPod@@" (a struct) the same way. Text
// that is not a decorated class name comes back unchanged, and so does a
// template's name (".?AV?$Name@...@@"): tools/msvc_names.py undecorates those.
std::string UndecorateClassName(std::string_view decorated);

} // namespace dao::core
