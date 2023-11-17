RTTI 库是参考了 C# 语言里的反射而实现的一套 C++ 运行时反射库

在 RTTI 库里，对象分为两类：
* **引用类型**：以引用语义（指针、智能指针、Handle 等）的方式使用类型，声明变量、传递参数的时候以指针的方式进行，变量赋值后有两个变量指向同一个对象。在新 RTTI 库里，引用类型需要是 `rtti::Object` 类型的子类
* **值类型**：以值语义的方式使用类型，变量赋值后变成了两个独立的实例。在新 RTTI 库里不限制其继承关系，但不允许以指针的方式使用

## 注册类型
对于引用类型
```cpp
class MyClass : rtti::Object
{
TYPE_DECLARE(MyClass, rtti::Object) // 声明当前类名以及其基类
public:
    MyClass(int v);
 
    int IntField;
 
    int GetProp1() const;
    void SetProp1(int v);
 
    int GetProp2() const;
 
    void Func(float v);
 
    bool ConvertTo(int& target);
};
 
TypeRegister<MyClass>::New("MyClass")
    .constructor<int>() // 构造函数，模版参数为构造函数的参数类型
    .convert<int>() // 转换器，表示当前类型可以转换为 int，内部会使用 MyClass::ConvertTo 方法
    .property("IntField", &MyClass::IntField, {{HASH("min_value"), 0}, {HASH("max_value"), 100}})
    .property("Prop1", &MyClass::GetProp1, &Test::SetProp1)
    .property("Prop2", &MyClass::GetProp2)
    .method("Func", &MyClass::Func2);
```

对于值类型
```cpp
class Vector3
{
public:
    float x, y, z;
 
    Vector3();
 
    Vector3(float x, float y, float z);
 
    float GetLength() const;
 
    operator Vector2() const;
};
 
TypeRegister<Vector3>::New("Vector3")
    .constructor<>() // 构造函数，模版参数为构造函数的参数类型
    .constructor<float, float, float>()
    .convert<Vector2>() // 转换器，表示当前类型可以转换为 Vector2，内部使用单参数构造函数和转换运算符
    .property("x", &Vector3::x)
    .property("y", &Vector3::y)
    .property("z", &Vector3::z)
    .method("Length", &Vector3::GetLength);
```

## 获取类型
```cpp
// 通过类型名称查找类型
rtti::Type* type = rtti::Type::Find("MyClass");
 
// 使用 type_of
rtti::Type* type = rtti::type_of<int>(); // 获取 int 类型的 Type* 对象
rtti::type_of<MyClass>() == rtti::type_of<std::shared_ptr<MyClass>>();
rtti::type_of<MyClass>() == rtti::type_of<std::weak_ptr<MyClass>>();
rtti::type_of<void>() == nullptr;
 
// 对于引用类型，可以使用 GetRttiType 方法获取其实际类型
std::shared_ptr<rtti::Object> obj(new MyClass(123));
obj->GetRttiType() == rtti::type_of<MyClass>();
```

## 类型转换
RTTI 库提供了 rtti::cast 方法用于在各种类型之间进行转换
* 对于能够直接隐式转换的类型（子类转父类、int 转 float 等），cast 方法等同于直接赋值，没有任何额外的开销
* 对于父类转子类，cast 方法内部会通过 RTTI 获取实际类型并判断是否能够进行转换
* 如果类型不匹配，不能直接转换，cast 会尝试查找目标类型里合适的单参数构造函数（转换构造函数），并使用传入的变量进行调用
* 如果没有合适的转换构造函数，则会尝试查找可用的转换器（对于引用类型是 ConvertTo 方法，对于值类型则是类型转换运算符）
* cast 会通过 pOK 参数返回转换结果
```cpp
// 将当前类型转换成指定类型
// 直接转换：
//   Ptr<Subclass>  -->  Ptr<Base>
//   Subclass*      -->  Base*
//   int            -->  float
// Unbox：
//   ObjectPtr --> ValueType / ValueType*
// Box：
//   ValueType --> ObjectPtr
template <class To, class From>
auto cast(const From& from, bool* pOK);
```
 

## 获取类型信息、创建实例、调用方法/属性...
```cpp
rtti::Type* type = rtti::Type::Find("MyClass");
rtti::ObjectPtr obj = type->Create<int>(123);
type->GetMethod("Func")->Invoke(obj, { rtti::cast<rtti::ObjectPtr>(1.0f) });  // 调用方法
type->GetProperty("Prop1")->SetValue(obj, rtti::cast<rtti::ObjectPtr>(123)); // 设置属性
```

同时，RTTI 库还支持 Attribute 机制，可以给包括类型、属性、方法打上一些标签，并在运行时获取相关信息。例如上面注册 MyClass 类的 IntField 属性的时候，后面的 `{{HASH("min_value"), 100}, {HASH("max_value"), 100}}` 就是 Attribute，这里给 IntField 属性添加了两个 Attribute，分别是属性的最大值和最小值
```cpp
.property("IntField", &MyClass::IntField, {{HASH("min_value"), 0}, {HASH("max_value"), 100}})
```

后续在可以这么使用：

```cpp
rtti::PropertyInfo* prop = ...
if (prop->HasAttribute(HASH("min_value")) && prop->HasAttribute(HASH("max_value")))
{
    int min_value = std::any_cast<int>(prop->GetAttribute(HASH("min_value")));
    int max_value = std::any_cast<int>(prop->GetAttribute(HASH("max_value")));
}
```

> Attribute 实际上是一些键值对：
> * key 是 size_t 类型，上面代码用了 HASH(编译时字符串 Hash ) 的方式生成 key 是为了提供更好的可读性，也可以直接填数字
> * value 是 C++17 里的 std::any 类型，用于存放任意值，后续可以使用 any_cast 转换回原始的类型。这里建议不要存放过于复杂的类型，通常一些编译时常量以及 rtti::Type* 是安全的。
