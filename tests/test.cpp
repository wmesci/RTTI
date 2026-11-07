#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define RTTI_ENABLE_LOG 0
#include <RTTI.h>
#include <DynamicType.h>

using namespace rtti;
using namespace std::string_literals;

// ============================================================================
// Minimal test harness
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;
static int g_tests = 0;

static void run_test(const char* name, void (*body)())
{
    g_tests++;
    std::printf("\n  [TEST %d] %s\n", g_tests, name);
    const int before = g_failed;
    body();
    if (g_failed == before)
        std::printf("    ok\n");
}

#define EXPECT(...)                                                                                  \
    do                                                                                               \
    {                                                                                                \
        if (__VA_ARGS__)                                                                             \
            ++g_passed;                                                                              \
        else                                                                                         \
        {                                                                                            \
            ++g_failed;                                                                              \
            std::printf("    \033[31mFAIL\033[0m %s:%d: %s\n", __FILE__, __LINE__, #__VA_ARGS__);           \
        }                                                                                            \
    } while (0)

#define EXPECT_EQ(a, b)                                                                              \
    do                                                                                               \
    {                                                                                                \
        const auto& _a = (a);                                                                        \
        const auto& _b = (b);                                                                        \
        if (_a == _b)                                                                                \
            ++g_passed;                                                                              \
        else                                                                                         \
        {                                                                                            \
            ++g_failed;                                                                              \
            std::printf("    \033[31mFAIL\033[0m %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);    \
        }                                                                                            \
    } while (0)

#define EXPECT_NE(a, b)                                                                              \
    do                                                                                               \
    {                                                                                                \
        const auto& _a = (a);                                                                        \
        const auto& _b = (b);                                                                        \
        if (_a != _b)                                                                                \
            ++g_passed;                                                                              \
        else                                                                                         \
        {                                                                                            \
            ++g_failed;                                                                              \
            std::printf("    \033[31mFAIL\033[0m %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b);    \
        }                                                                                            \
    } while (0)

#define EXPECT_STREQ(a, b)                                                                           \
    do                                                                                               \
    {                                                                                                \
        const std::string _a = (a);                                                                  \
        const std::string _b = (b);                                                                  \
        if (_a == _b)                                                                                \
            ++g_passed;                                                                              \
        else                                                                                         \
        {                                                                                            \
            ++g_failed;                                                                              \
            std::printf("    \033[31mFAIL\033[0m %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__,    \
                        _a.c_str(), _b.c_str());                                                     \
        }                                                                                            \
    } while (0)

#if RTTI_HAS_EXCEPTIONS
#define EXPECT_THROW(expr)                                                                           \
    do                                                                                               \
    {                                                                                                \
        bool threw = false;                                                                          \
        try                                                                                          \
        {                                                                                            \
            expr;                                                                                    \
        }                                                                                            \
        catch (const Error&)                                                                         \
        {                                                                                            \
            threw = true;                                                                            \
        }                                                                                            \
        EXPECT(threw);                                                                               \
    } while (0)
#else
#define EXPECT_THROW(expr)                                                                           \
    do                                                                                               \
    {                                                                                                \
        expr;                                                                                        \
    } while (0)
#endif

// ============================================================================
// Test types
// ============================================================================

constexpr size_t DisplayNameAttr = RTTI_HASH("DisplayName");

inline std::pair<size_t, std::any> DisplayName(const std::string& name)
{
    return { DisplayNameAttr, name };
}

enum class TestEnum { Value1, Value2 };
enum class TestEnum2 { Value1, Value2 };
enum class SmallEnum : uint8_t { A = 1, B = 2 };
enum class WideEnum : int64_t { A = 1, B = 2 };

struct TestStruct
{
    int value = 0;
    operator int() const { return value + 250; }
};

namespace test_ns
{
class Base : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register()
    {
        TypeRegister<Base>::New()
            .constructor<>()
            .property("BaseInt", &Base::BaseInt)
            .property("BaseString", &Base::BaseString)
            .method("BaseFunc", &Base::BaseFunc);
    }

    int BaseInt = 1;
    std::string BaseString;
    void BaseFunc(int value) { BaseInt += value; }
};
} // namespace test_ns

class Test : public test_ns::Base
{
    RTTI_TYPE_DECLARE(test_ns::Base)
public:
    static void RegisterRTTI()
    {
        TypeRegister<Test>::New()
            .constructor<>()
            .constructor<int>()
            .constructor<int, float>()
            .convert<int>()
            .property("A", &Test::A)
            .property("B", &Test::B)
            .property("Prop1", &Test::GetProp1, &Test::SetProp1)
            .property("Prop2", &Test::GetProp2)
            .method("Func3", &Test::Func3)
            .method("Func5", &Test::Func5)
            .method("StaticFunc", &Test::StaticFunc);
    }

    Test() = default;
    explicit Test(int value) { A = value; }
    Test(int value, float other) { A = value + static_cast<int>(other); }

    int GetProp1() { return A; }
    void SetProp1(int value) { A = value; }
    int GetProp2() { return 42; }
    int Func3() { return 34; }
    void Func5(int value) { A = value; }
    static int StaticFunc() { return 99; }

    template <typename U>
    bool ConvertTo(U& target)
    {
        (void)target;
        return false;
    }

    int A = 1;
    std::string B;
    ObjectPtr C;
    TestEnum D = TestEnum::Value1;
    TestStruct E;
};

template <>
bool Test::ConvertTo<int>(int& target)
{
    target = A + 127;
    return true;
}

class Flexible : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register()
    {
        TypeRegister<Flexible>::New()
            .constructor<>()
            .property("Name", &Flexible::GetName, &Flexible::SetName)
            .property("Number", &Flexible::GetNumber, &Flexible::SetNumber)
            .method("Add", &Flexible::Add)
            .method("Out", &Flexible::Out);
    }

    std::string GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }
    int GetNumber() const { return m_number; }
    void SetNumber(int number) { m_number = number; }
    int Add(int a, int b) { return a + b; }
    void Out(int& value) { value = 42; }

private:
    std::string m_name;
    int m_number = 0;
};

class OverloadedSetter : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register()
    {
        TypeRegister<OverloadedSetter>::New()
            .constructor<>()
            .property("Value", &OverloadedSetter::GetValue, &OverloadedSetter::SetValue);
    }

    int GetValue() const { return m_value; }
    void SetValue(int value) { m_value = value; }
    void SetValue(float value) { m_value = static_cast<int>(value); }

private:
    int m_value = 0;
};

class Sink : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register()
    {
        TypeRegister<Sink>::New().constructor<>().method("Take", &Sink::Take);
    }

    Type* received = nullptr;
    void Take(const ObjectPtr& value) { received = value ? value->GetRttiType() : nullptr; }
};

class Thrower : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register() { TypeRegister<Thrower>::New().constructor<>().method("Boom", &Thrower::Boom); }
#if RTTI_HAS_EXCEPTIONS
    void Boom() { throw std::runtime_error("boom"); }
#else
    void Boom() {}
#endif
};

class DynamicBase : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register()
    {
        TypeRegister<DynamicBase>::New()
            .constructor<>()
            .property("Name", &DynamicBase::Name)
            .method("Bump", &DynamicBase::Bump);
    }

    std::string Name = "base";
    int Value = 0;
    int Bump(int by) { return Value += by; }
};

class NoDefaultBase : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register() { TypeRegister<NoDefaultBase>::New().constructor<int>(); }
    explicit NoDefaultBase(int value) { (void)value; }
};

class ConvertibleBase : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register() { TypeRegister<ConvertibleBase>::New().constructor<>().convert<int>(); }
    template <typename U>
    bool ConvertTo(U& target)
    {
        (void)target;
        return false;
    }
};

template <>
bool ConvertibleBase::ConvertTo<int>(int& target)
{
    target = 7;
    return true;
}

class ConvertibleDerived : public ConvertibleBase
{
    RTTI_TYPE_DECLARE(ConvertibleBase)
public:
    static void Register() { TypeRegister<ConvertibleDerived>::New().constructor<>(); }
};

class CallbackValue
{
public:
    int value = 0;
};

class CallbackObject : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register()
    {
        TypeRegister<CallbackObject>::New()
            .constructor<>()
            .Convert<int>([](const CallbackObject& object, int& target)
                          {
                              target = object.value;
                              return true;
                          });
    }
    int value = 11;
};

