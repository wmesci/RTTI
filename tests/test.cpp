#include <cassert>
#define RTTI_ENABLE_LOG 1
#include <RTTI.h>

using namespace std::string_literals;

constexpr size_t DisplayNameAttr = HASH("DisplayName");

inline std::pair<size_t, std::any> DisplayName(const std::string& name)
{
    return {DisplayNameAttr, name};
}

inline std::string GetDisplayName(rtti::Attributable* attributable, const std::string& defaultValue)
{
    return attributable->GetAttribute<std::string>(DisplayNameAttr, defaultValue);
}

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

    static void RegisterRTTI()
    {
        rtti::TypeRegister<TestStruct>::New()
            .constructor<>()
            .convert<int>()
            .property("TE"s, &TestStruct::TE, {})
            .method("Func"s, &TestStruct::Func);
    }
};

class TestBase : public rtti::Object
{
    TYPE_DECLARE(rtti::Object)

public:
    static void RegisterRTTI()
    {
        rtti::TypeRegister<TestBase>::New()
            .property("TestBaseA"s, &TestBase::TestBaseA)
            .method("Func1"s, &TestBase::Func1)
            .method("BaseFunc2"s, &TestBase::BaseFunc2)
            .method("BaseFunc3"s, &TestBase::BaseFunc3)
            .method("BaseFunc4"s, &TestBase::BaseFunc4)
            .method("BaseFunc5"s, &TestBase::BaseFunc5)
            .method("BaseFunc7"s, &TestBase::BaseFunc7);
    }

public:
    TestBase()
    {
        printf("ctor()\n");
    }

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

    rtti::Ptr<TestBase> BaseFunc4() const
    {
        printf("ObjectPtr BaseFunc4() const\n");
        return rtti::MakePtr<TestBase>();
    }

    void BaseFunc5(int a)
    {
        printf("void BaseFunc5(int %d)\n", a);
    }

    void BaseFunc7(const rtti::ObjectPtr& a) const
    {
        printf("void BaseFunc7(const ObjectPtr& [%s]) const\n", a->GetRttiType()->GetName().c_str());
    }
};

class Test : public TestBase
{
    TYPE_DECLARE(TestBase)

public:
    static void RegisterRTTI()
    {
        rtti::TypeRegister<Test>::New()
            .constructor<>()
            .constructor<int>()
            .constructor<int, float>()
            .convert<int>()
            .property("A"s, &Test::A, {})
            .property("B"s, &Test::B, {})
            .property("C"s, &Test::C, {})
            .property("D"s, &Test::D, {})
            .property("E"s, &Test::E, {})
            .property("Prop1"s, &Test::GetProp1, &Test::SetProp1)
            .property("Prop2"s, &Test::GetProp2)
            .property("Prop3"s, &Test::GetProp3)
            .method("Func2"s, &Test::Func2)
            .method("Func3"s, &Test::Func3)
            .method("Func4"s, &Test::Func4)
            .method("Func5"s, &Test::Func5)
            .method("Func7"s, &Test::Func7)
            .method("Func8"s, &Test::Func8);
    }

public:
    Test() { printf("ctor()\n"); }
    Test(int i) { printf("ctor(%d)\n", i); }
    Test(int i, float f) { printf("ctor(%d, %f)\n", i, f); }

    int32_t A = 1;
    std::string B;
    rtti::ObjectPtr C;
    TestEnum D = TestEnum::Value1;
    TestStruct E;

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

    rtti::ObjectPtr Func4() const
    {
        printf("ObjectPtr Func4() const\n");
        return rtti::MakePtr<Test>();
    }

    void Func5(int a)
    {
        printf("void Func5(int %d)\n", a);
    }

    void Func7(const rtti::Ptr<TestBase>& a) const
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
    bool ConvertTo(rtti::Ptr<T>& target);
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

    HandleBase(const rtti::ObjectPtr& p)
        : m_ptr(p)
    {
    }

    rtti::ObjectPtr GetPtr() const
    {
        return m_ptr;
    }

protected:
    rtti::ObjectPtr m_ptr;
};

template <typename T>
struct Handle : public HandleBase
{
public:
    Handle() = default;

    Handle(const rtti::Ptr<T>& p)
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
        return static_cast<T*>(RTTI_RAW_FROM_PTR(m_ptr));
    }

    operator rtti::Ptr<T>() const
    {
        return RTTI_PTR_CAST(T, m_ptr);
    }

    static void RegisterRTTI()
    {
        rtti::TypeRegister<Handle<T>>::New("Handle<"s + rtti::GetTypeName<T>() + ">"s, {{HASH("handle"), rtti::type_of<T>()}})
            .template constructor<rtti::Ptr<T>>()
            .template convert<HandleBase>()
            .template convert<T>();
    }
};

void print(rtti::MethodBase* m)
{
    if (m->ReturnType() == nullptr)
        printf("void ");
    else if (m->ReturnType()->IsValueType())
        printf("%s ", m->ReturnType()->GetName().c_str());
    else
        printf("rtti::Ptr<%s> ", m->ReturnType()->GetName().c_str());

    printf("%s(", m->GetName().c_str());
    for (size_t i = 0; i < m->GetParameters().size(); i++)
    {
        auto p = m->GetParameters()[i];
        if (p.IsConst)
            printf("const ");

        if (p.ParameterType->IsValueType())
            printf("%s", p.ParameterType->GetName().c_str());
        else
            printf("rtti::Ptr<%s>", p.ParameterType->GetName().c_str());

        if (p.IsRef)
            printf("&");
        if (i < m->GetParameters().size() - 1)
            printf(", ");
    }
    printf(")");
}

