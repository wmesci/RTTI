#include <cassert>
#define RTTI_ENABLE_LOG 1
#include "src/RTTI.h"

using namespace std::string_literals;

using namespace rtti;

enum class TestEnum
{
    Value1,
    Value2
};

enum class TestEnum2
{
    Value1,
    Value2
};

struct TestStruct
{
    TestEnum TE = TestEnum::Value1;

    operator int() const
    {
        return 250;
    }

    void Func(int a)
    {
        printf("TestStruct::Func %d - %d\n", a, TE);
        TE = (TestEnum)a;
    }
};

class TestBase : public Object
{
    TYPE_DECLARE(TestBase, Object)
public:
    TestBase() { printf("ctor()\n"); }

    int32_t TestBaseA = 1;

    virtual void Func1()
    {
        printf("void TestBase::Func1()\n");
    }

    void BaseFunc2() const
    {
        printf("void BaseFunc2() const\n");
    }

    int32_t BaseFunc3()
    {
        printf("int BaseFunc3()\n");
        return 34;
    }

    std::shared_ptr<TestBase> BaseFunc4() const
    {
        printf("ObjectPtr BaseFunc4() const\n");
        return std::make_shared<TestBase>();
    }

    void BaseFunc5(int a)
    {
        printf("void BaseFunc5(int %d)\n", a);
    }

    // void BaseFunc6(int& a)
    //{
    //     printf("void BaseFunc6(int& %d)\n", a);
    //     a = 31415926;
    // }

    void BaseFunc7(const ObjectPtr& a) const
    {
        printf("void BaseFunc7(const ObjectPtr& [%s]) const\n", a->GetRttiType()->GetName().c_str());
    }
};

class Test : public TestBase
{
    TYPE_DECLARE(Test, TestBase)
public:
    Test() { printf("ctor()\n"); }
    Test(int i) { printf("ctor(%d)\n", i); }
    Test(int i, float f) { printf("ctor(%d, %f)\n", i, f); }

    int32_t A = 1;
    std::string B;
    ObjectPtr C;
    TestEnum D = TestEnum::Value1;
    const TestStruct E;

    int GetProp1()
    {
        printf("void GetProp1()\n");
        return 0;
    }

    void SetProp1(int v)
    {
        printf("void SetProp1()\n");
    }

    int GetProp2()
    {
        printf("void GetProp2()\n");
        return 0;
    }

    int GetProp3() const
    {
        printf("void GetProp3()\n");
        return 0;
    }

    virtual void Func1() override
    {
        printf("void Test::Func1()\n");
    }

    void Func2() const
    {
        printf("void Func2() const\n");
    }

    int Func3()
    {
        printf("int Func3()\n");
        return 34;
    }

    ObjectPtr Func4() const
    {
        printf("ObjectPtr Func4() const\n");
        return std::make_shared<Test>();
    }

    void Func5(int a)
    {
        printf("void Func5(int %d)\n", a);
    }

    void Func6(const std::shared_ptr<int>& a)
    {
        printf("void Func6(const std::shared_ptr<int>& %d)\n", *a);
    }

    void Func7(const std::shared_ptr<TestBase>& a) const
    {
        printf("void Func7(const ObjectPtr& [%s]) const\n", a->GetRttiType()->GetName().c_str());
    }

    void Func8(TestEnum a)
    {
        printf("void Func8(TestEnum %d)\n", a);
    }

    template <typename T>
    bool ConvertTo(T& target);

    template <typename T>
    bool ConvertTo(std::shared_ptr<T>& target);
};

template <>
bool Test::ConvertTo(int& target)
{
    target = 128;
    return true;
}

struct HandleBase
{
public:
    HandleBase() = default;

    HandleBase(const ObjectPtr& p)
        : m_ptr(p)
    {
    }

    ObjectPtr GetPtr() const
    {
        return m_ptr;
    }

protected:
    ObjectPtr m_ptr;
};

template <typename T>
struct Handle : public HandleBase
{
public:
    Handle() = default;

    Handle(const std::shared_ptr<T>& p)
        : HandleBase(p)
    {
    }

    template <typename U>
    Handle(const Handle<U>& h)
        : HandleBase(h.GetPtr())
    {
    }

    T* operator->() const
    {
        return static_cast<T*>(m_ptr.get());
    }

    operator std::shared_ptr<T>() const
    {
        return std::static_pointer_cast<T>(m_ptr);
    }
};