class ThrowingConstructor : public Object
{
    RTTI_TYPE_DECLARE(Object)
public:
    static void Register() { TypeRegister<ThrowingConstructor>::New().constructor<>(); }
#if RTTI_HAS_EXCEPTIONS
    ThrowingConstructor() { throw std::runtime_error("ctor"); }
#else
    ThrowingConstructor() = default;
#endif
};

// ============================================================================
// Registration
// ============================================================================

static void RegisterAllTypes()
{
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    InitCoreType();
    TypeRegister<TestEnum>::New({ DisplayName("TestEnumForDisplay") })
        .value("Value1", TestEnum::Value1)
        .value("Value2", TestEnum::Value2);
    TypeRegister<SmallEnum>::New("SmallEnum").value("A", SmallEnum::A).value("B", SmallEnum::B);
    TypeRegister<WideEnum>::New("WideEnum").value("A", WideEnum::A).value("B", WideEnum::B);
    TypeRegister<TestStruct>::New().constructor<>().convert<int>();
    test_ns::Base::Register();
    Test::RegisterRTTI();
    Flexible::Register();
    Sink::Register();
    Thrower::Register();
    OverloadedSetter::Register();
    DynamicBase::Register();
    NoDefaultBase::Register();
    ConvertibleBase::Register();
    ConvertibleDerived::Register();
    CallbackObject::Register();
    ThrowingConstructor::Register();
}

// ============================================================================
// Tests
// ============================================================================

