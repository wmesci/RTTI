#include <cassert>
#include "RTTI.h"
#include "TypeRegister.h"

using namespace rtti;

enum class TestEnum
{
    Value1,
    Value2
};

struct TestStruct
{
    TestEnum TE = TestEnum::Value2;
};

class TestBase : public Object
{
    TYPE_DECLARE(TestBase, Object)
public:
    TestBase() { printf("ctor()\n"); }

    int32_t TestBaseA = 1;

    void BaseFunc1()
    {
        printf("void BaseFunc1()\n");
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

    void BaseFunc6(int& a)
    {
        printf("void BaseFunc6(int& %d)\n", a);
        a = 31415926;
    }

    void BaseFunc7(const ObjectPtr& a) const
    {
        printf("void BaseFunc7(const ObjectPtr& [%s]) const\n", a->GetType()->GetName().c_str());
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
        printf("void Func1()\n");
    }

    void SetProp1(int v)
    {
        printf("void Func2() const\n");
    }

    int GetProp2()
    {
        printf("void Func1()\n");
    }

    int GetProp3() const
    {
        printf("void Func1()\n");
    }

    void Func1()
    {
        printf("void Func1()\n");
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

    void Func6(int& a)
    {
        printf("void Func6(int& %d)\n", a);
        a = 31415926;
    }

    void Func7(const ObjectPtr& a) const
    {
        printf("void Func7(const ObjectPtr& [%s]) const\n", a->GetType()->GetName().c_str());
    }
};

void print(MethodBase* m)
{
    if (m->ReturnType() == nullptr)
        printf("void ");
    else if (m->ReturnType()->IsBoxedType())
        printf("%s ", m->ReturnType()->GetName().c_str());
    else
        printf("std::shared_ptr<%s> ", m->ReturnType()->GetName().c_str());

    printf("%s(", m->GetName().c_str());
    for (size_t i = 0; i < m->GetParameters().size(); i++)
    {
        auto p = m->GetParameters()[i];
        if (p.IsConst)
            printf("const ");

        if (p.Type->IsBoxedType())
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

    TypeRegister<TestEnum>::New("TestEnum"s).value("Value1", TestEnum::Value1).value("Value2", TestEnum::Value2);

    TypeRegister<TestStruct>::New("TestStruct"s)
        .property("TE"s, &TestStruct::TE);

    TypeRegister<TestBase>::New("TestBase"s)
        .property("TestBaseA"s, &TestBase::TestBaseA)
        .method("BaseFunc1"s, &TestBase::BaseFunc1)
        .method("BaseFunc2"s, &TestBase::BaseFunc2)
        .method("BaseFunc3"s, &TestBase::BaseFunc3)
        .method("BaseFunc4"s, &TestBase::BaseFunc4)
        .method("BaseFunc5"s, &TestBase::BaseFunc5)
        .method("BaseFunc6"s, &TestBase::BaseFunc6)
        .method("BaseFunc7"s, &TestBase::BaseFunc7);

    TypeRegister<Test>::New("Test"s)
        .constructor<int>()
        .constructor<int, float>()
        .property("A"s, &Test::A, {{"clonable"s, true}, {"min_value", 100}, {"max_value", 100}})
        .property("B"s, &Test::B)
        .property("C"s, &Test::C)
        .property("D"s, &Test::D)
        .property("E"s, &Test::E)
        .property("Prop1"s, &Test::GetProp1, &Test::SetProp1)
        .property("Prop2"s, &Test::GetProp2)
        .property("Prop3"s, &Test::GetProp3)
        .method("Func1"s, &Test::Func1)
        .method("Func2"s, &Test::Func2)
        .method("Func3"s, &Test::Func3)
        .method("Func4"s, &Test::Func4)
        .method("Func5"s, &Test::Func5)
        .method("Func6"s, &Test::Func6)
        .method("Func7"s, &Test::Func7);
}

template <typename T>
struct Handle
{
};

namespace rtti
{
template <typename T>
struct TypeWarper<Handle<T>, false>
{
    using type = remove_cr<T>;
    using objtype = typename TypeWarper<type>::objtype;
    static Type* ClassType() { return TypeWarper<type>::ClassType(); }
};
} // namespace rtti

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    RegisterTypes();

    assert(typeof(Test) == typeof(std::shared_ptr<Test>));
    assert(type_of<Test>() == type_of<Handle<Test>>());

    auto type = Type::Find("Test"s);
    auto obj = type->Create<Test>();
    auto obj1 = type->Create<Test>(123);
    auto obj2 = type->Create<Test>(123, 789.12f);

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
        if (p->PropertyType()->IsBoxedType())
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
    printf("  ret = %s\n", type->GetMethod("Func4"s)->Invoke(obj, {})->GetType()->GetName().c_str());
    type->GetMethod("Func5"s, {typeof(int)})->Invoke(obj, {Box(1234)});
    type->GetMethod("Func7"s, {typeof(Object)})->Invoke(obj, {obj});

    printf("\n");

    return 0;
}