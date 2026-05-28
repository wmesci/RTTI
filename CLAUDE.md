# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

RTTI 是一个 **header-only** C++20 运行时类型信息和反射库，参考 C# 反射 API 设计。以 CMake INTERFACE 库形式发布（无编译产物），通过 `FetchContent` 或 `find_package` 集成。

## 构建与测试

```bash
mkdir build && cd build
cmake .. -DBUILD_RTTI_TESTS=ON
cmake --build .
./rtti_test        # 运行测试
ctest -V           # 或通过 CTest 运行
```

库本身是 INTERFACE 类型，无需编译；测试目标 `rtti_test` 只有开启 `BUILD_RTTI_TESTS` 时才会构建。

## 架构概览

### 两种类型体系

库将所有类型分为两类，这是理解整个架构的核心：

1. **引用类型（Reference Type）**：继承自 `rtti::Object`，使用 `Ptr<T>`（默认 `std::shared_ptr<T>`）管理，赋值后多个变量指向同一对象
2. **值类型（Value Type）**：不继承 `Object`，通过 `Boxed<T>` 包装为 `ObjectBox` 进入类型系统。赋值后成为独立实例

`Reflection.h` 中的 `TypeWarper<T>` 模板通过 `is_object<T>` 在编译期分派这两类，决定其 `vartype`（变量类型）和 `base`（类型系统中的基类：引用类型为实际基类，值类型统一为 `ObjectBox`）。

### 核心文件与职责

| 文件 | 职责 |
|------|------|
| `Config.h` | 指针类型别名 `RTTI_PTR_ALIAS(T)`（默认 `std::shared_ptr<T>`）+ 操作 traits：`pointer_ops<T>`（make/ptr_cast/to_raw/from_raw）、`object_base<T>`（Object 基类定制）。日志宏 |
| `System.h` | 前向声明、`MakePtr`/`PtrCast`/`RawFromPtr`/`PtrFromRaw` 辅助函数、`ObjectPtr` 类型、`is_object<T>`/`ObjectType`/`ValueType` concept、`HASH` 编译期字符串哈希宏 |
| `Reflection.h` | `TypeWarper<T>` 类型分派、`type_of<T>()`（返回类型单例 `Type*`）、`CreateType<T,BASE>()`（惰性创建 Type 对象）、`TYPE_DECLARE` 宏（在类中声明反射支持） |
| `Object.h` | 引用类型基类 `Object`，默认继承 `std::enable_shared_from_this<Object>` |
| `ObjectBox.h` | 值类型包装：`ObjectBox` 抽象基类、`Boxed<T>` 模板、`Box()`/`Unbox()` 装箱/拆箱函数 |
| `Attributable.h` | `Attributable` 基类，提供 `std::map<size_t, std::any>` 键值属性存储，`Type`、`MethodBase`、`PropertyInfo` 均继承自此 |
| `MethodInfo.h` | `MethodBase`（方法元数据基类）、`ConstructorInfo`（构造函数反射+调用）、`MethodInfo`（成员/静态方法反射+调用，含 `static_assert` 限制只接受函数指针/成员指针/无捕获 lambda） |
| `PropertyInfo.h` | `PropertyInfo`：通过 getter/setter `MethodInfo*` 实现属性读写反射 |
| `Type.h` | `Type` 类：类型元数据中心。包含类型名称、大小、基类、构造函数/方法/属性列表、类型转换器（`TypeConvertor`）、比较器（`ObjectComparer`）。提供 `Convert`（类型转换）、`Compare`（规范顺序保证对称性）、`Find`（O(log n) Map 查找）、`ForEach`（遍历所有类型）等静态方法，以及 `cast<To,From>()` 模板函数 |
| `TypeRegister.h` | `TypeRegister<T>`：流式 API 注册类型。对非枚举类型提供 `constructor`/`property`/`method`/`convert`/`compare`；对枚举类型提供 `value`/`compare`。还有 `DefaultEnumRegister` 自动注册枚举的基础设施 |
| `RTTI.h` | 汇总头文件 + `InitCoreType()` 注册所有基础类型（bool、int8_t~uint64_t、float、double、void*、string）及其相互转换/比较关系 |

### 类型注册表

所有 `Type` 对象存储在 `Type::GetRegistry()` 返回的 `static std::map<std::string, Type*>` 中，按名称索引。`Type::Find()` 为 O(log n) 查找，`Type::ForEach()` 按名称字母序迭代。`type_of<T>()` 内部调用 `CreateType<T,BASE>()`，使用函数内 `static` 变量保证单例且惰性初始化。类型注册必须在单线程启动阶段完成。

### cast 转换机制

`cast<To, From>(from, &ok)` 支持以下转换路径（优先级从高到低）：

1. 直接隐式转换（`std::is_convertible`、子类→基类）
2. 运行时 RTTI 检查（基类→子类 downcast）
3. 通过目标类型的单参数构造函数转换
4. 通过源类型的 `TypeConvertor` 列表转换（含引用类型的 `ConvertTo<T>` 方法和值类型的 `static_cast`）
5. 枚举↔整数互转

值类型（包括原始指针）到 `ObjectPtr` 需通过 `Box()` 装箱；`ObjectPtr` 到值类型通过 `Unbox()` 拆箱。

### 自定义配置

**智能指针类型**：在包含 RTTI 头文件之前定义 `RTTI_PTR_ALIAS(T)` 宏 + 特化 `rtti::detail::pointer_ops<T>` 提供 make/ptr_cast/to_raw/from_raw 四个操作。

**Object 基类**：特化 `rtti::detail::object_base<rtti::Object>` 替换默认的 `std::enable_shared_from_this` 基类。

所有翻译单元中的自定义特化必须一致（否则 ODR 违规）。类型注册应在单线程启动阶段完成。详见 README.md 的"自定义配置"章节。

### 关键设计约束

- **Compare 对称性**：`Type::Compare` 使用 `std::less<Type*>` 建立规范参数顺序，确保 `Compare(A,B)` 和 `Compare(B,A)` 结果一致
- **MethodInfo 安全性**：`MethodInfo::Register` 只接受函数指针、成员指针或无捕获 lambda——有捕获的 lambda（尤其引用捕获）会在编译期被 `static_assert` 拒绝，防止 use-after-free
- **值类型精确匹配**：`CreateInstance` 对值类型参数要求精确类型匹配（不像引用类型使用 `CanConvertTo`），避免 integral↔enum 转换路径引发的递归问题
- **`-fno-rtti` 兼容**：库不依赖 C++ 原生 `typeid`/`dynamic_cast`，在关闭 RTTI 的环境下可正常使用

### 命名约定

- 类型注册时属性名使用 PascalCase
- 使用 `HASH("string")` 编译期哈希优化属性/特性查找性能
- `TYPE_DECLARE(BaseClass)` 宏在类体内声明基类关系并覆盖 `GetRttiType()`
