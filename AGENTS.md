# AGENTS.md

本文件为 AI 编码代理（Claude Code / Copilot 等）在该仓库（RTTI，`build/_deps/rtti-src`）工作时提供指引。若与代码冲突，以代码为准。

## 项目概述

RTTI 是一个 **header-only** C++20 运行时类型信息/反射库，参考 C# 反射 API。以 CMake INTERFACE 库发布（`rtti::rtti`），通过 FetchContent 或 `find_package` 集成。

## 构建与测试

```bash
cmake -S . -B build -DRTTI_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# 常用变体
cmake -S . -B build-asan -DRTTI_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake -S . -B build-noexc -DRTTI_BUILD_TESTS=ON -DCMAKE_CXX_FLAGS="-fno-exceptions -DRTTI_NO_EXCEPTIONS"
```

- 旧选项 `BUILD_RTTI_TESTS` 仍兼容（映射到 `RTTI_BUILD_TESTS`），新代码使用 `RTTI_BUILD_TESTS`。
- 测试目标 `rtti_test` 仅在开启选项时构建。
- CI：Debug/Release × macOS/Linux/Windows、ASan/UBSan、TSan、`-fno-rtti`、`-fno-exceptions`、安装消费。

## 架构概览

### 两种类型体系

1. **引用类型**：继承 `rtti::Object`，由 `Ptr<T>`（默认 `std::shared_ptr<T>`，可自定义）管理。
2. **值类型**：不继承 `Object`，通过 `Boxed<T>` 装箱进入 `ObjectPtr` 世界。

`Reflection.h` 的 `TypeWrapper<T>` 在编译期分派：引用类型取 `BASE_TYPE`，值类型 base 为 `ObjectBox`。

### 核心文件

| 文件 | 职责 |
|---|---|
| `Config.h` | `RTTI_PTR_ALIAS`、`RTTI_OBJECT_BASE`、`RTTI_CUSTOM_POINTER_OPS`、异常检测、日志钩子 |
| `Error.h` | `ErrorCode`、`Error`、`TargetInvocationError`、线程局部 `GetLastError()`、`Fail/Throw/Report` |
| `System.h` | 前向声明、`MakePtr/PtrCast/RawFromPtr/PtrFromRaw`、`ObjectPtr`、哈希、traits、`RTTI_HASH` |
| `Reflection.h` | `TypeWrapper`、`type_of<T>()`、`CreateType<T,BASE>()`、`ParameterInfo`、`TypeFlags`、`RTTI_TYPE_DECLARE` |
| `Object.h` | `Object`：`GetRttiType`、`GetHashCode`、`GetEmbeddedObject`/`GetNativeObject`（动态组合适配器） |
| `ObjectBox.h` | `ObjectBox`、`Boxed<T>`、`Box()`、checked `Unbox<T>`、`try_unbox` |
| `Attributable.h` | `map<size_t, any>` 属性存储、`GetAttribute/TryGetAttribute`、合并式 `SetAttributes` |
| `MethodInfo.h` | `MethodBase`、`ConstructorInfo`、`MethodInfo`：精确参数个数、失败不调用、目标异常包装 |
| `PropertyInfo.h` | getter/setter `MethodInfo`；`GetValue/SetValue` 抛错，`TryGetValue/TrySetValue` |
| `Type.h` | `Type` + `TypeRegistry`：全限定名/别名、独占所有权、Freeze、构造/方法/属性/转换/比较、`cast` |
| `TypeRegister.h` | 流式注册：`constructor/property/method/convert/Convert/compare/value`；幂等 |
| `RTTI.h` | 汇总头 + `InitCoreType()`（一次性注册内置数值/string/void*） |
| `DynamicType.h` | `DynamicType`/`DynamicObject`/`DynamicPropertyInfo`/`DynamicMethodInfo`/`DynamicConstructorInfo` |

### 类型注册表

