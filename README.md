# RTTI

一个 header-only、C++20、参考 C# 反射 API 的运行时类型信息与反射库。

- 引用类型（继承 `rtti::Object`）与值类型（自动装箱）统一进入类型系统
- 运行时类型查询、类型转换、属性/方法/构造函数反射
- 属性（Attribute）、枚举、继承、比较与转换器
- 运行时 `DynamicType` / `DynamicObject`，支持继承动态类型或组合编译期基类
- 不依赖 C++ 原生 `typeid` / `dynamic_cast`（`-fno-rtti` 可用）
- 支持异常与 `-fno-exceptions` 两种构建；后者提供线程局部 `GetLastError()` + `Try*` API
- 类型注册表独占所有权，支持 `Freeze()` 后只读；`Freeze` 后仍可创建动态类型

## 要求

- C++20 或更高
- CMake 3.14 或更高

## 安装与集成

### FetchContent（推荐）

```cmake
include(FetchContent)
FetchContent_Declare(
    RTTI
    GIT_REPOSITORY https://github.com/wmesci/RTTI.git
    GIT_TAG main
)
FetchContent_MakeAvailable(RTTI)

target_link_libraries(YourTarget PRIVATE rtti::rtti)
```

### 安装后 find_package

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
find_package(RTTI REQUIRED)
target_link_libraries(YourTarget PRIVATE rtti::rtti)
```

安装后头文件位于 `<prefix>/include/RTTI/`，导入目标的 include 目录指向该目录，因此使用：

```cpp
#include <RTTI.h>
#include <DynamicType.h>
```

## 初始化

核心数值类型、`std::string`、`void*` 的类型信息由 `rtti::InitCoreType()` 注册。它只生效一次，可安全重复调用：

```cpp
#include <RTTI.h>

int main()
{
    rtti::InitCoreType();

    // ... 注册自己的类型，然后可选地冻结注册表：
    // rtti::TypeRegistry::Get().Freeze();
}
```

`Freeze()` 会禁止后续编译期类型/成员注册（`TypeRegistry` 抛 `RegistryFrozen`），但**仍允许** `NewDynamicType`，适合"启动期注册、运行期脚本动态类型"的模式。

## 快速开始

### 引用类型

```cpp
#include <RTTI.h>

class MyClass : public rtti::Object
{
    RTTI_TYPE_DECLARE(rtti::Object)

public:
    MyClass() = default;
    explicit MyClass(int value)
        : m_value(value)
    {
    }

    int GetValue() const { return m_value; }
    void SetValue(int value) { m_value = value; }

private:
    int m_value = 0;
};

// 注册（建议在引擎/应用初始化阶段集中执行）
rtti::TypeRegister<MyClass>::New()
    .constructor<>()
    .constructor<int>()
    .property("Value", &MyClass::GetValue, &MyClass::SetValue)
    .method("SetValue", &MyClass::SetValue);
```

创建与访问：

```cpp
auto object = rtti::type_of<MyClass>()->CreateInstance<MyClass>(42);

auto* type = object->GetRttiType();          // rtti::Type*
auto* property = type->GetProperty("Value"); // rtti::PropertyInfo*

property->SetValue(object, rtti::Box(100));
int value = property->GetValue<int>(object);

auto* method = type->GetMethod("SetValue");
method->Invoke(object, 200);
```

### 值类型

```cpp
struct Vector3
{
    float x = 0, y = 0, z = 0;
    float GetLength() const { return std::sqrt(x * x + y * y + z * z); }
};

rtti::TypeRegister<Vector3>::New()
    .constructor<>()
    .property("x", &Vector3::x)
    .property("y", &Vector3::y)
    .property("z", &Vector3::z)
    .method("GetLength", &Vector3::GetLength);

