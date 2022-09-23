#pragma once

// Pointer types

template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T>
using WeakRef = std::weak_ptr<T>;

template <typename T>
using Scope = std::unique_ptr<T>;

template <typename T1, typename T2>
Ref<T1> RefCast(const Ref<T2>& x) { return std::static_pointer_cast<T1>(x); }

// Macros

#define HZ_ASSERT(x, ...) { if (!(x)) { SPDLOG_ERROR(__VA_ARGS__); assert(false); } }
