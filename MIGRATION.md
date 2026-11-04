# Migration Guide (0.9.x → 1.1)

This release is a deliberate breaking refactor. It fixes the findings in
`CODE_REVIEW.md`; the public surface was cleaned up rather than kept
bug-compatible.

## Headers

The library is still flat (`src/*.h`) and the installed include directory is
`include/RTTI`, so consumers use:

```cpp
#include <RTTI.h>
#include <DynamicType.h>
```

`#include <RTTI/RTTI.h>` was never valid for the installed target and is no
longer documented.

## Renames

| Old | New |
|---|---|
| `TYPE_DECLARE(...)` | `RTTI_TYPE_DECLARE(...)` |
| `HASH("x")` | `RTTI_HASH("x")` |
| `TypeWarper`, `warper`, `warper_t` | `TypeWrapper`, `wrapper`, `wrapper_t` |
| `TypeConvertor`, `Convertor`, `m_typeConvertors` | `TypeConverter`, `Converter`, `m_typeConverters` |
| `remove_cr<T>` | `remove_const_ref<T>` |
| `IsSubClassOf` | `IsSubclassOf` |
| `GetEnumInfos()` | `GetEnumValues()` |
| `PropertyInfo::PropertyType()` | `GetPropertyType()` |
| `MethodBase::OwnerType()` | `GetOwnerType()` |
| `MethodBase::ReturnType()` | `GetReturnType()` |
| `Type::Create<T>()` | `Type::CreateInstance<T>()` |
| `Type::GetUnderlyingType()` | `GetEnumUnderlyingType()` / `GetPointeeType()` |
| `ObjectBox::GetPointer()` | `ObjectBox::GetValueAddress()` (payload address) |
| `BUILD_RTTI_TESTS` (CMake) | `RTTI_BUILD_TESTS` (old name still mapped) |

Free helpers keep their existing spelling: `type_of`, `cast`, `convert`,
`compare`, `is_comparable`, `hash_string`, `MakePtr`, `PtrCast`,
`RawFromPtr`, `PtrFromRaw`, `NewType`, `NewDynamicType`, `InitCoreType`.

## Type names and lookup

- Compile-time types now use fully qualified names (`halo::Transform`) as the
  canonical identity. The name passed to `TypeRegister<T>::New("...")` is an
  explicit alias; conflicting aliases throw `DuplicateTypeName`.
- A best-effort short-name alias is still registered when unique, which keeps
  old serialized documents loadable.
- `GetSerializationName()` returns the explicit registration name (if any),
  otherwise the unique short alias, otherwise the canonical name. This is the
  recommended value for external identifiers / serialized `$type` fields and
  preserves existing HaloEngine documents. New code should use `GetName()`
  when it needs the stable fully qualified identity.
- Built-ins use stable canonical names (`int32_t`, `int64_t`, `float`,
  `string`, `void*`, ...). `Type::Find("int")` remains available as a soft
  alias for `int32_t` on platforms where they are the same type.

## Registry ownership and freezing

- `TypeRegistry` now owns every `Type` (`unique_ptr`), and `Type` owns its
  constructors/methods/properties. There is no intentional process-lifetime
  leak anymore.
- `Type` is non-copyable and cannot be constructed on the stack.
- Registration is idempotent: repeated `TypeRegister<T>::New()...` calls do
  not accumulate duplicate metadata.
- `TypeRegistry::Get().Freeze()` forbids later compile-time registration
  (`RegistryFrozen`). `NewDynamicType` remains allowed after freeze.
- `Type::GetRegistry()` (mutable map) was removed. Use `Type::Find`,
  `Type::ForEach` (snapshot semantics) or `Type::GetTypes`.

## Errors

- Misuse now throws `rtti::Error` with an `ErrorCode`; target exceptions are
  wrapped in `rtti::TargetInvocationError` (`Inner()` returns the
  `std::exception_ptr`).
- `Invoke`, `GetValue`, `SetValue`, `CreateInstance`, `Unbox` no longer
  silently fall back to default values on bad input.
- `MethodInfo::Invoke` / `PropertyInfo::SetValue` require exact argument
  counts; conversion failure aborts before invoking the target.
- Non-throwing variants are available: `TryInvoke`, `TryGetValue`,
  `TrySetValue`, `TryCreateInstance`, `try_cast`, `try_unbox`.
- `cast` remains non-throwing (use `bool* ok` / `GetLastError`);
  `cast_or_throw` is the throwing variant.
- `-fno-exceptions` / `-DRTTI_NO_EXCEPTIONS`: recoverable failures record a
  thread-local `GetLastError()` and return a default; registration/grammar
  errors abort.

## Boxing and unboxing

- `Unbox<T>` always validates the RTTI type, including in Release builds.
  `Unbox<void*>(Box(&value))` now returns the stored pointer value.
- `Box(Ptr<T>)` returns the object pointer itself, not a boxed handle.
  `Box("literal")` boxes `std::string`.
- `Box` no longer accepts `Ptr<T>` through the value-type path, so variadic
  `Invoke(target, somePtr)` passes the object correctly.

## cast

- `cast<int>(Ptr<T>)` and `cast<int*>(...)` are supported.
- Object casts return `Ptr<T>`; raw pointer targets return `T*`.
- Object-to-value conversion walks the source type's base chain.
- Numeric conversions (including `bool`, character types and `long double`)
  are built in; no per-pair `convert<>()` registration is required.

## DynamicType

- `DynamicObject` no longer claims to be a C++ subclass it is not. A dynamic
  type may combine one compile-time base with a default constructor; the
  base instance is stored inside `DynamicObject` and used by reflection
  dispatch and `cast<Base>`.
- Compile-time bases without a default constructor are rejected with
  `InvalidDynamicType`.
- Dynamic metadata is stored in the base `Type` containers, so
  `GetConstructor(list)`, `GetMethod(name, args)` and `GetProperties` behave
  consistently through `Type*` and `DynamicType*`.
- `NewDynamicMethod`/`NewDynamicConstructor` owners are bound by
  `NewDynamicType`; use `AttachMethod`/`AttachProperty`/`AttachConstructor`
  to attach additional metadata.
- `DynamicPropertyInfo` validates the target type and the written value type,
  and reading an unset property returns a default value without inserting a
  null entry.

## CMake

- New option: `RTTI_BUILD_TESTS` (old `BUILD_RTTI_TESTS` is mapped).
- The package version file is architecture independent.
- Install layout is unchanged: headers under `include/RTTI`, include directory
  points there, package config in `lib/cmake/rtti`.

## Suggested engine-side migration

1. Add `rtti::InitCoreType()` once during startup.
2. Replace old macro/accessor names (the table above).
3. After all engine/component registrations, optionally call
   `rtti::TypeRegistry::Get().Freeze()`; dynamic script types still work.
4. Replace `if (!type->CreateInstance(...))` patterns with
   `TryCreateInstance` (or catch `rtti::Error`).
5. Replace raw `GetPointer()` on `ObjectBox` with `GetValueAddress()`.
6. Use `GetSerializationName()` for external identifiers (MCP/script/UI/asset
   names) and `GetName()` for canonical identity. Old short names still resolve
   through soft aliases.
7. `select_overload` supports both `select_overload<Sig>(&C::f)` and
   `select_overload<Sig, C>(&C::f)`.