auto boxed = rtti::Box(Vector3{ 1, 2, 3 });
auto* method = boxed->GetRttiType()->GetMethod("GetLength");
float length = rtti::Unbox<float>(method->Invoke(boxed, {}));
```

> 值类型在反射边界通过 `Boxed<T>` 进入 `ObjectPtr` 世界；`Unbox<T>` 会在 Release 下也校验类型，不匹配时抛 `rtti::Error`（或设置 `GetLastError()`）。

### 方法参数

方法参数可以是值、`const T&`、非 const `T&`（可用于 out 参数）：

```cpp
class Api : public rtti::Object
{
    RTTI_TYPE_DECLARE(rtti::Object)
public:
    static void Register()
    {
        rtti::TypeRegister<Api>::New()
            .constructor<>()
            .method("TryRead", &Api::TryRead);
    }

    bool TryRead(int& output)
    {
        output = 42;
        return true;
    }
};

// out 参数写回调用方传入的 box
auto boxedOut = rtti::Box(0);
rtti::type_of<Api>()->GetMethod("TryRead")
    ->Invoke(rtti::MakePtr<Api>(), { boxedOut });
int output = rtti::Unbox<int>(boxedOut); // 42
```

反射调用使用**精确参数个数**；参数转换失败会抛出 `Error(ArgumentType)`，绝不使用默认值继续调用。

### 类型转换

```cpp
MyClass* raw = ...;
rtti::Ptr<MyClass> object = rtti::cast<MyClass>(raw);   // 原始指针 -> 智能指针
bool ok = false;
auto base = rtti::cast<Base>(object, &ok);              // 失败返回 null 并设置 ok=false
```

`cast` 是非抛出版本（通过 `bool* ok` / `GetLastError` 报告失败）；需要抛异常时使用 `cast_or_throw`，需要显式检查时使用 `try_cast`。

### 类型转换器

两种写法，任选其一：

```cpp
class MyClass : public rtti::Object
{
    RTTI_TYPE_DECLARE(rtti::Object)
public:
    // 旧协议：成员模板
    template <typename U>
    bool ConvertTo(U& target);
};

// 方式 1：使用 ConvertTo 成员
rtti::TypeRegister<MyClass>::New().convert<int>();

// 方式 2：注册回调（不需要修改类）
rtti::TypeRegister<MyClass>::New()
    .Convert<int>([](const MyClass& value, int& target)
                  {
                      target = value.GetValue();
                      return true;
                  });
```

### 属性（Attribute）

```cpp
rtti::TypeRegister<MyClass>::New()
    .property("Value", &MyClass::GetValue, &MyClass::SetValue,
              { { RTTI_HASH("Min"), 0 }, { RTTI_HASH("Max"), 100 } });

auto* property = rtti::type_of<MyClass>()->GetProperty("Value");
int min = property->GetAttribute<int>(RTTI_HASH("Min"));
bool hasMax = property->HasAttribute(RTTI_HASH("Max"));
```

属性 key 是编译期字符串哈希；`RTTI_HASH` 要求字符串字面量。属性以 `std::map<size_t, std::any>` 存储，`New()` 会合并而不是覆盖已有属性。

### 枚举

```cpp
enum class Color { Red, Green, Blue };

rtti::TypeRegister<Color>::New()
    .value("Red", Color::Red)
    .value("Green", Color::Green)
    .value("Blue", Color::Blue);