static void test_type_identity()
{
    RegisterAllTypes();

    EXPECT_EQ(type_of<Test>(), type_of<Ptr<Test>>());
    auto roundTrip = PtrFromRaw(RawFromPtr(MakePtr<Test>()));
    EXPECT(roundTrip != nullptr);
    EXPECT(type_of<Test>()->GetName().find("Test") != std::string::npos);
    EXPECT_EQ(Type::Find("int32_t"), type_of<int32_t>());
    EXPECT_EQ(Type::Find("int"), type_of<int32_t>());
    EXPECT_EQ(Type::Find("int64_t"), type_of<int64_t>());
    EXPECT_EQ(Type::Find("float"), type_of<float>());
    EXPECT_EQ(Type::Find("string"), type_of<std::string>());
    EXPECT(Type::Find("NoSuchType") == nullptr);

    EXPECT_EQ(type_of<int>()->GetSize(), sizeof(int));
    EXPECT_EQ(type_of<int*>()->GetPointeeType(), type_of<int>());
    EXPECT_EQ(type_of<SmallEnum>()->GetEnumUnderlyingType(), type_of<uint8_t>());
    EXPECT(type_of<int*>()->IsPointer());
    EXPECT(type_of<bool>()->IsIntegral());
    EXPECT(type_of<float>()->IsFloating());
    EXPECT(type_of<Test>()->IsSubclassOf(type_of<test_ns::Base>()));
    EXPECT(type_of<Test>()->IsAssignableTo(type_of<test_ns::Base>()));
    EXPECT(type_of<test_ns::Base>()->IsAssignableFrom(MakePtr<Test>()));
    EXPECT(!type_of<Test>()->IsAssignableFrom(MakePtr<test_ns::Base>()));
}

static void test_registration_idempotent()
{
    RegisterAllTypes();

    Type* type = type_of<Test>();
    const size_t constructors = type->GetConstructors().size();
    const size_t methods = type->GetMethods().size();
    const size_t properties = type->GetProperties().size();

    Test::RegisterRTTI();
    Test::RegisterRTTI();

    EXPECT_EQ(type->GetConstructors().size(), constructors);
    EXPECT_EQ(type->GetMethods().size(), methods);
    EXPECT_EQ(type->GetProperties().size(), properties);
    EXPECT_EQ(type_of<int>()->GetConstructors().size(), 2u);
}

static void test_enum_conversion()
{
    RegisterAllTypes();

    bool ok = false;
    EXPECT_EQ(cast<SmallEnum>(Box(2), &ok), SmallEnum::B);
    EXPECT(ok);
    EXPECT_EQ(cast<SmallEnum>(Box(int64_t{ 1 }), &ok), SmallEnum::A);
    EXPECT(ok);
    EXPECT_EQ(cast<WideEnum>(Box(2), &ok), WideEnum::B);
    EXPECT(ok);
    EXPECT_EQ(cast<int>(Box(SmallEnum::B), &ok), 2);
    EXPECT(ok);
    EXPECT_EQ(cast<int64_t>(Box(WideEnum::B), &ok), 2);
    EXPECT(ok);

    EXPECT(type_of<SmallEnum>()->CanConvertTo(type_of<int32_t>()));
    EXPECT(type_of<int32_t>()->CanConvertTo(type_of<SmallEnum>()));

    auto* type = type_of<TestEnum>();
    EXPECT_STREQ(type->GetEnumName(Box(TestEnum::Value1)), "Value1");
    EXPECT_EQ(cast<TestEnum>(type->GetEnumValue("Value2")), TestEnum::Value2);
    EXPECT_EQ(type->GetEnumValues().size(), 2u);
}

static void test_boxing()
{
    RegisterAllTypes();

    auto boxedInt = Box(123);
    EXPECT_EQ(boxedInt->GetRttiType(), type_of<int>());
    EXPECT_EQ(Unbox<int>(boxedInt), 123);
    EXPECT_EQ(boxedInt->GetHashCode(), std::hash<int>()(123));
    EXPECT_EQ(Box(-0.0f)->GetHashCode(), Box(0.0f)->GetHashCode());

    auto boxedString = Box("hello"s);
    EXPECT_EQ(Unbox<std::string>(boxedString), "hello");
    auto literal = Box("literal");
    EXPECT_EQ(literal->GetRttiType(), type_of<std::string>());

    int value = 42;
    auto boxedPointer = Box(&value);
    EXPECT_EQ(boxedPointer->GetRttiType(), type_of<int*>());
    EXPECT_EQ(Unbox<int*>(boxedPointer), &value);
    EXPECT_EQ(Unbox<void*>(boxedPointer), &value);
    EXPECT_EQ(cast<void*>(boxedPointer), &value);

    int mutate = 1;
    auto mutableBox = Box(mutate);
    Unbox<int&>(mutableBox) = 9;
    EXPECT_EQ(Unbox<int>(mutableBox), 9);

    EXPECT_THROW(Unbox<int>(Box(3.0f)));
    EXPECT_EQ(GetLastError().code, ErrorCode::TypeMismatch);

    int out = 0;
    EXPECT(try_unbox<int>(Box(5), out));
    EXPECT_EQ(out, 5);
    EXPECT(!try_unbox<int>(Box("no"s), out));
}

