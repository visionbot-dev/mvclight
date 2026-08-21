#ifndef MVC_LIGHT_MVC_LIGHT_EXPORT_H
#define MVC_LIGHT_MVC_LIGHT_EXPORT_H

/*
 * 符号可见性控制（设计文档附录 D.3）。
 * 仅 mvc_light_* 函数通过 MVCLIGHT_API 导出；内部 C++ 符号全部隐藏。
 */

#if defined(_WIN32)
#  if defined(MVCLIGHT_BUILD_SHARED)
#    define MVCLIGHT_API __declspec(dllexport)
#  elif defined(MVCLIGHT_USE_SHARED)
#    define MVCLIGHT_API __declspec(dllimport)
#  else
#    define MVCLIGHT_API
#  endif
#elif defined(__GNUC__) && defined(MVCLIGHT_BUILD_SHARED)
#  define MVCLIGHT_API __attribute__((visibility("default")))
#else
#  define MVCLIGHT_API
#endif

#endif // MVC_LIGHT_MVC_LIGHT_EXPORT_H
