# RTTI

RTTI 是一个参考 C# 反射机制实现的 C++ 运行时类型信息和反射库，提供了类似 C# 的反射 API，使得 C++ 能够在运行时获取和操作类型信息。

## 特性

* 🚀 类似 C# 的反射 API
* 💡 运行时类型信息查询
* 🔄 类型转换和比较
* 📦 属性和方法的反射
* 🛠 构造函数的反射
* 🔍 基类和继承关系查询
* ⚡️ 头文件优先设计
* 🎯 仅静态库，易于集成
* 🧩 支持自定义 Object 基类和智能指针类型

## 要求

* C++20 或更高版本
* CMake 3.14 或更高版本

## 安装与集成

### 通过 CMake FetchContent（推荐）

```cmake
include(FetchContent)
FetchContent_Declare(
    RTTI
    GIT_REPOSITORY https://github.com/wmesci/RTTI.git
    GIT_TAG main  # 或指定版本标签
)
FetchContent_MakeAvailable(RTTI)

target_link_libraries(YourTarget PRIVATE rtti::rtti)
```

### 通过安装使用

```bash
git clone https://github.com/wmesci/RTTI.git
cd RTTI
mkdir build && cd build
cmake ..
make
sudo make install
cmake
find_package(RTTI REQUIRED)
target_link_libraries(YourTarget PRIVATE rtti::rtti)
```

## 快速开始

### 引用类型（继承自 Object）

```cpp
#include <RTTI/RTTI.h>
#include <RTTI/Object.h>

class MyClass : public rtti::Object
{
    TYPE_DECLARE(rtti::Object) // 声明基类
public:
    MyClass(int value) : value_(value) {}
  
    int GetValue() const { return value_; }
    void SetValue(int v) { value_ = v; }
  
private:
    int value_;
};

// 注册类型和成员
TypeRegister<MyClass>::New("MyClass")
    .constructor<int>()  // 注册构造函数
    .property("Value", &MyClass::GetValue, &MyClass::SetValue)  // 注册属性
    .method("SetValue", &MyClass::SetValue);  // 注册方法
```

### 值类型

```cpp
struct Vector3
{
    float x, y, z;
  
    float GetLength() const { return std::sqrt(x*x + y*y + z*z); }
};

// 注册值类型
TypeRegister<Vector3>::New("Vector3")
    .constructor<>()  // 默认构造函数
    .property("x", &Vector3::x)
    .property("y", &Vector3::y)
    .property("z", &Vector3::z)
    .method("GetLength", &Vector3::GetLength);
```

## 使用反射

```cpp
// 创建对象
auto obj = rtti::create<MyClass>(42);

// 获取类型信息
Type* type = obj->GetRttiType();
std::cout << "Type name: " << type->GetName() << std::endl;

// 获取和设置属性
auto prop = type->GetProperty("Value");
if (prop)
{
    int value = 0;
    prop->Get(obj, value);  // 获取属性值
    std::cout << "Value: " << value << std::endl;
  
    prop->Set(obj, 100);    // 设置属性值
}

// 调用方法
auto method = type->GetMethod("SetValue");
if (method)
{
    method->Invoke(obj, 200);  // 调用方法
}
```

### 类型转换

```cpp
bool ok = false;
auto derived = rtti::cast<DerivedClass>(baseObj, &ok);
if (ok) {
    // 转换成功
}
```

## 进阶用法

### 添加特性（Attributes）

```cpp
TypeRegister<MyClass>::New("MyClass")
    .constructor<int>()
    .property("Value", &MyClass::GetValue, &MyClass::SetValue, {
        {HASH("min_value"), 0},
        {HASH("max_value"), 100}
    });

// 获取特性
rtti::PropertyInfo* prop = // 获取属性信息;
if (prop->HasAttribute(HASH("min_value")) && prop->HasAttribute(HASH("max_value")))
{
    int min_value = std::any_cast<int>(prop->GetAttribute(HASH("min_value")));
    int max_value = std::any_cast<int>(prop->GetAttribute(HASH("max_value")));
}
```

### 自定义类型转换

```cpp
TypeRegister<MyClass>::New("MyClass")
    .convert<int>()  // 允许 MyClass 转换为 int
    .convert<std::string>();  // 允许 MyClass 转换为 string
```

### 动态类型

`DynamicType` 允许在运行时通过 map schema 创建 Type，无需编译期注册。适合脚本绑定、数据驱动设计等场景。

```cpp
#include <RTTI/RTTI.h>
#include <RTTI/DynamicType.h>

// 1. 定义 schema（属性名 → 属性类型）
std::map<std::string, Type*> schema;
schema["x"]     = type_of<float>();
schema["y"]     = type_of<float>();
schema["name"]  = type_of<std::string>();

// 2. 注册动态类型
auto* vec3Type = NewDynamicType("DynamicVector3", schema);

// 3. 创建实例
auto obj = vec3Type->CreateInstance({});

// 4. 通过反射 API 读写属性
vec3Type->GetProperty("x")->SetValue(obj, Box(1.0f));
vec3Type->GetProperty("y")->SetValue(obj, Box(2.0f));
float x = vec3Type->GetProperty("x")->GetValue<float>(obj);  // 1.0f
```

#### 子类继承

动态类型可以继承另一个 Type（动态或编译期），自动获得父类的属性和方法：

```cpp
// 父类：动态 Entity
auto* entityType = NewDynamicType("Entity",
    {{"id", type_of<int>()}});

// 子类：动态 Player，继承 Entity
auto* playerType = NewDynamicType("Player",
    {{"hp", type_of<int>()}},   // 自有属性
    {},                          // 方法
    {},                          // attributes
    entityType);                 // baseType

// 子类自动获得父类属性
playerType->GetProperty("id");  // 从 Entity 继承
playerType->GetProperty("hp");  // Player 自有

// IsSubClassOf 检查
playerType->IsSubClassOf(entityType);  // true
```