static void test_object_registration()
{
    RegisterAllTypes();
    auto* type = type_of<Test>();

    EXPECT_EQ(type->GetConstructors().size(), 3u);
    EXPECT(type->GetConstructor() != nullptr);
    EXPECT(type->GetConstructor({ type_of<int>() }) != nullptr);

    auto object = type->CreateInstance({});
    EXPECT(object != nullptr);
    EXPECT_EQ(object->GetRttiType(), type_of<Test>());

    auto withArg = type->CreateInstance<Test>(123);
    EXPECT(withArg != nullptr);
    EXPECT_EQ(withArg->A, 123);

    auto* property = type->GetProperty("A");
    EXPECT(property != nullptr);
    EXPECT(property->CanRead());
    EXPECT(property->CanWrite());
    EXPECT_EQ(type->GetProperty("BaseInt"), type_of<test_ns::Base>()->GetProperty("BaseInt"));
    EXPECT(type->GetMethod("Func3") != nullptr);
    EXPECT(type->GetMethod("StaticFunc") != nullptr);
    EXPECT(type->GetMethod("StaticFunc")->IsStatic());
    EXPECT(!type->GetMethod("Func3")->IsStatic());
}

static void test_method_invocation()
{
    RegisterAllTypes();
    auto object = MakePtr<Test>();
    auto* type = type_of<Test>();

    EXPECT_EQ(Unbox<int>(type->GetMethod("Func3")->Invoke(object, {})), 34);
    type->GetMethod("Func5")->Invoke(object, { Box(42) });
    EXPECT_EQ(object->A, 42);
    type->GetMethod("Func5")->Invoke(object, { Box(TestEnum::Value2) });
    EXPECT_EQ(object->A, 1);
    type->GetMethod("Func5")->Invoke(object, { Box(TestStruct{ 7 }) });
    EXPECT_EQ(object->A, 257);

    EXPECT_EQ(Unbox<int>(type->GetMethod("StaticFunc")->Invoke(nullptr, {})), 99);

    EXPECT_THROW(type->GetMethod("Func5")->Invoke(object, {}));
    EXPECT_THROW(type->GetMethod("Func5")->Invoke(object, { Box(std::string("no")), Box(1) }));

    type->GetMethod("Func5")->Invoke(object, { MakePtr<Test>() });
    EXPECT_EQ(object->A, 128);
}

static void test_method_target_exception()
{
    RegisterAllTypes();
    auto object = MakePtr<Thrower>();
#if RTTI_HAS_EXCEPTIONS
    bool caught = false;
    try
    {
        type_of<Thrower>()->GetMethod("Boom")->Invoke(object, {});
    }
    catch (const TargetInvocationError&)
    {
        caught = true;
    }
    EXPECT(caught);
#else
    EXPECT_THROW(type_of<Thrower>()->GetMethod("Boom")->Invoke(object, {}));
#endif
}

static void test_property_access()
{
    RegisterAllTypes();
    auto object = MakePtr<Test>();
    auto* type = type_of<Test>();

    type->GetProperty("A")->SetValue(object, Box(456));
    EXPECT_EQ(object->A, 456);
    EXPECT_EQ(type->GetProperty("A")->GetValue<int>(object), 456);
    EXPECT_EQ(type->GetProperty("A")->GetValue(object)->GetRttiType(), type_of<int>());

    auto* readOnly = type->GetProperty("Prop2");
    EXPECT(readOnly->CanRead());
    EXPECT(!readOnly->CanWrite());
    EXPECT_EQ(readOnly->GetValue<int>(object), 42);
    EXPECT_THROW(readOnly->SetValue(object, Box(1)));

    type->GetProperty("B")->SetValue(object, Box(std::string("text")));
    EXPECT_STREQ(type->GetProperty("B")->GetValue<std::string>(object), "text");

    auto* overloaded = type_of<OverloadedSetter>();
    auto overloadedObject = MakePtr<OverloadedSetter>();
    overloaded->GetProperty("Value")->SetValue(overloadedObject, Box(12));
    EXPECT_EQ(overloaded->GetProperty("Value")->GetValue<int>(overloadedObject), 12);

    auto* flexible = type_of<Flexible>();
    auto flexibleObject = MakePtr<Flexible>();
    flexible->GetProperty("Name")->SetValue(flexibleObject, Box("dynamic name"));
    EXPECT_STREQ(flexible->GetProperty("Name")->GetValue<std::string>(flexibleObject), "dynamic name");
    flexible->GetProperty("Number")->SetValue(flexibleObject, Box(8));
    EXPECT_EQ(flexible->GetProperty("Number")->GetValue<int>(flexibleObject), 8);

    auto boxedOut = Box(0);
    flexible->GetMethod("Out")->Invoke(flexibleObject, { boxedOut });
    EXPECT_EQ(Unbox<int>(boxedOut), 42);

    EXPECT_THROW(type->GetProperty("A")->GetValue<std::string>(object));
    EXPECT_THROW(type->GetProperty("A")->SetValue(object, Box(std::string("no"))));

    ObjectPtr ignored;
    EXPECT(type->GetProperty("A")->TryGetValue(object, ignored));
    EXPECT(!readOnly->TrySetValue(object, Box(1)));
}