auto* type = rtti::type_of<Color>();
std::string name = type->GetEnumName(rtti::Box(Color::Red));      // "Red"
rtti::ObjectPtr value = type->GetEnumValue("Blue");
Color color = rtti::cast<Color>(rtti::Box(2), nullptr);            // 整数 -> 枚举
```

内置数值类型之间（含 `bool`、字符类型、`long double`）由类型系统统一做数值转换，不需要逐对注册。

### 名称、别名与继承

- 编译期类型默认使用**全限定名**（例如 `halo::Transform`、`test_ns::Base`），避免命名空间同名冲突。
- 注册时传入的名字是**显式别名**：`TypeRegister<T>::New("Transform")` 会使 `Type::Find("Transform")` 可用；若该名字已属于另一个类型则抛 `DuplicateTypeName`。
- 库会尽力为全限定名添加唯一短名别名（例如 `Base`），仅用于兼容旧数据；短名冲突时不会覆盖，使用全限定名即可。
- `GetName()` 返回规范名（全限定名/动态类型名），`GetShortName()` 返回短名；
  `GetSerializationName()` 在短名唯一时返回短名（兼容旧存档），否则返回规范名。
  新增代码建议使用 `GetName()` 作为稳定身份、`GetSerializationName()` 作为可读/序列化标识。

### 错误处理

```cpp
try
{
    rtti::type_of<MyClass>()->GetProperty("Value")->SetValue(object, rtti::Box("bad"));
}
catch (const rtti::Error& error)
{
    rtti::ErrorCode code = error.Code(); // rtti::ErrorCode::ArgumentType 等
}
```

- 开启异常（默认）：反射 API 在误用时抛出 `rtti::Error`；被调用目标自身抛出的异常包装为 `rtti::TargetInvocationError`，可通过 `Inner()` 取回 `std::exception_ptr`。
- `-fno-exceptions` / `-DRTTI_NO_EXCEPTIONS`：可恢复错误会设置线程局部的 `rtti::GetLastError()` 并返回默认值/`nullptr`；注册冲突等编程错误走 `abort`。
- 需要"不抛异常"的调用使用 `Try*` API：

```cpp
rtti::ObjectPtr result;
if (method->TryInvoke(object, { rtti::Box(1) }, result))
{
    // ...
}
int value = 0;
if (!property->TryGetValue(object, value))
{
    // rtti::GetLastError().code
}
if (!property->TrySetValue(object, 5))
{
}
```

`ClearLastError()` 用于清除上一次错误。

## 动态类型（DynamicType）

`DynamicType` 在运行时由 schema 创建类型，实例为 `DynamicObject`，属性存储在动态 map 中。

```cpp
#include <DynamicType.h>

std::map<std::string, rtti::DynamicPropertyDesc> schema;
schema["x"] = rtti::type_of<float>();
schema["y"] = rtti::type_of<float>();
schema["name"] = rtti::type_of<std::string>();

auto* vectorType = rtti::NewDynamicType("DynVector3", schema);
auto object = vectorType->CreateInstance({});

vectorType->GetProperty("x")->SetValue(object, rtti::Box(1.0f));
float x = vectorType->GetProperty("x")->GetValue<float>(object);
```

### 动态方法

```cpp
auto* add = rtti::NewDynamicMethod(
    nullptr, "Add", rtti::type_of<int>(),
    { { rtti::type_of<int>(), false, false },
      { rtti::type_of<int>(), false, false } },
    [](const rtti::ObjectPtr&, const std::vector<rtti::ObjectPtr>& args) -> rtti::ObjectPtr
    {
        return rtti::Box(rtti::Unbox<int>(args[0]) + rtti::Unbox<int>(args[1]));
    });

auto* calculator = rtti::NewDynamicType("Calculator", { { "value", rtti::type_of<int>() } }, { add });
auto instance = calculator->CreateInstance({});
int sum = rtti::Unbox<int>(calculator->GetMethod("Add")->Invoke(instance, { rtti::Box(10), rtti::Box(20) }));
```

`NewDynamicType` 会把动态方法的 owner 绑定到新类型；也可以之后通过 `AttachMethod` / `AttachProperty` / `AttachConstructor` 附加。

### 继承动态类型

```cpp
auto* entity = rtti::NewDynamicType("Entity", { { "id", rtti::type_of<int>() } });
auto* player = rtti::NewDynamicType("Player", { { "hp", rtti::type_of<int>() } }, {}, {}, entity);

player->IsSubclassOf(entity);          // true
player->GetProperty("id") != nullptr;  // 继承属性
```

### 继承编译期类型（组合适配器）

动态类型可以组合一个编译期基类。该基类必须**可默认构造**；`DynamicObject` 内部持有一个真实的基类实例，继承属性的读写、继承方法的调用、以及 `cast<Base>(dynamicObject)` 都会路由到该实例，而不是做非法的指针强转。

```cpp
class Actor : public rtti::Object
{
    RTTI_TYPE_DECLARE(rtti::Object)
public:
    Actor() = default;
    std::string Name;
    int Bump(int by) { return m_value += by; }
private:
    int m_value = 0;
};

rtti::TypeRegister<Actor>::New()
    .constructor<>()
    .property("Name", &Actor::Name)
    .method("Bump", &Actor::Bump);