#### 动态方法

```cpp
auto method = NewDynamicMethod(
    nullptr, "Add", type_of<int>(),
    {{type_of<int>(), false, false}},
    [](const ObjectPtr&, const std::vector<ObjectPtr>& args) -> ObjectPtr {
        return Box(Unbox<int>(args[0]) + Unbox<int>(args[1]));
    });

auto* calcType = NewDynamicType("Calculator",
    {{"value", type_of<int>()}}, {method});

auto obj = calcType->CreateInstance({});
auto result = calcType->GetMethod("Add")->Invoke(obj, {Box(10), Box(20)});
int sum = Unbox<int>(result);  // 30
```

#### DynamicObject 辅助 API

```cpp
auto obj = vec3Type->CreateInstance({});
auto* dyn = static_cast<DynamicObject*>(RawFromPtr(obj));
dyn->Set("x", Box(5.0f));
ObjectPtr v = dyn->Get("x");  // Box(5.0f)
```

> **注意**：动态类型在 map 中查找属性为 O(log n)。属性读写不经过编译期 getter/setter（`DynamicPropertyInfo` 直接操作 `DynamicObject` 内部 map）。工厂函数返回的对象为 process-lifetime（与 Type 一致的 intentional leak 模式）。

### 类型系统

RTTI 库支持两种对象类型：

#### 引用类型

* 继承自 rtti::Object
* 使用引用语义（指针、智能指针等）
* 变量赋值后多个变量指向同一对象

#### 值类型

* 不需要继承特定基类
* 使用值语义
* 变量赋值后成为独立实例

### 类型操作

#### 获取类型信息

```cpp
// 通过类型名称查找
rtti::Type* type = rtti::Type::Find("MyClass");

// 使用 type_of
rtti::Type* type = rtti::type_of<int>();
rtti::type_of<MyClass>() == rtti::type_of<std::shared_ptr<MyClass>>();

// 获取对象的实际类型
std::shared_ptr<rtti::Object> obj(new MyClass(123));
obj->GetRttiType() == rtti::type_of<MyClass>();
```

#### 类型转换机制

`rtti::cast` 方法支持多种转换方式：

* 直接隐式转换（子类转父类、int 转 float 等）
* 通过 RTTI 进行父类转子类转换
* 使用转换构造函数
* 使用转换器（引用类型的 ConvertTo 方法或值类型的转换运算符）

## 自定义配置

RTTI 支持自定义基类和智能指针类型，以适应不同的项目需求。

### 自定义智能指针类型

RTTI 默认使用 `std::shared_ptr<T>`。如需使用自定义智能指针（如 `IntrusivePtr`），在包含 RTTI 头文件**之前**：

1. 定义 `RTTI_PTR_ALIAS(T)` 宏指定指针类型
2. 定义 `RTTI_CUSTOM_POINTER_OPS` 宏跳过默认实现
3. 在 `namespace rtti::detail` 中提供自定义的 `pointer_ops<T>` 主模板

注意：由于 `rtti::Ptr<T>` 此时尚未定义，返回值类型需使用 `RTTI_PTR_ALIAS(U)` 宏。

```cpp
// 在包含 RTTI 头文件之前
#define RTTI_PTR_ALIAS(T)       my::IntrusivePtr<T>
#define RTTI_CUSTOM_POINTER_OPS

namespace rtti::detail {
template <typename T>
struct pointer_ops {
    using PtrT = RTTI_PTR_ALIAS(T);

    template <typename U = T, typename... Args>
    static RTTI_PTR_ALIAS(U) make(Args&&... args) {
        return my::MakeIntrusive<U>(std::forward<Args>(args)...);
    }

    template <typename U>
    static RTTI_PTR_ALIAS(U) ptr_cast(const PtrT& p) {
        return my::static_pointer_cast<U>(p);
    }

    static T* to_raw(const PtrT& p) noexcept { return p.get(); }
    static PtrT from_raw(T* raw) { return PtrT(raw); }
};
} // namespace rtti::detail

#include <RTTI/RTTI.h>
```

### 自定义 Object 基类

默认情况下 RTTI 使用 `std::enable_shared_from_this<Object>` 作为 Object 基类。如需替换，定义 `RTTI_OBJECT_BASE` 宏：

```cpp
#define RTTI_OBJECT_BASE MyCustomBase
#include <RTTI/RTTI.h>
```

### 自定义指针的操作要求

自定义智能指针类型需在 `pointer_ops` 中实现以下操作：

- `make<U>(args...)` — 创建智能指针实例
- `ptr_cast<U>(p)` — 智能指针类型转换（如 `static_pointer_cast`）
- `to_raw(p)` — 从智能指针获取原始指针
- `from_raw(raw)` — 从原始指针获取智能指针（注意生命周期）

> **注意**：所有翻译单元中的自定义定义必须保持一致，否则会导致 ODR 违规（程序 ill-formed, no diagnostic required）。类型注册（`type_of<T>()`、`TypeRegister::New()`）应在单线程启动阶段完成。

## 最佳实践

* 使用 TYPE_DECLARE 宏声明基类关系
* 在全局命名空间中注册类型
* 属性名使用 PascalCase 命名风格
* 使用 HASH 宏优化字符串查找性能
* 优先使用值类型处理小型对象

## 构建与测试

```bash
mkdir build && cd build
cmake .. -DRTTI_BUILD_TEST=ON
make
./RTTI-Test  # 运行测试
```

## 许可证

MIT License
