#include <cstdio>
#include <RTTI.h>

class ConsumerObject : public rtti::Object
{
    RTTI_TYPE_DECLARE(rtti::Object)
public:
    int Value = 0;
};

int main()
{
    rtti::InitCoreType();

    rtti::TypeRegister<ConsumerObject>::New()
        .constructor<>()
        .property("Value", &ConsumerObject::Value);

    auto object = rtti::MakePtr<ConsumerObject>();
    auto* property = rtti::type_of<ConsumerObject>()->GetProperty("Value");
    property->SetValue(object, rtti::Box(42));

    const int value = property->GetValue<int>(object);
    std::printf("consumer: %d\n", value);
    return value == 42 ? 0 : 1;
}