void RegisterTypes()
{
    rtti::InitCoreType();

    rtti::TypeRegister<TestEnum>::New({DisplayName("TestEnumForDisplay")})
        .value("Value1", TestEnum::Value1)
        .value("Value2", TestEnum::Value2);

    TestStruct::RegisterRTTI();

    TestBase::RegisterRTTI();
    Test::RegisterRTTI();

    Handle<TestBase>::RegisterRTTI();
    Handle<Test>::RegisterRTTI();
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    RegisterTypes();

    printf("%s - %d - %s\n", rtti::type_of<int*>()->GetName().c_str(), rtti::type_of<int*>()->IsPointer(), rtti::type_of<int*>()->GetUnderlyingType()->GetName().c_str());

    assert(rtti::type_of<TestEnum2>()->IsEnum());
    assert(rtti::type_of<TestEnum2>()->CanConvertTo<int32_t>());

    assert(rtti::type_of<Test>() == rtti::type_of<rtti::Ptr<Test>>());
    assert(rtti::type_of<Test>()->CanConvertTo<int32_t>());

    Handle<TestBase> ht = Handle<Test>(rtti::MakePtr<Test>());

    assert(ht->GetRttiType() == rtti::type_of<Test>());

    rtti::ObjectPtr box_ht = rtti::Box(ht);
    printf("box_ht --> %s\n", box_ht->GetRttiType()->GetName().c_str());

    rtti::Type* inner_type = box_ht->GetRttiType()->GetAttribute<rtti::Type*>(HASH("handle"));
    assert(inner_type == rtti::type_of<TestBase>());

    auto ht2 = rtti::cast<HandleBase>(box_ht).GetPtr();
    assert(ht2 == ht.GetPtr());

    assert(rtti::type_of<TestEnum>()->GetAttribute<std::string>(DisplayNameAttr) == "TestEnumForDisplay"s);

    assert(rtti::type_of<int>() == rtti::Box(123)->GetRttiType());

    auto type = rtti::Type::Find("Test"s);

    auto obj = type->Create<Test>();
    auto obj1 = type->Create<Test>(123);
    auto obj2 = type->Create<Test>(123, 789.12f);

    assert(rtti::cast<void*>(obj) == RTTI_RAW_FROM_PTR(obj));
    assert(rtti::cast<Test>((TestBase*)RTTI_RAW_FROM_PTR(obj)) == RTTI_RAW_FROM_PTR(obj));
    assert(rtti::cast<Test*>((TestBase*)RTTI_RAW_FROM_PTR(obj)) == RTTI_RAW_FROM_PTR(obj));

    rtti::ObjectPtr ttt;
    rtti::Type::Convert(obj, rtti::type_of<int>(), ttt);

    assert(rtti::cast<int>(ttt) == 128);
    assert(rtti::cast<uint16_t>(ttt) == 128);
    assert(abs(rtti::cast<double>(ttt) - 128.0) <= 0.0001);
    auto comparable_enum_enum = rtti::is_comparable<TestEnum, TestEnum>();
    assert(comparable_enum_enum);
    auto comparable_enum_int = rtti::is_comparable<TestEnum, int>();
    assert(!comparable_enum_int);
    auto comparable_int_double = rtti::is_comparable<int, double>();
    assert(comparable_int_double);
    auto comparable_int_string = rtti::is_comparable<int, std::string>();
    assert(!comparable_int_string);
    assert(rtti::compare(ttt, 128.0f) == rtti::CompareResult::Equals);
    assert(rtti::compare(0u, ttt) == rtti::CompareResult::NotEquals);
    assert(rtti::compare(ttt, std::string("xxx")) == rtti::CompareResult::Failed);
    assert(ttt->GetHashCode() == std::hash<int>()(128));

    assert(rtti::cast<TestBase>(obj) != nullptr);
    assert(rtti::cast<Test>(rtti::cast<TestBase>(obj)) != nullptr);

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
            printf("rtti::Ptr<%s> %s", p->PropertyType()->GetName().c_str(), p->GetName().c_str());
        printf("\n");
    }

    printf("\n");
    printf("\033[31m [Test] \033[0m\n");

    type->GetMethod("Func1"s)->Invoke(obj, {});
    type->GetMethod("Func2"s)->Invoke(obj, {});
    printf("  ret = %d\n", rtti::Unbox<int>(type->GetMethod("Func3"s)->Invoke(obj, {})));
    printf("  ret = %s\n", type->GetMethod("Func4"s)->Invoke(obj, {})->GetRttiType()->GetName().c_str());
    type->GetMethod("Func5"s, {rtti::type_of<int>()})->Invoke(obj, {rtti::Box(TestEnum::Value1)});
    type->GetMethod("Func5"s, {rtti::type_of<int>()})->Invoke(obj, {rtti::Box(TestStruct())});
    type->GetMethod("Func5"s, {rtti::type_of<int>()})->Invoke(obj, {rtti::MakePtr<Test>()});
    type->GetMethod("Func7"s, {rtti::type_of<TestBase>()})->Invoke(obj, {obj});
    type->GetMethod("Func8"s, {rtti::type_of<TestEnum>()})->Invoke(obj, {rtti::Box(123)});

    TestStruct testStruct;
    rtti::type_of<TestStruct>()->GetMethod("Func"s)->Invoke(rtti::Box(testStruct), {rtti::Box(3)});
    rtti::type_of<TestStruct>()->GetMethod("Func"s)->Invoke(rtti::Box(&testStruct), {rtti::Box(4)});

    assert(testStruct.TE == (TestEnum)4);

    printf("\n");

    return 0;
}