auto* type = rtti::NewDynamicType("DynActor", { { "extra", rtti::type_of<float>() } }, {}, {}, rtti::type_of<Actor>());
auto object = type->CreateInstance({});

type->GetProperty("Name")->SetValue(object, rtti::Box(std::string("hero")));
rtti::Ptr<Actor> actor = rtti::cast<Actor>(object);  // 指向内部组合的 Actor 实例
```

若基类没有默认构造，`NewDynamicType` 会抛出 `InvalidDynamicType`（无异常构建下为编程错误 abort）。

## 类型注册表与生命周期

- `TypeRegistry` 独占拥有所有 `Type`；`Type` 独占拥有构造函数/方法/属性元数据，不再泄漏。
- 类型注册在 `Freeze()` 前可重复调用；注册是幂等的（相同签名不会重复添加）。
- `Type::ForEach` 使用快照遍历，回调中注册新类型不会导致无限循环。
- `Type::GetRegistry()` 不再公开可变 map；使用 `Type::Find`、`Type::ForEach`、`Type::GetTypes`。

## 自定义配置

### 自定义智能指针

在包含任何 RTTI 头文件**之前**定义：

```cpp
#define RTTI_PTR_ALIAS(T)       my::IntrusivePtr<T>
#define RTTI_CUSTOM_POINTER_OPS

namespace rtti::detail
{
template <typename T>
struct pointer_ops
{
    using PtrT = RTTI_PTR_ALIAS(T);

    template <typename U = T, typename... Args>
    static RTTI_PTR_ALIAS(U) make(Args&&... args)
    {
        return my::MakeIntrusive<U>(std::forward<Args>(args)...);
    }

    template <typename U>
    static RTTI_PTR_ALIAS(U) ptr_cast(const PtrT& p)
    {
        return my::static_pointer_cast<U>(p);
    }

    static T* to_raw(const PtrT& p) noexcept { return p.get(); }

    static PtrT from_raw(T* raw) { return PtrT(raw); }
};
} // namespace rtti::detail

#include <RTTI.h>
```

要求：`make`、`ptr_cast`、`to_raw`、`from_raw`。所有翻译单元必须使用同一定义（否则 ODR 违规）。

### 自定义 Object 基类

```cpp
#define RTTI_OBJECT_BASE MyRefCountBase
#include <RTTI.h>
```

`MyRefCountBase` 必须在该包含之前可见，并提供多态删除所需的虚析构。

## 构建与测试

```bash
cmake -S . -B build -DRTTI_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

测试目标为 `rtti_test`。CI 覆盖 Debug/Release、macOS/Linux/Windows、ASan/UBSan、TSan、`-fno-rtti`、`-fno-exceptions`，以及安装后的 `find_package` 消费。

## 命名约定

- 类型、结构体、成员函数、枚举：`PascalCase`
- 成员变量：`m_camelCase`
- 宏：`RTTI_` 前缀（`RTTI_TYPE_DECLARE`、`RTTI_HASH`、`RTTI_PTR_ALIAS` 等）
- 自由函数/模板助手：`snake_case`（`type_of`、`cast`、`convert`、`compare`、`hash_string`）
- 访问器统一 `Get*`（`GetName`、`GetOwnerType`、`GetReturnType`、`GetPropertyType`、`GetParameters`）

## 已知限制

- 反射调用只支持函数指针、成员指针和无捕获 lambda；有捕获 lambda 会在编译期被拒绝。
- 不支持可变参数函数、`noexcept`/引用限定成员函数的自动注册、默认参数。
- 值类型到值类型的反射转换要求目标类型可构造或存在 `ConvertTo`/回调；复杂跨类型转换仍建议显式注册。
- 动态属性按字符串键存储；同名属性在不同动态类型之间共享对象内的键空间，属性访问会校验所属类型。
- 类型系统不提供跨共享库（DSO）的注册表合并语义；每个进程/可见性域应保持一致。
- `nameof.hpp` 是 vendored 的 Neargye nameof 0.10.3（MIT），用于编译期类型名。

## 许可证

MIT，见 [LICENSE](LICENSE)。