static void test_casts()
{
    RegisterAllTypes();
    auto object = MakePtr<Test>();
    auto base = cast<test_ns::Base>(object);
    EXPECT(base != nullptr);
    auto derived = cast<Test>(base);
    EXPECT(derived != nullptr);
    EXPECT_EQ(RawFromPtr(derived), RawFromPtr(object));

    auto* raw = RawFromPtr(object);
    EXPECT_EQ(cast<Test*>(static_cast<test_ns::Base*>(raw)), raw);
    EXPECT_EQ(cast<void*>(object), static_cast<void*>(RawFromPtr(object)));
    int pointee = 7;
    EXPECT_EQ(cast<int*>(Box(&pointee)), &pointee);

    ObjectPtr nullObject;
    bool ok = true;
    auto nullResult = cast<Test>(nullObject, &ok);
    EXPECT(ok && nullResult == nullptr);

    EXPECT_EQ(cast<int>(Box(128)), 128);
    EXPECT_EQ(cast<double>(Box(128)), 128.0);
    EXPECT_EQ(cast<bool>(Box(1)), true);

    ObjectPtr converted;
    EXPECT(Type::Convert(object, type_of<int>(), converted));
    EXPECT_EQ(Unbox<int>(converted), 128);

    EXPECT(!cast<int>(Box("bad"s), &ok));
    EXPECT(!ok);

    EXPECT_EQ(cast<int>(MakePtr<ConvertibleBase>()), 7);
    EXPECT_EQ(cast<int>(MakePtr<ConvertibleDerived>()), 7);
    EXPECT_EQ(cast<int>(MakePtr<CallbackObject>()), 11);
}

static void test_compare()
{
    RegisterAllTypes();

    EXPECT(Type::Compare(Box(128), Box(128.0f)) == CompareResult::Equals);
    EXPECT(Type::Compare(Box(3.9f), Box(3)) == CompareResult::NotEquals);
    EXPECT(Type::Compare(Box(3), Box(3.9f)) == CompareResult::NotEquals);
    EXPECT(Type::Compare(Box(0u), Box(128)) == CompareResult::NotEquals);
    EXPECT(Type::Compare(Box(TestEnum::Value1), Box(TestEnum::Value1)) == CompareResult::Equals);
    EXPECT(Type::Compare(ObjectPtr(), ObjectPtr()) == CompareResult::Equals);
    EXPECT(Type::Compare(ObjectPtr(), Box(5)) == CompareResult::NotEquals);
    EXPECT(Type::Compare(Box(3.0f), Box(3)) == CompareResult::Equals);

    EXPECT(is_comparable<int, double>());
    EXPECT(!is_comparable<int, std::string>());
    EXPECT(is_comparable<Test, Test>());
    auto first = MakePtr<Test>();
    auto second = MakePtr<Test>();
    EXPECT(Type::Compare(first, first) == CompareResult::Equals);
    EXPECT(Type::Compare(first, second) == CompareResult::NotEquals);
    EXPECT(!is_comparable<Test, std::string>());
}

static void test_attributes()
{
    RegisterAllTypes();
    auto* type = type_of<TestEnum>();
    EXPECT(type->HasAttribute(DisplayNameAttr));
    EXPECT_STREQ(type->GetAttribute<std::string>(DisplayNameAttr), "TestEnumForDisplay");
    EXPECT_STREQ(type->GetAttribute<std::string>(RTTI_HASH("nonexistent"), "fallback"), "fallback");
    EXPECT(type->TryGetAttribute<std::string>(DisplayNameAttr) != nullptr);
    EXPECT(type->TryGetAttribute<int>(DisplayNameAttr) == nullptr);
}

static void test_dynamic_basics()
{
    RegisterAllTypes();

    auto* type = NewDynamicType("DynVec3", { { "x", type_of<float>() }, { "y", type_of<float>() }, { "name", type_of<std::string>() } });
    EXPECT(type != nullptr);
    EXPECT_EQ(Type::Find("DynVec3"), type);
    EXPECT_STREQ(type->GetName(), "DynVec3");
    EXPECT(!type->IsValueType());
    EXPECT(type->IsDynamicType());
    EXPECT(type->GetBaseType() == type_of<Object>());
    EXPECT(type->GetSize() == 0u);

    auto object = type->CreateInstance({});
    EXPECT(object != nullptr);
    EXPECT_EQ(object->GetRttiType(), type);
    EXPECT(type->GetProperty("x")->GetValue<float>(object) == 0.0f);

    type->GetProperty("x")->SetValue(object, Box(3.5f));
    EXPECT(type->GetProperty("x")->GetValue<float>(object) == 3.5f);
    EXPECT_THROW(type->GetProperty("x")->SetValue(object, Box(std::string("no"))));

    auto other = type->CreateInstance({});
    type->GetProperty("x")->SetValue(other, Box(9.0f));
    EXPECT(type->GetProperty("x")->GetValue<float>(object) == 3.5f);
    EXPECT(type->GetProperty("x")->GetValue<float>(other) == 9.0f);

    EXPECT(type->GetProperty("missing") == nullptr);
    EXPECT_THROW(NewDynamicType("DynVec3", {}));
}