- `TypeRegistry::Get()` 是进程级注册表，`std::shared_mutex` 保护；**独占拥有** `Type`，`Type` 独占拥有成员元数据。
- 编译期类型默认规范名为全限定名；注册时传入的名字是别名，冲突抛 `DuplicateTypeName`。
- 内置类型使用固定规范名（`int32_t`、`int64_t`、`float`、`string` 等），跨平台序列化稳定；旧短名通过软别名兼容。
- `Freeze()` 禁止后续编译期注册；动态类型仍可在写锁下创建。
- `InitCoreType()` 使用 `std::once_flag`，可重复调用。
- `ForEach` 遍历快照，回调中注册类型安全。

### 错误模型

- 默认启用异常：误用抛 `rtti::Error`（含 `ErrorCode`）；目标方法自身异常包装为 `TargetInvocationError`。
- `-fno-exceptions`：可恢复错误记录 `GetLastError()` 并返回默认值/`nullptr`；注册冲突等编程错误 abort。
- `TryInvoke` / `TryGetValue` / `TrySetValue` / `TryCreateInstance` 不抛异常。
- `cast` 是非抛出版本（`bool* ok` / LastError）；`cast_or_throw` 抛异常；`try_cast` 返回 bool。

### `cast` 与 `Unbox`

- `cast<To>` 支持对象↔对象、对象→值、值→值、指针、`void*`；对象返回 `Ptr<T>`。
- `Unbox<T>` 始终做 RTTI 校验（Release 也是）；失败抛错/LastError，不再依赖 `assert`。
- `Box(Ptr<T>)` 返回对象指针本身，不装箱 handle；`Box("literal")` 装箱为 `std::string`。
- `Boxed<T>` 的 `GetValueAddress()` 是载荷地址；`GetPointerValue()` 仅指针载荷。

### DynamicType

- schema → `DynamicPropertyInfo`，值存储在 `DynamicObject` 的私有 map；读写校验属性类型，读取不插入条目。
- 动态方法/构造器由 `NewDynamicMethod`/`NewDynamicConstructor` 创建，通过 `AttachMethod`/`AttachConstructor` 挂载；owner 自动绑定。
- 可继承动态类型；也可组合一个**可默认构造**的编译期基类：
  - `DynamicObject` 持有基类实例；继承属性/方法路由到该实例；
  - `cast<Base>(dynamicObject)` 返回该实例，不做非法强转；
  - 基类无默认构造时 `NewDynamicType` 报 `InvalidDynamicType`。
- 默认构造由工厂自动添加；`DynamicType::TryCreateInstance` 负责设置 effective type 和组合基类。

### 命名约定

- 类型/成员/宏：PascalCase + `m_` 成员前缀；宏 `RTTI_` 前缀。
- 自由函数/模板助手：snake_case（`type_of`、`cast`、`convert`、`compare`、`hash_string`）。
- 访问器统一 `Get*`：`GetName`、`GetOwnerType`、`GetReturnType`、`GetPropertyType`、`GetParameters()`（const）。
- 旧名已移除：`TYPE_DECLARE`→`RTTI_TYPE_DECLARE`、`HASH`→`RTTI_HASH`、`IsSubClassOf`→`IsSubclassOf`、`GetEnumInfos`→`GetEnumValues`、`PropertyType`→`GetPropertyType`、`ReturnType`→`GetReturnType`、`TypeWarper`→`TypeWrapper`、`TypeConvertor`→`TypeConverter`、`remove_cr`→`remove_const_ref`。

### 关键约束

- 反射方法的 callable 仅接受函数指针、成员指针、无捕获 lambda（编译期 `static_assert`）。
- 参数个数必须精确匹配；转换失败不会调用目标。
- 参数可以是 `T`、`const T&`、`T&`（out 参数写回传入的 box）。
- 属性 setter 可以是 `void(R)`、`void(R&)`、`void(const R&)`；getter 返回值/引用均可。
- 注册是幂等的；重复注册不会累积构造/方法/属性。
- `Freeze` 后不能改元数据；动态类型除外。
