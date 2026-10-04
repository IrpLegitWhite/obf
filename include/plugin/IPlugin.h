#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

	// Версия API. При изменении интерфейса — поднять.
#define OBFS_PLUGIN_API_VERSION 3

// ─── Типы экспортов ──────────────────────────────────────────
	typedef const char* (*obf_name_fn)();
	typedef const char* (*obf_description_fn)();
	typedef int         (*obf_api_version_fn)();
	typedef const char* (*obf_languages_fn)();
	typedef int         (*obf_can_apply_fn)(const char* code);
	typedef char* (*obf_apply_fn)(const char* code, size_t size, size_t* out_size);
	typedef void        (*obf_free_fn)(char* ptr);

	// ─── Требуемые экспорты плагина ──────────────────────────────
	//
	//   extern "C" __declspec(dllexport) const char* obf_name();
	//   extern "C" __declspec(dllexport) const char* obf_description();
	//   extern "C" __declspec(dllexport) int         obf_api_version();
	//   extern "C" __declspec(dllexport) const char* obf_languages();  // "cpp,c,csharp"
	//   extern "C" __declspec(dllexport) int         obf_can_apply(const char* code);
	//   extern "C" __declspec(dllexport) char*       obf_apply(const char* code, size_t size, size_t* out_size);
	//   extern "C" __declspec(dllexport) void        obf_free(char* ptr);
	//
	// ─── Пример ──────────────────────────────────────────────────
	//
	//   extern "C" __declspec(dllexport) const char* obf_name() {
	//       return "my-plugin";
	//   }
	//   extern "C" __declspec(dllexport) const char* obf_description() {
	//       return "My obfuscation plugin";
	//   }
	//   extern "C" __declspec(dllexport) int obf_api_version() {
	//       return OBFS_PLUGIN_API_VERSION;
	//   }
	//   extern "C" __declspec(dllexport) const char* obf_languages() {
	//       return "cpp,c,csharp";
	//   }
	//   extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
	//       return code ? 1 : 0;
	//   }
	//   extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size, size_t* out_size) {
	//       std::string in(code, size);
	//       std::string out = in;  // твоя обфускация
	//       char* r = (char*)std::malloc(out.size() + 1);
	//       std::memcpy(r, out.c_str(), out.size() + 1);
	//       *out_size = out.size();
	//       return r;
	//   }
	//   extern "C" __declspec(dllexport) void obf_free(char* ptr) {
	//       std::free(ptr);
	//   }
	//
#ifdef __cplusplus
}
#endif