static void test_dynamic_methods()
{
    RegisterAllTypes();

    auto* method = NewDynamicMethod(
        nullptr, "Add", type_of<int>(),
        { { type_of<int>(), false, false }, { type_of<int>(), false, false } },
        [](const ObjectPtr&, const std::vector<ObjectPtr>& args) -> ObjectPtr
        { return Box(Unbox<int>(args[0]) + Unbox<int>(args[1])); });
    auto* type = NewDynamicType("DynCalc", {}, { method });
    EXPECT_EQ(type->GetMethod("Add")->GetOwnerType(), type);
    auto object = type->CreateInstance({});
    EXPECT_EQ(Unbox<int>(type->GetMethod("Add")->Invoke(object, { Box(10), Box(20) })), 30);
    EXPECT(type->GetMethod("Add", { type_of<int>(), type_of<int>() }) != nullptr);
}

static void test_dynamic_compile_time_base()
{
    RegisterAllTypes();

    auto* type = NewDynamicType("DynExtendsBase", { { "extra", type_of<float>() } }, {}, {}, type_of<DynamicBase>());
    EXPECT(type != nullptr);
    auto object = type->CreateInstance({});
    EXPECT(object != nullptr);

    type->GetProperty("Name")->SetValue(object, Box(std::string("hero")));
    EXPECT_STREQ(type->GetProperty("Name")->GetValue<std::string>(object), "hero");
    type->GetProperty("Name")->SetValue(object, Box(std::string("hero")));

    auto base = cast<DynamicBase>(object);
    EXPECT(base != nullptr);
    EXPECT(RawFromPtr(base) != RawFromPtr(object));
    EXPECT_STREQ(RawFromPtr(base)->Name, "hero");
    EXPECT_EQ(Unbox<int>(type->GetMethod("Bump")->Invoke(object, 9)), 9);
    EXPECT_EQ(RawFromPtr(base)->Value, 9);

    EXPECT_THROW(NewDynamicType("DynNoDefault", {}, {}, {}, type_of<NoDefaultBase>()));
}

static void test_dynamic_inheritance()
{
    RegisterAllTypes();

    auto* parent = NewDynamicType("DynParent", { { "age", type_of<int>() } });
    auto* child = NewDynamicType("DynChild", { { "score", type_of<float>() } }, {}, {}, parent);
    EXPECT(child->IsSubclassOf(parent));
    EXPECT(child->IsSubclassOf(type_of<Object>()));
    EXPECT(!parent->IsSubclassOf(child));
    EXPECT(child->GetProperty("age") != nullptr);
    EXPECT(child->GetProperty("score") != nullptr);

    auto object = child->CreateInstance({});
    child->GetProperty("age")->SetValue(object, Box(3));
    EXPECT_EQ(child->GetProperty("age")->GetValue<int>(object), 3);
    auto asParent = cast<Object>(object);
    EXPECT(asParent != nullptr);
    EXPECT_EQ(asParent->GetRttiType(), child);
}

static void test_dynamic_constructors()
{
    RegisterAllTypes();

    auto* type = NewDynamicType("DynCtor", { { "value", type_of<int>() } });
    auto* constructor = NewDynamicConstructor(
        type, { { type_of<int>(), false, false } },
        [](const std::vector<ObjectPtr>& args) -> ObjectPtr
        {
            auto object = MakePtr<DynamicObject>();
            object->Set("value", args[0]);
            return object;
        });
    type->AttachConstructor(constructor);
    EXPECT(type->GetConstructor({ type_of<int>() }) != nullptr);

    auto object = type->CreateInstance({ Box(77) });
    EXPECT(object != nullptr);
    EXPECT_EQ(object->GetRttiType(), type);
    EXPECT_EQ(type->GetProperty("value")->GetValue<int>(object), 77);
}

static void test_dynamic_property_helpers()
{
    RegisterAllTypes();
    auto* type = NewDynamicType("DynStandalone", {});
    auto* property = NewDynamicProperty(type, "bonus", type_of<float>(), { { RTTI_HASH("default"), 0.0f } });
    type->AttachProperty(property);
    EXPECT_EQ(type->GetProperty("bonus"), property);
    EXPECT(property->HasAttribute(RTTI_HASH("default")));
}

static void test_freezing()
{
    RegisterAllTypes();

    TypeRegistry::Get().Freeze();
    EXPECT(TypeRegistry::Get().IsFrozen());
    EXPECT_THROW(TypeRegister<Flexible>::New());
    EXPECT_THROW(TypeRegister<Test>::New().constructor<>());

    auto* dynamic = NewDynamicType("DynAfterFreeze", {});
    EXPECT(dynamic != nullptr);
    EXPECT(dynamic->CreateInstance({}) != nullptr);
}