void print(MethodBase* m)
{
    if (m->ReturnType() == nullptr)
        printf("void ");
    else if (m->ReturnType()->IsValueType())
        printf("%s ", m->ReturnType()->GetName().c_str());
    else
        printf("std::shared_ptr<%s> ", m->ReturnType()->GetName().c_str());

    printf("%s(", m->GetName().c_str());
    for (size_t i = 0; i < m->GetParameters().size(); i++)
    {
        auto p = m->GetParameters()[i];
        if (p.IsConst)
            printf("const ");

        if (p.Type->IsValueType())
            printf("%s", p.Type->GetName().c_str());
        else
            printf("std::shared_ptr<%s>", p.Type->GetName().c_str());

        if (p.IsRef)
            printf("&");
        if (i < m->GetParameters().size() - 1)
            printf(", ");
    }
    printf(")");
}

void RegisterTypes()
{
    InitCoreType();

    TypeRegister<TestEnum>::New("TestEnum"s, {{HASH("displayName"), "TestEnumForDisplay"s}})
        .value("Value1", TestEnum::Value1)
        .value("Value2", TestEnum::Value2);

    TypeRegister<TestStruct>::New("TestStruct"s)
        .convert<int>()
        .property("TE"s, &TestStruct::TE)
        .method("Func"s, &TestStruct::Func);

    TypeRegister<TestBase>::New("TestBase"s)
        .property("TestBaseA"s, &TestBase::TestBaseA)
        .method("Func1"s, &TestBase::Func1)
        .method("BaseFunc2"s, &TestBase::BaseFunc2)
        .method("BaseFunc3"s, &TestBase::BaseFunc3)
        .method("BaseFunc4"s, &TestBase::BaseFunc4)
        .method("BaseFunc5"s, &TestBase::BaseFunc5)
        //.method("BaseFunc6"s, &TestBase::BaseFunc6)
        .method("BaseFunc7"s, &TestBase::BaseFunc7);

    TypeRegister<Test>::New("Test"s)
        .constructor<>()
        .constructor<int>()
        .constructor<int, float>()
        .convert<int>()
        .property("A"s, &Test::A, {{HASH("clonable"), true}, {HASH("min_value"), 100}, {HASH("max_value"), 100}})
        .property("B"s, &Test::B)
        .property("C"s, &Test::C)
        .property("D"s, &Test::D)
        .property("E"s, &Test::E)
        .property("Prop1"s, &Test::GetProp1, &Test::SetProp1)
        .property("Prop2"s, &Test::GetProp2)
        .property("Prop3"s, &Test::GetProp3)
        .method("Func2"s, &Test::Func2)
        .method("Func3"s, &Test::Func3)
        .method("Func4"s, &Test::Func4)
        .method("Func5"s, &Test::Func5)
        .method("Func6"s, &Test::Func6)
        .method("Func7"s, &Test::Func7)
        .method("Func8"s, &Test::Func8);

    TypeRegister<Handle<TestBase>>::New("Handle<TestBase>"s, {{HASH("handle"), type_of<TestBase>()}})
        .constructor<std::shared_ptr<TestBase>>()
        .convert<HandleBase>()
        .convert<TestBase>();

    TypeRegister<Handle<Test>>::New("Handle<Test>"s, {{HASH("handle"), type_of<Test>()}})
        .constructor<std::shared_ptr<Test>>()
        .convert<HandleBase>()
        .convert<Test>();
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    RegisterTypes();

    printf("%s - %d - %s\n", type_of<int*>()->GetName().c_str(), type_of<int*>()->IsPointer(), type_of<int*>()->GetUnderlyingType()->GetName().c_str());

    assert(type_of<TestEnum2>()->IsEnum());
    assert(type_of<TestEnum2>()->CanConvertTo<int32_t>());

    assert(type_of<Test>() == type_of<std::shared_ptr<Test>>());
    assert(type_of<Test>()->CanConvertTo<int32_t>());

    Handle<TestBase> ht = Handle<Test>(std::make_shared<Test>());

    assert(ht->GetRttiType() == type_of<Test>());

    ObjectPtr box_ht = rtti::Box(ht);
    printf("box_ht --> %s", box_ht->GetRttiType()->GetName().c_str());

    Type* inner_type = box_ht->GetRttiType()->GetAttribute<rtti::Type*>(HASH("handle"));
    assert(inner_type == type_of<TestBase>());

    auto ht2 = cast<HandleBase>(box_ht).GetPtr();
    assert(ht2 == ht.GetPtr());

    assert(type_of<TestEnum>()->GetAttribute<std::string>(HASH("displayName")) == "TestEnumForDisplay"s);

    assert(type_of<int>() == Box(123)->GetRttiType());

    auto type = Type::Find("Test"s);

    assert(type->GetProperty("A")->HasAttribute(HASH("clonable")));
    assert(!type->GetProperty("B")->HasAttribute(HASH("clonable")));

    auto obj = type->Create<Test>();
    auto obj1 = type->Create<Test>(123);
    auto obj2 = type->Create<Test>(123, 789.12f);

    assert(rtti::cast<Test>((TestBase*)obj.get()) == obj.get());
    assert(rtti::cast<Test*>((TestBase*)obj.get()) == obj.get());

    ObjectPtr ttt;
    Type::Convert(obj, type_of<int>(), ttt);

    assert(cast<int>(ttt) == 128);
    assert(cast<uint16_t>(ttt) == 128);
    assert(abs(cast<double>(ttt) - 128.0) <= 0.0001);
    auto comparable_enum_int = is_comparable<TestEnum, int>();
    assert(comparable_enum_int);
    auto comparable_int_enum = is_comparable<int, TestEnum>();
    assert(comparable_int_enum);
    auto comparable_int_double = is_comparable<int, double>();
    assert(comparable_int_double);
    auto comparable_int_string = is_comparable<int, std::string>();
    assert(!comparable_int_string);
    assert(compare(ttt, 128.0f) == CompareResult::Equals);
    assert(compare(0u, ttt) == CompareResult::NotEquals);
    assert(compare(ttt, std::string("xxx")) == CompareResult::Failed);
    assert(ttt->GetHashCode() == std::hash<int>()(128));

    assert(cast<TestBase>(obj) != nullptr);
    assert(cast<Test>(cast<TestBase>(obj)) != nullptr);

    obj->A = 123;
    obj->B = "str"s;
    obj->C = obj1;
    obj->D = TestEnum::Value1;

    obj1->A = 255;
    obj1->B = "测试字符串！"s;
    obj1->C = obj2;
    obj1->D = TestEnum::Value2;

    printf("Type = %s\n", type->GetName().c_str());

    printf("\033[31m [Constructors] \033[0m\n");
    for (auto ctor : type->GetConstructors())
    {
        printf("    ");
        print(ctor);
        printf("\n");
    };

    printf("\033[31m [Methods] \033[0m\n");
    for (auto m : type->GetMethods())
    {
        printf("    ");
        print(m);
        printf("\n");
    }

    printf("\033[31m [Properties] \033[0m\n");
    for (auto p : type->GetProperties())
    {
        printf("    ");
        if (p->CanWrite())
            printf("RW ");
        else
            printf("R  ");
        if (p->PropertyType()->IsValueType())
            printf("%s %s", p->PropertyType()->GetName().c_str(), p->GetName().c_str());
        else
            printf("std::shared_ptr<%s> %s", p->PropertyType()->GetName().c_str(), p->GetName().c_str());
        printf("\n");
    }

    printf("\n");
    printf("\033[31m [Test] \033[0m\n");

    type->GetMethod("Func1"s)->Invoke(obj, {});
    type->GetMethod("Func2"s)->Invoke(obj, {});
    printf("  ret = %d\n", Unbox<int>(type->GetMethod("Func3"s)->Invoke(obj, {})));
    printf("  ret = %s\n", type->GetMethod("Func4"s)->Invoke(obj, {})->GetRttiType()->GetName().c_str());
    type->GetMethod("Func5"s, {type_of<int>()})->Invoke(obj, {Box(TestEnum::Value1)});
    type->GetMethod("Func5"s, {type_of<int>()})->Invoke(obj, {Box(TestStruct())});
    type->GetMethod("Func5"s, {type_of<int>()})->Invoke(obj, {std::make_shared<Test>()});
    type->GetMethod("Func6"s, {type_of<std::shared_ptr<int>>()})->Invoke(obj, {Box(std::make_shared<int>(135))});
    type->GetMethod("Func7"s, {type_of<TestBase>()})->Invoke(obj, {obj});
    type->GetMethod("Func8"s, {type_of<TestEnum>()})->Invoke(obj, {Box(123)});

    TestStruct testStruct;
    type_of<TestStruct>()->GetMethod("Func"s)->Invoke(Box(testStruct), {Box(3)});
    type_of<TestStruct>()->GetMethod("Func"s)->Invoke(Box(&testStruct), {Box(4)});

    assert(testStruct.TE == (TestEnum)4);

    printf("\n");

    return 0;
}