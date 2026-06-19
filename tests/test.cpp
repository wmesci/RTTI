#include <cstdio>
#include <cstdlib>
#define RTTI_ENABLE_LOG 1
#include <RTTI.h>
#include <DynamicType.h>

using namespace rtti;
using namespace std::string_literals;

// ============================================================================
// Minimal test framework
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;
static int g_tests = 0;

static int run_test(const char* name, void (*body)())
{
    g_tests++;
    printf("\n  [TEST %d] %s\n", g_tests, name);
    int prev_failed = g_failed;
    body();
    return g_failed == prev_failed ? 0 : 1;
}

#define EXPECT(cond)                                                       \
    do {                                                                   \
        if (cond) {                                                        \
            g_passed++;                                                    \
        } else {                                                           \
            g_failed++;                                                    \
            printf("    \033[31mFAIL\033[0m at %s:%d: %s\n",              \
                   __FILE__, __LINE__, #cond);                             \
        }                                                                  \
    } while (0)

#define EXPECT_EQ(a, b)                                                    \
    do {                                                                   \
        auto _a = (a);                                                     \
        auto _b = (b);                                                     \
        if (_a == _b) {                                                    \
            g_passed++;                                                    \
        } else {                                                           \
            g_failed++;                                                    \
            printf("    \033[31mFAIL\033[0m at %s:%d: %s == %s\n",       \
                   __FILE__, __LINE__, #a, #b);                            \
        }                                                                  \
    } while (0)

#define EXPECT_NE(a, b)                                                    \
    do {                                                                   \
        auto _a = (a);                                                     \
        auto _b = (b);                                                     \
        if (_a != _b) {                                                    \
            g_passed++;                                                    \
        } else {                                                           \
            g_failed++;                                                    \
            printf("    \033[31mFAIL\033[0m at %s:%d: %s != %s\n",       \
                   __FILE__, __LINE__, #a, #b);                            \
        }                                                                  \
    } while (0)

#define EXPECT_TRUE(cond) EXPECT(cond)
#define EXPECT_FALSE(cond) EXPECT(!(cond))
#define EXPECT_STREQ(a, b)                                                 \
    do {                                                                   \
        std::string _a = (a);                                              \
        std::string _b = (b);                                              \
        if (_a == _b) {                                                    \
            g_passed++;                                                    \
        } else {                                                           \
            g_failed++;                                                    \
            printf("    \033[31mFAIL\033[0m at %s:%d: \"%s\" != \"%s\"\n", \
                   __FILE__, __LINE__, _a.c_str(), _b.c_str());            \
        }                                                                  \
    } while (0)

// ============================================================================
// Test helper types
// ============================================================================

constexpr size_t DisplayNameAttr = HASH("DisplayName");

inline std::pair<size_t, std::any> DisplayName(const std::string& name)
{
    return {DisplayNameAttr, name};
}

enum class TestEnum { Value1, Value2 };
enum class TestEnum2 { Value1, Value2 };

struct TestStruct
{
    TestEnum TE = TestEnum::Value1;
    operator int() const { return 250; }
    void Func(int a) { TE = (TestEnum)a; }

    static void RegisterRTTI()
    {
        TypeRegister<TestStruct>::New()
            .constructor<>()
            .convert<int>()
            .property("TE"s, &TestStruct::TE)
            .method("Func"s, &TestStruct::Func);
    }
};

class TestBase : public Object
{
    TYPE_DECLARE(Object)
public:
    static void RegisterRTTI()
    {
        TypeRegister<TestBase>::New()
            .property("TestBaseA"s, &TestBase::TestBaseA)
            .property("TestBaseStr"s, &TestBase::TestBaseStr)
            .method("BaseFunc5"s, &TestBase::BaseFunc5);
    }
    int32_t TestBaseA = 1;
    std::string TestBaseStr;
    void BaseFunc5(int a) { (void)a; }
};

class Test : public TestBase
{
    TYPE_DECLARE(TestBase)
public:
    static void RegisterRTTI()
    {
        TypeRegister<Test>::New()
            .constructor<>()
            .constructor<int>()
            .constructor<int, float>()
            .convert<int>()
            .property("A"s, &Test::A)
            .property("B"s, &Test::B)
            .property("C"s, &Test::C)
            .property("D"s, &Test::D)
            .property("Prop1"s, &Test::GetProp1, &Test::SetProp1)
            .property("Prop2"s, &Test::GetProp2)
            .method("Func3"s, &Test::Func3)
            .method("Func5"s, &Test::Func5)
            .method("Func8"s, &Test::Func8)
            .method("StaticFunc"s, &Test::StaticFunc);
    }

    Test() {}
    Test(int i) { (void)i; }
    Test(int i, float f) { (void)i; (void)f; }

    int32_t A = 1;
    std::string B;
    ObjectPtr C;
    TestEnum D = TestEnum::Value1;
    TestStruct E;

    int GetProp1() { return 0; }
    void SetProp1(int v) { (void)v; }
    int GetProp2() { return 42; }

    int Func3() { return 34; }
    void Func5(int a) { (void)a; }
    void Func8(TestEnum a) { (void)a; }

    static int StaticFunc() { return 99; }

    template <typename T>
    bool ConvertTo(T& target);
};

template <>
bool Test::ConvertTo(int& target)
{
    target = 128;
    return true;
}

struct HandleBase
{
    HandleBase() = default;
    HandleBase(const ObjectPtr& p) : m_ptr(p) {}
    ObjectPtr GetPtr() const { return m_ptr; }
protected:
    ObjectPtr m_ptr;
};

template <typename T>
struct Handle : public HandleBase
{
    Handle() = default;
    Handle(const Ptr<T>& p) : HandleBase(p) {}

    template <typename U>
    Handle(const Handle<U>& h) : HandleBase(h.GetPtr()) {}

    T* operator->() const { return static_cast<T*>(RawFromPtr(m_ptr)); }
    operator Ptr<T>() const { return PtrCast<T>(m_ptr); }

    static void RegisterRTTI()
    {
        TypeRegister<Handle<T>>::New("Handle<"s + GetTypeName<T>() + ">"s, {{HASH("handle"), type_of<T>()}})
            .template constructor<Ptr<T>>()
            .template convert<HandleBase>()
            .template convert<T>();
    }
};

// ============================================================================
// Registration helper
// ============================================================================

void RegisterAllTypes()
{
    InitCoreType();
    TypeRegister<TestEnum>::New({DisplayName("TestEnumForDisplay")})
        .value("Value1", TestEnum::Value1)
        .value("Value2", TestEnum::Value2);
    TestStruct::RegisterRTTI();
    TestBase::RegisterRTTI();
    Test::RegisterRTTI();
    Handle<TestBase>::RegisterRTTI();
    Handle<Test>::RegisterRTTI();
}

// ============================================================================
// 1. Core Type System
// ============================================================================

void test_type_system()
{
    RegisterAllTypes();

    // type_of returns same Type* for Ptr<T> and T
    EXPECT_EQ(type_of<Test>(), type_of<Ptr<Test>>());

    // type_of for pointer types
    auto* ptrType = type_of<int*>();
    EXPECT_TRUE(ptrType->IsPointer());
    EXPECT_EQ(ptrType->GetUnderlyingType(), type_of<int>());

    // IsValueType
    EXPECT_TRUE(type_of<int>()->IsValueType());
    EXPECT_FALSE(type_of<Test>()->IsValueType());

    // IsEnum
    EXPECT_TRUE(type_of<TestEnum>()->IsEnum());
    EXPECT_TRUE(type_of<TestEnum2>()->IsEnum());

    // Type::Find
    EXPECT_EQ(Type::Find("Test"), type_of<Test>());
    EXPECT_EQ(Type::Find(type_of<int32_t>()->GetName()), type_of<int32_t>());
    EXPECT(static_cast<bool>(Type::Find("Test")));
    EXPECT(Type::Find("nonexistent") == nullptr);

    // Type::ForEach
    int count = 0;
    bool found = false;
    Type::ForEach([&](Type* t) { count++; if (t->GetName() == "Test") found = true; });
    EXPECT_TRUE(count > 0);
    EXPECT_TRUE(found);
}

// ============================================================================
// 2. Enum Types
// ============================================================================

void test_enums()
{
    RegisterAllTypes();

    // DefaultEnumRegister
    auto* t2 = type_of<TestEnum2>();
    EXPECT_TRUE(t2->IsEnum());
    EXPECT_TRUE(t2->CanConvertTo<int32_t>());

    // Enum values
    auto* t = type_of<TestEnum>();
    EXPECT_STREQ(t->GetEnumName(Box(TestEnum::Value1)), "Value1");
    EXPECT_STREQ(t->GetEnumName(Box(TestEnum::Value2)), "Value2");

    // GetEnumValue
    auto v = t->GetEnumValue("Value1");
    EXPECT(v != nullptr);
    EXPECT_EQ(cast<TestEnum>(v), TestEnum::Value1);

    // Enum attributes
    EXPECT_STREQ(t->GetAttribute<std::string>(DisplayNameAttr), "TestEnumForDisplay");

    // Enum to underlying type
    auto boxed = Box(TestEnum2::Value1);
    bool ok = false;
    int32_t val = cast<int32_t>(boxed, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(val, static_cast<int32_t>(TestEnum2::Value1));
}

// ============================================================================
// 3. Object Type Registration
// ============================================================================

void test_object_registration()
{
    RegisterAllTypes();
    auto* testType = Type::Find("Test");
    EXPECT(testType != nullptr);

    // Constructors
    auto ctors = testType->GetConstructors();
    EXPECT_TRUE(ctors.size() >= 2);

    // Default constructor
    auto obj = testType->Create<Test>();
    EXPECT(obj != nullptr);
    EXPECT_EQ(obj->GetRttiType(), type_of<Test>());

    // Parameterized constructors
    EXPECT(testType->Create<Test>(123) != nullptr);
    EXPECT(testType->Create<Test>(123, 789.12f) != nullptr);

    // GetConstructor with parameter types
    EXPECT(testType->GetConstructor({type_of<int>()}) != nullptr);

    // Properties
    auto* propA = testType->GetProperty("A");
    EXPECT(propA != nullptr);
    EXPECT_TRUE(propA->CanRead());
    EXPECT_TRUE(propA->CanWrite());

    // Inherited property
    auto* baseProp = testType->GetProperty("TestBaseA");
    EXPECT(baseProp != nullptr);

    // Methods (including inherited)
    EXPECT(testType->GetMethod("Func3") != nullptr);
    EXPECT(testType->GetMethod("StaticFunc") != nullptr);
    EXPECT_TRUE(testType->GetMethod("StaticFunc")->IsStatic());
}

// ============================================================================
// 4. Value Type Boxing / Unboxing
// ============================================================================

void test_boxing()
{
    RegisterAllTypes();

    // Box int
    auto obj = Box(123);
    EXPECT(obj != nullptr);
    EXPECT_EQ(obj->GetRttiType(), type_of<int>());
    EXPECT_EQ(Unbox<int>(obj), 123);

    // Box string
    auto strObj = Box("hello"s);
    EXPECT_EQ(strObj->GetRttiType(), type_of<std::string>());
    EXPECT_EQ(Unbox<std::string>(strObj), std::string("hello"));

    // Hash code
    auto hcObj = Box(128);
    EXPECT_EQ(hcObj->GetHashCode(), std::hash<int>()(128));

    // Unbox as pointer for in-place modification
    auto mutObj = Box<int>(123);
    int* p = Unbox<int*>(mutObj);
    *p = 234;
    EXPECT_EQ(Unbox<int>(mutObj), 234);

    // Boxed pointer type
    int val = 42;
    auto ptrObj = Box(&val);
    EXPECT_EQ(ptrObj->GetRttiType(), type_of<int*>());
    EXPECT_TRUE(ptrObj->GetRttiType()->IsPointer());
}

// ============================================================================
// 5. Type Conversion (cast)
// ============================================================================

void test_cast()
{
    RegisterAllTypes();
    auto obj = MakePtr<Test>();

    // Object to void*
    EXPECT_EQ(cast<void*>(obj), RawFromPtr(obj));

    // Upcast
    auto base = cast<TestBase>(obj);
    EXPECT(base != nullptr);

    // Downcast via RTTI
    auto derived = cast<Test>(base);
    EXPECT(derived != nullptr);

    // Downcast raw pointer
    auto* raw = RawFromPtr(obj);
    EXPECT_EQ(cast<Test*>((TestBase*)raw), raw);

    // Object-to-int via ConvertTo<T>
    ObjectPtr result;
    EXPECT_TRUE(Type::Convert(obj, type_of<int>(), result));
    EXPECT_EQ(cast<int>(result), 128);

    // cast with ok flag
    bool ok = false;
    int v = cast<int>(Box(42), &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(v, 42);

    // Numeric conversions
    auto boxed = Box(128);
    EXPECT_EQ(cast<int>(boxed), 128);
    EXPECT_EQ(cast<uint16_t>(boxed), 128);
    EXPECT_EQ(cast<double>(boxed), 128.0);

    // Struct to int via operator int
    ObjectPtr testStructResult;
    bool structOk = Type::Convert(Box(TestStruct()), type_of<int>(), testStructResult);
    EXPECT_TRUE(structOk);
    EXPECT_EQ(cast<int>(testStructResult), 250);
}

// ============================================================================
// 6. Type Comparison
// ============================================================================

void test_compare()
{
    RegisterAllTypes();

    // is_comparable
    auto c_ee = is_comparable<TestEnum, TestEnum>();
    EXPECT_TRUE(c_ee);
    auto c_ei = is_comparable<TestEnum, int>();
    EXPECT_FALSE(c_ei);
    auto c_id = is_comparable<int, double>();
    EXPECT_TRUE(c_id);
    auto c_is = is_comparable<int, std::string>();
    EXPECT_FALSE(c_is);

    // Compare equals
    EXPECT_EQ(compare(Box(128), 128.0f), CompareResult::Equals);

    // Compare not equals
    EXPECT_EQ(compare(Box(0u), Box(128)), CompareResult::NotEquals);

    // Compare failed (incompatible)
    EXPECT_EQ(compare(Box(128), std::string("xxx")), CompareResult::Failed);

    // Compare symmetry
    auto f = Box(3.0f);
    auto i = Box(3);
    EXPECT_EQ(compare(f, i), compare(i, f));

    // Compare with nullptr
    EXPECT_EQ(compare(ObjectPtr(nullptr), ObjectPtr(nullptr)), CompareResult::Equals);
    EXPECT_EQ(compare(ObjectPtr(nullptr), Box(5)), CompareResult::Failed);
}

// ============================================================================
// 7. Attributes
// ============================================================================

void test_attributes()
{
    RegisterAllTypes();

    auto* t = type_of<TestEnum>();
    EXPECT_TRUE(t->HasAttribute(DisplayNameAttr));
    EXPECT_STREQ(t->GetAttribute<std::string>(DisplayNameAttr), "TestEnumForDisplay");

    // Default value
    EXPECT_STREQ(t->GetAttribute<std::string>(HASH("nonexistent"), "default"), "default");

    // Handle<T> stores inner type as attribute
    Handle<TestBase> ht = Handle<Test>(MakePtr<Test>());
    auto boxed = Box(ht);
    auto* inner = boxed->GetRttiType()->GetAttribute<Type*>(HASH("handle"));
    EXPECT_EQ(inner, type_of<TestBase>());
}

// ============================================================================
// 8. Method Invocation via Reflection
// ============================================================================

void test_method_invocation()
{
    RegisterAllTypes();
    auto obj = MakePtr<Test>();
    auto* testType = type_of<Test>();

    // Void method
    auto* m2 = testType->GetMethod("Func3");
    EXPECT(m2 != nullptr);
    auto ret = m2->Invoke(obj, {});
    EXPECT(ret != nullptr);
    EXPECT_EQ(Unbox<int>(ret), 34);

    // Method with parameter
    auto* m5 = testType->GetMethod("Func5", {type_of<int>()});
    EXPECT(m5 != nullptr);
    m5->Invoke(obj, {Box(42)});

    // Method with conversion: enum → int
    m5->Invoke(obj, {Box(TestEnum::Value1)});

    // Method with conversion: TestStruct → int
    m5->Invoke(obj, {Box(TestStruct())});

    // Method with conversion: Test → int (via ConvertTo)
    m5->Invoke(obj, {MakePtr<Test>()});

    // Static method
    auto* sm = testType->GetMethod("StaticFunc");
    EXPECT(sm != nullptr);
    EXPECT_TRUE(sm->IsStatic());
    auto sr = sm->Invoke(nullptr, {});
    EXPECT_EQ(Unbox<int>(sr), 99);
}

// ============================================================================
// 9. Property Access via Reflection
// ============================================================================

void test_property_access()
{
    RegisterAllTypes();
    auto obj = MakePtr<Test>();
    obj->A = 123;

    // Get field value
    auto* prop = type_of<Test>()->GetProperty("A");
    EXPECT(prop != nullptr);
    EXPECT_TRUE(prop->CanRead());
    EXPECT_EQ(prop->GetValue<int32_t>(obj), 123);

    // Set field value
    prop->SetValue(obj, Box(456));
    EXPECT_EQ(obj->A, 456);

    // Read-only property
    auto* prop2 = type_of<Test>()->GetProperty("Prop2");
    EXPECT(prop2 != nullptr);
    EXPECT_TRUE(prop2->CanRead());
    EXPECT_FALSE(prop2->CanWrite());
    EXPECT_EQ(prop2->GetValue<int>(obj), 42);

    // RW property
    auto* prop1 = type_of<Test>()->GetProperty("Prop1");
    EXPECT(prop1 != nullptr);
    EXPECT_TRUE(prop1->CanRead());
    EXPECT_TRUE(prop1->CanWrite());

    // String property
    obj->B = "test";
    auto* strProp = type_of<Test>()->GetProperty("B");
    EXPECT_STREQ(strProp->GetValue<std::string>(obj), "test");

    // Inherited property
    auto* baseProp = type_of<Test>()->GetProperty("TestBaseA");
    EXPECT(baseProp != nullptr);
    EXPECT_TRUE(baseProp->CanWrite());
}

// ============================================================================
// 10. Handle<T> Wrapper Pattern
// ============================================================================

void test_handle()
{
    RegisterAllTypes();

    // Handle preserves derived type
    Handle<TestBase> ht = Handle<Test>(MakePtr<Test>());
    EXPECT_EQ(ht->GetRttiType(), type_of<Test>());

    // Boxed Handle recovers inner object
    auto boxed = Box(ht);
    auto recovered = cast<HandleBase>(boxed).GetPtr();
    EXPECT_EQ(recovered, ht.GetPtr());

    // Handle converts to HandleBase
    bool ok = false;
    cast<HandleBase>(Box(ht), &ok);
    EXPECT_TRUE(ok);
}

// ============================================================================
// 11. Error and Edge Paths
// ============================================================================

void test_error_paths()
{
    RegisterAllTypes();

    // nullptr assignability
    EXPECT_TRUE(type_of<Test>()->IsAssignableFrom(ObjectPtr(nullptr)));
    EXPECT_FALSE(type_of<int>()->IsAssignableFrom(ObjectPtr(nullptr)));

    // nullptr cast for Object types
    ObjectPtr nullPtr;
    bool ok = true;
    auto result = cast<Test>(nullPtr, &ok);
    EXPECT_TRUE(ok);
    EXPECT(result == nullptr);

    // IsAssignableTo with nullptr
    EXPECT_FALSE(type_of<Test>()->IsAssignableTo(nullptr));

    // Find nonexistent
    EXPECT(Type::Find("NoSuchType") == nullptr);

    // Incompatible conversion
    EXPECT_FALSE(type_of<int>()->CanConvertTo<std::string>());

    // Property without setter
    auto* prop2 = type_of<Test>()->GetProperty("Prop2");
    EXPECT_FALSE(prop2->CanWrite());

    // Constructor with wrong arg count
    auto obj = type_of<Test>()->CreateInstance({Box(1), Box(2), Box(3)});
    EXPECT(obj == nullptr);
}

// ============================================================================
// 12. void*, ObjectBox, string
// ============================================================================

void test_special_types()
{
    RegisterAllTypes();

    // void* registered
    auto* voidPtr = type_of<void*>();
    EXPECT(voidPtr != nullptr);
    EXPECT_TRUE(voidPtr->IsPointer());

    // Box & unbox void*
    int val = 42;
    auto obj = Box(&val);
    EXPECT_EQ(obj->GetRttiType(), type_of<int*>());

    // ObjectBox registered
    EXPECT(type_of<ObjectBox>() != nullptr);

    // String registered and roundtrippable
    auto* strType = type_of<std::string>();
    EXPECT(strType != nullptr);

    auto strBox = Box("roundtrip"s);
    EXPECT_STREQ(Unbox<std::string>(strBox), "roundtrip");

    // String comparison
    EXPECT_EQ(compare(Box("hello"s), Box("hello"s)), CompareResult::Equals);
}

// ============================================================================
// 13. Inheritance
// ============================================================================

void test_inheritance()
{
    RegisterAllTypes();

    // IsSubClassOf
    EXPECT_TRUE(type_of<Test>()->IsSubClassOf(type_of<TestBase>()));
    EXPECT_TRUE(type_of<Test>()->IsSubClassOf(type_of<Object>()));
    EXPECT_FALSE(type_of<TestBase>()->IsSubClassOf(type_of<Test>()));

    // IsAssignableFrom for derived
    EXPECT_TRUE(type_of<TestBase>()->IsAssignableFrom(MakePtr<Test>()));
    EXPECT_FALSE(type_of<Test>()->IsAssignableFrom(MakePtr<TestBase>()));

    // GetBaseType
    EXPECT_EQ(type_of<Test>()->GetBaseType(), type_of<TestBase>());
    EXPECT_EQ(type_of<TestBase>()->GetBaseType(), type_of<Object>());

    // Value type base is ObjectBox
    EXPECT_EQ(type_of<int>()->GetBaseType(), type_of<ObjectBox>());
}

// ============================================================================
// 14. Type::Create and CanConvertTo
// ============================================================================

void test_create_and_convert()
{
    RegisterAllTypes();

    // Create default
    auto obj = type_of<Test>()->CreateInstance({});
    EXPECT(obj != nullptr);

    // Create with matching arg
    auto obj2 = type_of<Test>()->CreateInstance({Box(123)});
    EXPECT(obj2 != nullptr);

    // GetConstructor
    EXPECT(type_of<Test>()->GetConstructor() != nullptr);

    // CanConvertTo
    EXPECT_TRUE(type_of<int32_t>()->CanConvertTo<uint64_t>());
    EXPECT_TRUE(type_of<int>()->CanConvertTo<float>());
    EXPECT_TRUE(type_of<Test>()->CanConvertTo<int32_t>());
    EXPECT_TRUE(type_of<TestStruct>()->CanConvertTo<int>());
    EXPECT_FALSE(type_of<int>()->CanConvertTo<std::string>());
    EXPECT_FALSE(type_of<std::string>()->CanConvertTo<int>());
}

// ============================================================================
// 15. DynamicType — registration, properties, instances
// ============================================================================

void test_dynamic_type_basics()
{
    InitCoreType();

    std::map<std::string, DynamicPropertyDesc> schema;
    schema["x"] = type_of<float>();
    schema["y"] = type_of<float>();
    schema["name"] = type_of<std::string>();

    auto* dt = NewDynamicType("DynVec3", schema);
    EXPECT(dt != nullptr);

    // Type::Find
    EXPECT_EQ(Type::Find("DynVec3"), dt);

    // GetName
    EXPECT_STREQ(dt->GetName(), "DynVec3");

    // IsValueType / IsEnum — dynamic types are reference types
    EXPECT_FALSE(dt->IsValueType());
    EXPECT_FALSE(dt->IsEnum());
    EXPECT_FALSE(dt->IsPointer());

    // GetBaseType is Object
    EXPECT(dt->GetBaseType() != nullptr);
    EXPECT_EQ(dt->GetBaseType(), type_of<Object>());

    // GetProperties
    auto props = dt->GetProperties();
    EXPECT_TRUE(props.size() >= 3);

    // GetProperty
    auto* px = dt->GetProperty("x");
    EXPECT(px != nullptr);
    EXPECT_TRUE(px->CanRead());
    EXPECT_TRUE(px->CanWrite());
    EXPECT_EQ(px->PropertyType(), type_of<float>());

    auto* pn = dt->GetProperty("name");
    EXPECT(pn != nullptr);
    EXPECT_EQ(pn->PropertyType(), type_of<std::string>());

    // Non-existent property
    EXPECT(dt->GetProperty("nonexistent") == nullptr);
}

void test_dynamic_create_instance()
{
    InitCoreType();

    std::map<std::string, DynamicPropertyDesc> schema;
    schema["val"] = type_of<int>();

    auto* dt = NewDynamicType("DynInt", schema);

    // Create default instance
    auto obj = dt->CreateInstance({});
    EXPECT(obj != nullptr);

    // Instance has correct RTTI type (DynamicObject is an Object subclass)
    EXPECT(obj->GetRttiType() != nullptr);
    EXPECT(obj->GetRttiType()->IsSubClassOf(type_of<Object>()));

    // GetConstructors
    auto ctors = dt->GetConstructors();
    EXPECT_TRUE(ctors.size() >= 1);

    // GetConstructor (default)
    auto* defCtor = dt->GetConstructor();
    EXPECT(defCtor != nullptr);
    EXPECT_TRUE(defCtor->GetParameters().empty());
}

void test_dynamic_property_read_write()
{
    InitCoreType();

    std::map<std::string, DynamicPropertyDesc> schema;
    schema["x"] = type_of<float>();
    schema["flag"] = type_of<bool>();
    schema["label"] = type_of<std::string>();

    auto* dt = NewDynamicType("DynProps", schema);
    auto obj = dt->CreateInstance({});

    // Set and get float property
    dt->GetProperty("x")->SetValue(obj, Box(3.14f));
    float x = dt->GetProperty("x")->GetValue<float>(obj);
    EXPECT_EQ(x, 3.14f);

    // Set and get bool property
    dt->GetProperty("flag")->SetValue(obj, Box(true));
    bool flag = dt->GetProperty("flag")->GetValue<bool>(obj);
    EXPECT_TRUE(flag);

    // String property
    dt->GetProperty("label")->SetValue(obj, Box("dynamic"s));
    std::string label = dt->GetProperty("label")->GetValue<std::string>(obj);
    EXPECT_STREQ(label, "dynamic");
}

void test_dynamic_independent_instances()
{
    InitCoreType();

    std::map<std::string, DynamicPropertyDesc> schema;
    schema["v"] = type_of<int>();

    auto* dt = NewDynamicType("DynIndependent", schema);
    auto obj1 = dt->CreateInstance({});
    auto obj2 = dt->CreateInstance({});

    // Set different values
    dt->GetProperty("v")->SetValue(obj1, Box(100));
    dt->GetProperty("v")->SetValue(obj2, Box(200));

    EXPECT_EQ(dt->GetProperty("v")->GetValue<int>(obj1), 100);
    EXPECT_EQ(dt->GetProperty("v")->GetValue<int>(obj2), 200);

    // Modify obj1, obj2 unchanged
    dt->GetProperty("v")->SetValue(obj1, Box(300));
    EXPECT_EQ(dt->GetProperty("v")->GetValue<int>(obj1), 300);
    EXPECT_EQ(dt->GetProperty("v")->GetValue<int>(obj2), 200);
}

void test_dynamic_method()
{
    InitCoreType();

    std::map<std::string, DynamicPropertyDesc> schema;
    schema["a"] = type_of<int>();
    schema["b"] = type_of<int>();

    Type* owner = nullptr;
    auto method = NewDynamicMethod(
        owner, "Add", type_of<int>(),
        {{type_of<int>(), false, false}, {type_of<int>(), false, false}},
        [](const ObjectPtr&, const std::vector<ObjectPtr>& args) -> ObjectPtr {
            int a = Unbox<int>(args[0]);
            int b = Unbox<int>(args[1]);
            return Box(a + b);
        });

    EXPECT(method != nullptr);
    EXPECT_STREQ(method->GetName(), "Add");
    EXPECT_EQ(method->ReturnType(), type_of<int>());

    // Register dynamic type with the method
    auto* dt = NewDynamicType("DynMethod", schema, {method});

    auto* foundMethod = dt->GetMethod("Add");
    EXPECT(foundMethod != nullptr);

    // Invoke
    auto obj = dt->CreateInstance({});
    auto result = foundMethod->Invoke(obj, {Box(10), Box(20)});
    EXPECT_EQ(Unbox<int>(result), 30);
}

void test_dynamic_attributes()
{
    InitCoreType();

    std::map<std::string, DynamicPropertyDesc> schema;
    schema["health"] = type_of<int>();

    // DynamicType with attributes
    auto* dt = NewDynamicType("DynAttr", schema, {},
        {{HASH("category"), std::string("entity")}});

    EXPECT_TRUE(dt->HasAttribute(HASH("category")));
    EXPECT_STREQ(dt->GetAttribute<std::string>(HASH("category")), "entity");

    // DynamicMethod with attributes
    auto method = NewDynamicMethod(
        nullptr, "Heal", type_of<void>(), {},
        [](const ObjectPtr&, const std::vector<ObjectPtr>&) -> ObjectPtr {
            return nullptr;
        },
        {{HASH("tooltip"), std::string("Restore HP")}});

    EXPECT_TRUE(method->HasAttribute(HASH("tooltip")));
    EXPECT_STREQ(method->GetAttribute<std::string>(HASH("tooltip")), "Restore HP");
}

void test_dynamic_edge_cases()
{
    InitCoreType();

    // Empty schema
    std::map<std::string, DynamicPropertyDesc> emptySchema;
    auto* emptyType = NewDynamicType("DynEmpty", emptySchema);
    EXPECT(emptyType != nullptr);
    EXPECT_TRUE(emptyType->GetProperties().empty());

    // Create instance of empty type
    auto emptyObj = emptyType->CreateInstance({});
    EXPECT(emptyObj != nullptr);

    // Non-existent property
    EXPECT(emptyType->GetProperty("foo") == nullptr);

    // Non-existent method
    EXPECT(emptyType->GetMethod("bar") == nullptr);

    // DynamicObject helper
    auto* dyn = static_cast<DynamicObject*>(RawFromPtr(emptyObj));
    EXPECT(dyn != nullptr);

    // Get non-existent attribute
    std::string def = dyn->Get("nonexistent") ? "has" : "null";
    // Get returns nullptr for non-existent keys
    EXPECT(dyn->Get("nonexistent") == nullptr);
}

// ============================================================================
// 22. Dynamic property / method / constructor attributes
// ============================================================================

void test_dynamic_per_property_attributes()
{
    InitCoreType();

    // Schema with per-property attributes
    std::map<std::string, DynamicPropertyDesc> schema;
    schema["hp"].type = type_of<int>();
    schema["hp"].attributes = {{HASH("min"), 0}, {HASH("max"), 100}};
    schema["label"].type = type_of<std::string>();
    schema["label"].attributes = {{HASH("max_length"), 32}};

    auto* dt = NewDynamicType("DynPropAttr", schema);

    // Property attributes accessible via reflection
    auto* hpProp = dt->GetProperty("hp");
    EXPECT(hpProp != nullptr);
    EXPECT_TRUE(hpProp->HasAttribute(HASH("min")));
    EXPECT_EQ(hpProp->GetAttribute<int>(HASH("min")), 0);
    EXPECT_EQ(hpProp->GetAttribute<int>(HASH("max")), 100);

    auto* labelProp = dt->GetProperty("label");
    EXPECT_TRUE(labelProp->HasAttribute(HASH("max_length")));
    EXPECT_EQ(labelProp->GetAttribute<int>(HASH("max_length")), 32);

    // Properties without explicit attributes have none
    EXPECT_FALSE(labelProp->HasAttribute(HASH("min")));
}

void test_dynamic_method_attributes()
{
    InitCoreType();

    auto method = NewDynamicMethod(
        nullptr, "Validate", type_of<bool>(), {},
        [](const ObjectPtr&, const std::vector<ObjectPtr>&) -> ObjectPtr {
            return Box(true);
        },
        {{HASH("category"), std::string("validation")},
         {HASH("order"), 1}});

    EXPECT_TRUE(method->HasAttribute(HASH("category")));
    EXPECT_STREQ(method->GetAttribute<std::string>(HASH("category")), "validation");
    EXPECT_EQ(method->GetAttribute<int>(HASH("order")), 1);

    // Register with the method
    auto* dt = NewDynamicType("DynMethodAttr", {{"x", type_of<float>()}}, {method});
    auto* found = dt->GetMethod("Validate");
    EXPECT(found != nullptr);
    EXPECT_TRUE(found->HasAttribute(HASH("category")));
}

void test_dynamic_constructor_attributes()
{
    InitCoreType();

    auto ctor = NewDynamicConstructor(
        nullptr, {{type_of<int>(), false, false}},
        [](const std::vector<ObjectPtr>& args) -> ObjectPtr {
            auto obj = MakePtr<DynamicObject>();
            static_cast<DynamicObject*>(RawFromPtr(obj))->Set("val", args[0]);
            return obj;
        },
        {{HASH("description"), std::string("creates with initial value")}});

    EXPECT_TRUE(ctor->HasAttribute(HASH("description")));
    EXPECT_STREQ(ctor->GetAttribute<std::string>(HASH("description")),
                 "creates with initial value");
}

void test_dynamic_standalone_property()
{
    InitCoreType();

    auto* dt = NewDynamicType("DynStandalone", {});

    // Create a standalone property with attributes and add it to the type
    auto* prop = NewDynamicProperty(dt, "bonus", type_of<float>(),
        {{HASH("default"), 0.0f}});

    EXPECT(prop != nullptr);
    EXPECT_STREQ(prop->GetName(), "bonus");
    EXPECT_TRUE(prop->HasAttribute(HASH("default")));
}

// ============================================================================
// 22. DynamicType subclass (base type inheritance)
// ============================================================================

void test_dynamic_subclass()
{
    InitCoreType();

    // Parent dynamic type
    std::map<std::string, DynamicPropertyDesc> parentSchema;
    parentSchema["name"] = type_of<std::string>();
    auto* parentDt = NewDynamicType("DynParent", parentSchema);

    // Child dynamic type inheriting from parent
    std::map<std::string, DynamicPropertyDesc> childSchema;
    childSchema["hp"] = type_of<int>();
    auto* childDt = NewDynamicType("DynChild", childSchema, {}, {}, parentDt);

    // IsSubClassOf
    EXPECT_TRUE(childDt->IsSubClassOf(parentDt));
    EXPECT_TRUE(childDt->IsSubClassOf(type_of<Object>()));
    EXPECT_FALSE(parentDt->IsSubClassOf(childDt));

    // GetBaseType
    EXPECT_EQ(childDt->GetBaseType(), parentDt);

    // Inherited properties appear in child
    auto* inheritedProp = childDt->GetProperty("name");
    EXPECT(inheritedProp != nullptr);
    EXPECT_EQ(inheritedProp->PropertyType(), type_of<std::string>());

    // Child's own properties
    auto* ownProp = childDt->GetProperty("hp");
    EXPECT(ownProp != nullptr);
    EXPECT_EQ(ownProp->PropertyType(), type_of<int>());

    // GetProperties includes both own + inherited
    auto allProps = childDt->GetProperties();
    EXPECT_TRUE(allProps.size() >= 2);

    // Create instance and use inherited + own properties
    auto obj = childDt->CreateInstance({});
    inheritedProp->SetValue(obj, Box("hero"s));
    ownProp->SetValue(obj, Box(100));
    EXPECT_STREQ(childDt->GetProperty("name")->GetValue<std::string>(obj), "hero");
    EXPECT_EQ(childDt->GetProperty("hp")->GetValue<int>(obj), 100);
}

void test_dynamic_subclass_compile_time()
{
    RegisterAllTypes();
    InitCoreType();

    // Dynamic type inheriting from compile-time TestBase
    std::map<std::string, DynamicPropertyDesc> schema;
    schema["extra"] = type_of<float>();
    auto* dt = NewDynamicType("DynExtendsTestBase", schema, {}, {},
                              type_of<TestBase>());

    // IsSubClassOf
    EXPECT_TRUE(dt->IsSubClassOf(type_of<TestBase>()));
    EXPECT_TRUE(dt->IsSubClassOf(type_of<Object>()));

    // Inherited property from TestBase
    auto* baseProp = dt->GetProperty("TestBaseA");
    EXPECT(baseProp != nullptr);  // TestBaseA is defined on TestBase

    // Own property
    auto* extraProp = dt->GetProperty("extra");
    EXPECT(extraProp != nullptr);

    // Create instance — effective type is set to the DynamicType
    auto obj = dt->CreateInstance({});
    EXPECT(obj != nullptr);
    EXPECT_EQ(obj->GetRttiType(), dt);

    // IsAssignableFrom now works because GetRttiType returns the DynamicType
    EXPECT(type_of<TestBase>()->IsAssignableFrom(obj));
}

// ============================================================================
// main
// ============================================================================

int main()
{
    printf("========================================\n");
    printf("  RTTI Unit Tests\n");
    printf("========================================\n");

    run_test("Core type system",           test_type_system);
    run_test("Enum types",                  test_enums);
    run_test("Object type registration",    test_object_registration);
    run_test("Value type boxing/unboxing",  test_boxing);
    run_test("Type conversion (cast)",      test_cast);
    run_test("Type comparison",             test_compare);
    run_test("Attributes",                  test_attributes);
    run_test("Method invocation",           test_method_invocation);
    run_test("Property access",             test_property_access);
    run_test("Handle<T> wrapper pattern",   test_handle);
    run_test("Error and edge paths",        test_error_paths);
    run_test("void*, ObjectBox, string",    test_special_types);
    run_test("Inheritance",                 test_inheritance);
    run_test("Create and CanConvertTo",     test_create_and_convert);
    run_test("DynamicType basics",           test_dynamic_type_basics);
    run_test("Dynamic create instance",      test_dynamic_create_instance);
    run_test("Dynamic property read/write",  test_dynamic_property_read_write);
    run_test("Dynamic independent instances", test_dynamic_independent_instances);
    run_test("Dynamic method",               test_dynamic_method);
    run_test("Dynamic attributes",           test_dynamic_attributes);
    run_test("Dynamic edge cases",           test_dynamic_edge_cases);
    run_test("Dynamic per-prop attributes",   test_dynamic_per_property_attributes);
    run_test("Dynamic method attributes",      test_dynamic_method_attributes);
    run_test("Dynamic ctor attributes",        test_dynamic_constructor_attributes);
    run_test("Dynamic standalone property",    test_dynamic_standalone_property);
    run_test("Dynamic subclass (dynamic)",     test_dynamic_subclass);
    run_test("Dynamic subclass (compile-time)", test_dynamic_subclass_compile_time);

    printf("\n========================================\n");
    int total = g_passed + g_failed;
    printf("  Results: %d / %d passed", g_passed, total);
    if (g_failed > 0)
        printf("  \033[31m(%d FAILED)\033[0m", g_failed);
    printf("\n========================================\n");

    return g_failed > 0 ? 1 : 0;
}