static void test_foreach_snapshot()
{
    RegisterAllTypes();

    const size_t before = Type::GetTypes().size();
    size_t visits = 0;
    Type::ForEach([&](Type*)
    {
        ++visits;
        if (visits == 1)
            NewDynamicType("DynDuringForEach", {});
    });
    EXPECT_EQ(visits, before);
    EXPECT(Type::GetTypes().size() == before + 1);
}

static void test_namespaces()
{
    RegisterAllTypes();

    EXPECT_EQ(Type::Find("test_ns::Base"), type_of<test_ns::Base>());
    EXPECT_EQ(Type::Find("Base"), type_of<test_ns::Base>());
    EXPECT_EQ(Type::Find("test_ns::Base")->GetShortName(), "Base");
    EXPECT_EQ(Type::Find("test_ns::Base")->GetSerializationName(), "Base");
    EXPECT_EQ(type_of<int32_t>()->GetSerializationName(), "int32_t");
}

static void test_errors()
{
    RegisterAllTypes();

    ClearLastError();
    EXPECT(!type_of<int>()->CanConvertTo(static_cast<Type*>(nullptr)));
    ObjectPtr nullTarget;
    EXPECT(!Type::Convert(Box(1), nullptr, nullTarget));

    auto object = MakePtr<Test>();
    EXPECT_THROW(type_of<Test>()->CreateInstance({ Box(1), Box(2), Box(3), Box(4) }));
    ObjectPtr ignored;
    EXPECT(!type_of<Test>()->TryCreateInstance({ Box(1), Box(2), Box(3), Box(4) }, ignored));

    EXPECT_THROW(type_of<Test>()->GetMethod("Func3")->Invoke(nullptr, {}));
    EXPECT_EQ(GetLastError().code, ErrorCode::NullTarget);

    EXPECT_THROW(type_of<Test>()->GetMethod("Func3")->Invoke(MakePtr<Flexible>(), {}));

    EXPECT_THROW(type_of<Test>()->CreateInstance("*"s));
}

static void test_custom_type()
{
    RegisterAllTypes();

    Type* custom = NewType("CustomValue", sizeof(int), TypeFlags::None, nullptr, type_of<ObjectBox>());
    EXPECT(custom != nullptr);
    EXPECT_EQ(Type::Find("CustomValue"), custom);
    EXPECT(custom->IsValueType());
    EXPECT(custom->GetSize() == sizeof(int));
    EXPECT_THROW(NewType("CustomValue", sizeof(int), TypeFlags::None, nullptr, type_of<ObjectBox>()));
}

static void test_select_overload()
{
    RegisterAllTypes();
    auto function = select_overload<int (Flexible::*)(int, int)>(&Flexible::Add);
    EXPECT(function != nullptr);
    auto function2 = select_overload<int (int, int), Flexible>(&Flexible::Add);
    EXPECT(function2 != nullptr);
}

static void test_threaded_type_of()
{
    RegisterAllTypes();

    std::vector<std::thread> threads;
    std::vector<Type*> results(4, nullptr);
    for (int i = 0; i < 4; i++)
    {
        threads.emplace_back([&results, i]
        {
            switch (i)
            {
                case 0: results[0] = type_of<TestStruct>(); break;
                case 1: results[1] = type_of<Flexible>(); break;
                case 2: results[2] = type_of<CallbackValue>(); break;
                default: results[3] = type_of<ThrowingConstructor>(); break;
            }
        });
    }
    for (auto& thread : threads)
        thread.join();
    for (Type* type : results)
        EXPECT(type != nullptr);
}

static void test_try_api()
{
    RegisterAllTypes();
    auto object = MakePtr<Flexible>();

    ObjectPtr result;
    EXPECT(type_of<Flexible>()->GetMethod("Add")->TryInvoke(object, { Box(1), Box(2) }, result));
    EXPECT_EQ(Unbox<int>(result), 3);

    EXPECT(!type_of<Flexible>()->GetMethod("Add")->TryInvoke(object, { Box(1) }, result));
    EXPECT(!type_of<Flexible>()->GetMethod("Add")->TryInvoke(nullptr, { Box(1), Box(2) }, result));

    EXPECT(type_of<Flexible>()->GetProperty("Number")->TrySetValue(object, Box(5)));
    int number = 0;
    EXPECT(type_of<Flexible>()->GetProperty("Number")->TryGetValue(object, number));
    EXPECT_EQ(number, 5);
}

static void test_value_conversions()
{
    RegisterAllTypes();

    EXPECT_EQ(cast<int>(Box(int8_t{ -1 })), -1);
    EXPECT_EQ(cast<uint8_t>(Box(int{ 255 })), 255);
    EXPECT_EQ(cast<double>(Box(3.5f)), 3.5);
    EXPECT_EQ(cast<bool>(Box(0)), false);
    EXPECT_EQ(cast<bool>(Box(2)), true);
    EXPECT_EQ(cast<char>(Box(65)), 'A');
    EXPECT_EQ(cast<long double>(Box(1)), 1.0L);
    ObjectPtr enumResult;
    EXPECT(Type::Convert(Box(3), type_of<TestEnum>(), enumResult));
    EXPECT_EQ(cast<TestEnum>(enumResult), static_cast<TestEnum>(3));
}

// ============================================================================
// Interface query
// ============================================================================

class ITestLight : public Interface
{
public:
    virtual int LightValue() const = 0;
};

class IOtherCapability : public Interface
{
public:
    virtual int Capability() const = 0;
};

class InterfaceActor : public Object, public ITestLight, public IOtherCapability
{
public:
    RTTI_TYPE_DECLARE(Object, ITestLight, IOtherCapability)

    int LightValue() const override { return 7; }
    int Capability() const override { return 9; }
};

class InterfaceDerived : public InterfaceActor
{
public:
    RTTI_TYPE_DECLARE(InterfaceActor)
};

class PlainActor : public Object
{
public:
    RTTI_TYPE_DECLARE(Object)
};

class InnerProvider : public ITestLight
{
public:
    explicit InnerProvider(int value) : m_value(value) {}
    int LightValue() const override { return m_value; }
private:
    int m_value;
};

class InnerActor : public Object
{
public:
    RTTI_TYPE_DECLARE(Object)
    RTTI_INTERFACE_IMPL(ITestLight, m_provider.get())

    explicit InnerActor(int value) : m_provider(std::make_unique<InnerProvider>(value)) {}

private:
    std::unique_ptr<InnerProvider> m_provider;
};

static void test_interface_query()
{
    auto actor = MakePtr<InterfaceActor>();
    EXPECT(type_of<ITestLight>()->IsInterface());
    EXPECT(type_of<InterfaceActor>()->Implements(type_of<ITestLight>()));
    EXPECT(type_of<InterfaceActor>()->Implements(type_of<IOtherCapability>()));
    EXPECT_EQ(type_of<InterfaceActor>()->GetInterfaces().size(), 2u);

    auto* light = actor->As<ITestLight>();
    auto* other = actor->As<IOtherCapability>();
    EXPECT(light != nullptr);
    EXPECT(other != nullptr);
    EXPECT_EQ(light->LightValue(), 7);
    EXPECT_EQ(other->Capability(), 9);
    EXPECT(actor->As<Interface>() == nullptr);
    EXPECT(actor->As<PlainActor>() == nullptr);

    auto derived = MakePtr<InterfaceDerived>();
    EXPECT(derived->As<ITestLight>() != nullptr);
    EXPECT_EQ(derived->As<ITestLight>()->LightValue(), 7);
    EXPECT(type_of<InterfaceDerived>()->Implements(type_of<ITestLight>()));

    auto plain = MakePtr<PlainActor>();
    EXPECT(plain->As<ITestLight>() == nullptr);

    TypeRegister<InnerActor>::New().interface<ITestLight>();
    EXPECT(type_of<InnerActor>()->Implements(type_of<ITestLight>()));

    auto inner = MakePtr<InnerActor>(42);
    EXPECT(inner->As<ITestLight>() != nullptr);
    EXPECT_EQ(inner->As<ITestLight>()->LightValue(), 42);
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::printf("========================================\n");
    std::printf("  RTTI Unit Tests\n");
    std::printf("========================================\n");

    RegisterAllTypes();

    run_test("Type identity", test_type_identity);
    run_test("Registration idempotent", test_registration_idempotent);
    run_test("Enum conversion", test_enum_conversion);
    run_test("Boxing/unboxing", test_boxing);
    run_test("Object registration", test_object_registration);
    run_test("Method invocation", test_method_invocation);
    run_test("Method target exception", test_method_target_exception);
    run_test("Property access", test_property_access);
    run_test("Casts", test_casts);
    run_test("Compare", test_compare);
    run_test("Attributes", test_attributes);
    run_test("Dynamic basics", test_dynamic_basics);
    run_test("Dynamic methods", test_dynamic_methods);
    run_test("Dynamic compile-time base", test_dynamic_compile_time_base);
    run_test("Dynamic inheritance", test_dynamic_inheritance);
    run_test("Dynamic constructors", test_dynamic_constructors);
    run_test("Dynamic property helpers", test_dynamic_property_helpers);
    run_test("ForEach snapshot", test_foreach_snapshot);
    run_test("Namespaces", test_namespaces);
    run_test("Errors", test_errors);
    run_test("Custom type", test_custom_type);
    run_test("select_overload", test_select_overload);
    run_test("Threaded type_of", test_threaded_type_of);
    run_test("Try* API", test_try_api);
    run_test("Value conversions", test_value_conversions);
    run_test("Interface query", test_interface_query);
    run_test("Freezing", test_freezing);

    std::printf("\n========================================\n");
    std::printf("  Results: %d / %d assertions", g_passed, g_passed + g_failed);
    if (g_failed > 0)
        std::printf("  \033[31m(%d FAILED)\033[0m", g_failed);
    std::printf("\n========================================\n");

    return g_failed > 0 ? 1 : 0;
}
