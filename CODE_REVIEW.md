# RTTI 库 Code Review 报告

> **状态说明（2026-11-05）**：本报告中的 P0/P1 已全部修复，P2/P3 已按确认方案处理
> （类型/成员/宏重命名、注册表 Freeze/所有权、错误模型、动态组合适配器、性能索引、
> 测试与 CI 等）。具体的 API 变更与迁移步骤见 [MIGRATION.md](MIGRATION.md)。
> 本文件保留原始审查证据。


## 0. 审查元信息

| 项 | 内容 |
|---|---|
| 仓库 | `build/_deps/rtti-src`（`https://github.com/wmesci/RTTI.git`） |
| 版本 | `HEAD = 0a154d3`（`feat: add DynamicType support for runtime type creation`），本地 `main` 与 `origin/main` 同步，工作区干净（本报告除外） |
| 审查日期 | 2026-11-05 |
| 审查范围 | `src/` 13 个头文件（约 4 000 行，含 vendored `nameof.hpp` 1 279 行）、`tests/test.cpp`（1 144 行）、`CMakeLists.txt`、`cmake/RTTIConfig.cmake.in`、`.github/workflows/cmake-multi-platform.yml`、`README.md`、`AGENTS.md`、`.vscode/settings.json` |
| 审查方式 | 逐行静态阅读 + 30+ 个独立编译探针（ASan / UBSan / TSan / NDEBUG）+ 安装包消费测试 + CI/CMake 检查 |
| 环境 | macOS 26.3.1 arm64，Apple clang 21.0.0，CMake Debug 基线测试 `209/209 passed` |
| 限制 | 未在 Windows/Linux 实际编译（Linux LP64 结论基于标准类型定义，并用等价机制复现）；按要求未查看/修改 `build/_deps/rtti-src` 之外的任何文件 |

> 本报告只新增本文件，未修改库的任何源码。审查期间的探针位于 `_review_scratch/`，报告完成后已删除。

---

## 1. 结论摘要

**总体判断**：这是一个"happy path 完整、设计意图清晰"的 C# 风格反射库，header-only、无原生 RTTI 依赖、注册 API 简洁，适合小规模工具/编辑器元数据场景。但作为游戏引擎运行时基础设施，当前状态**不合格**：存在 9 个可导致崩溃、未定义行为或静默错误的 P0，类型注册表缺少所有权与线程模型，动态类型与 C++ 对象模型的交叉继承是根本性设计缺陷，且 CI 实际从未编译/运行过测试。

**最需要立即处理的 5 个问题**

1. **整型 → 枚举转换空指针崩溃**（`Type.h:516`）：`cast<MyEnum>(Box(someInt))` 在参数宽度不精确匹配时直接 SEGV（UBSan 定位 `Type.h:477`）。
2. **动态类型继承编译期类型 = 未定义行为**（`DynamicType.h:160` + `MethodInfo.h:210-226`）：`NewDynamicType(..., baseType=type_of<CompileTimeBase>())` 创建的是 `DynamicObject`，却被 `static_cast<Base*>` 当作真实子类访问字段/方法；测试只断言属性"存在"，从未调用它。
3. **`Unbox` 无类型校验 + boxed 指针语义错误**（`ObjectBox.h:63-66/84/129`）：Release 下 `Unbox<int>(Box(3.0f))` 静默读出 float 位模式；`Unbox<void*>(Box(&v))` 返回的是 box 内部指针槽地址，而不是 `&v`。
4. **反射调用参数转换失败后仍以默认值执行**（`MethodInfo.h:9-17`）：`Invoke(obj, "hello")` 会以空串调用目标方法；`SetValue` 传错类型会把字段写成 0。
5. **类型注册不幂等且无生命周期管理**（`Type.h:58`、`TypeRegister.h:45-85`）：重复注册线性累积构造/方法/属性并永久泄漏；100 次注册后同一类型有 100 份成员，产生 +594 次活跃分配 / ~66 KB 不可回收内存。引擎热重载/PIE 重启会直接踩中。

> 紧随其后的 P0-09：可变参数 `Invoke(obj, somePtr)` 会把对象装箱成 `shared_ptr` 值类型，方法收到错误对象（见 §4 P0-09），同样需要立即修复。

**质量指标（本次实测）**

| 指标 | 数值 |
|---|---|
| 现有测试 | 27 个用例 / 209 条断言，Debug 全绿 |
| 新发现缺陷 | P0 = 9，P1 = 15，P2 = 27，P3 = 12 |
| 反射调用开销 | 4 次堆分配 / 调用（直接调用 0 次）；`Box(int)` 1 次分配 |
| 属性查找（200 个属性） | 约 656 ns / 次（线性字符串比较） |
| 重复注册 100 次 | 活跃分配 +594，约 +66 KB，成员条目 ×100 |
| CI | 传错 CMake 选项名，测试目标从未构建，`ctest` 空跑即绿 |

---

## 2. 严重度定义

| 级别 | 判定标准 |
|---|---|
| **P0** | 可复现的崩溃 / 内存安全 / 未定义行为 / 静默的错误结果 / 数据竞争；必须在任何生产使用前修复 |
| **P1** | 严重设计缺陷或 API 契约不成立，会在正常使用路径上造成资源泄漏、难排查的错误或不可扩展；应在下一个集成版本前修复 |
| **P2** | 可维护性、可读性、一致性、性能与打包问题，不阻塞当前使用但会持续放大成本 |
| **P3** | 纯风格 / 注释 / 死代码 / 文档小错 |

---

## 3. 问题总表

| ID | 级别 | 类别 | 位置 | 标题 |
|---|---|---|---|---|
| P0-01 | P0 | 正确性/崩溃 | `Type.h:513-517` | 整型到枚举转换使用 nullptr 底层类型，空指针解引用 |
| P0-02 | P0 | 架构/UB | `DynamicType.h:160`, `MethodInfo.h:206-227` | 动态类型继承编译期类型导致非法 `static_cast` |
| P0-03 | P0 | 内存安全/语义 | `ObjectBox.h:63-66,84,92-131` | `Unbox` 无类型校验；boxed 指针 `void*` 解箱错误 |
| P0-04 | P0 | 正确性/静默错误 | `MethodInfo.h:9-17`, `:230` | 参数转换失败后仍以默认值调用目标 |
| P0-05 | P0 | 健壮性/崩溃 | `Type.h:189-193,463-477` | 公共 API 对 `Type* == nullptr` 无保护 |
| P0-06 | P0 | 健壮性/死循环 | `Type.h:603-607` | `ForEach` 期间注册类型会被遍历到，可能无限循环 |
| P0-07 | P0 | 并发/数据竞争 | `Type.h:58,612-616` | 全局类型注册表无同步，懒创建并发写 map |
| P0-08 | P0 | 安全模型 | `ObjectBox.h:84,100,110,123` | 所有运行时校验仅依赖 `assert`，NDEBUG 下全部消失 |
| P0-09 | P0 | 静默错误/类型混淆 | `MethodInfo.h:175-179`；`ObjectBox.h:69-75` | 可变参数 `Invoke` 把对象实参装箱成值类型，方法收到错误的 `ObjectPtr` |
| P1-01 | P1 | 生命周期/资源 | `Type.h:50-59,612-616`；`TypeRegister.h` | 注册表无所有权、无注销、注册不幂等、永久泄漏 |
| P1-02 | P1 | 类型标识 | `Reflection.h:87-93`；`Type.h:58`；`RTTI.h:205-311` | 短名去命名空间导致碰撞；重命名丢键；LP64 下 `int64_t`/`long` 冲突 |
| P1-03 | P1 | 自定义指针契约 | `Config.h:80`；`TypeRegister.h:33`；`System.h:36-40` | `PtrFromRaw<T>` 对 T≠Object 不可编译；构造函数绕过 pointer_ops |
| P1-04 | P1 | API 契约 | `Type.h:672-810` | `cast<>` 组合不一致：`Ptr<T>→值`、`int*`、`void*`、引用目标 |
| P1-05 | P1 | API 限制 | `MethodInfo.h:32`；`TypeRegister.h:147-198` | 非 const 引用参数无法注册（out 参数不可用）；getter/setter 签名强耦合；`New({})` 歧义 |
| P1-06 | P1 | 语义 | `Type.h:499-517,555-593`；`TypeRegister.h:11-24` | 转换/比较不沿继承链继承；对象类型不可比较；对象 comparer 不能编译 |
| P1-07 | P1 | 语义/不确定性 | `Type.h:568-571` | `Compare` 按 Type 对象地址选 comparer，结果随注册顺序变化 |
| P1-08 | P1 | 动态类型完整性 | `DynamicType.h:143-246` | 重载隐藏、基类容器为空、`m_schema` 死成员、构造按参数个数匹配、owner 为空、构造工厂不可挂载、`m_func` 被 move 空 |
| P1-09 | P1 | 动态对象安全 | `DynamicType.h:14-81` | `m_properties` 公开、读写不做 schema 校验、读操作插入条目、跨类型属性键冲突 |
| P1-10 | P1 | 错误模型 | `PropertyInfo.h:47-62`；`Type.h:681-805`；`Config.h:107-131` | 失败/void 成功都用 nullptr；`SetValue` 静默失败；`ok` 被忽略；日志默认关闭且用 printf |
| P1-11 | P1 | 性能 | `ObjectBox.h:69-75`；`Type.h:341-460`；`MethodInfo.h` | 每次 Box/Invoke 堆分配；属性/方法 O(n) 字符串查找；重复注册放大扫描成本 |
| P1-12 | P1 | 工程/质量门 | `.github/...:64`；`CMakeLists.txt:40` | CI 使用错误选项名，测试从未运行；无 sanitizer/macOS/安装验证 |
| P1-13 | P1 | 生命周期/UAF | `Type.h:50,62`；`:58` | `Type` 可栈构造/拷贝，析构不注销，`Find` 返回悬垂指针 |
| P1-14 | P1 | API 契约 | `TypeRegister.h:94-138` | 对象转换依赖未声明的 `ConvertTo<T>` 成员，README 示例不可编译 |
| P1-15 | P1 | 类型语义 | `ObjectBox.h:69-75`；`Reflection.h:13-28` | `Box(Ptr<T>)` 把 handle 装箱成值类型，与 `type_of<Ptr<T>>` 矛盾且类型名碰撞 |
| P2-01 | P2 | 命名 | 全局 | 访问器/动词/成员前缀三套风格并存 |
| P2-02 | P2 | 命名/语义 | 全局 | `warper`、`TypeConvertor`、`GetEnumInfos`、`IsSubClassOf` 等命名与语义 |
| P2-03 | P2 | 宏/全局污染 | `System.h:2-6`；`Config.h:107`；`System.h:92` | 保留标识符宏、未前缀宏 `HASH`/`TYPE_DECLARE`/`RETURN` |
| P2-04 | P2 | const 正确性 | `Type.h:189-218`；`MethodInfo.h:91` | 大量 `const_cast`/非 const `Type*`/`GetParameters` 非 const |
| P2-05 | P2 | 诊断质量 | 多处 | 大量无消息 `static_assert`；注释掉的死代码 |
| P2-06 | P2 | 属性系统 | `Attributable.h`；`TypeRegister.h:45-85` | hash-only 键、`std::any` 复制、`New` 覆盖属性、无法枚举/检测碰撞 |
| P2-07 | P2 | 枚举 API | `Type.h:117-144` | `GetEnumValue` O(n)；别名顺序按字母；`CanConvertTo` 与 `Convert` 不一致 |
| P2-08 | P2 | 尺寸/类型 | `Type.h:53,70`；`DynamicType.h:161` | `m_size` 截断为 uint32；动态类型 size=0；size 语义（payload vs box）不明 |
| P2-09 | P2 | 查找/覆盖 | `Type.h:341-460` | `GetMethods/GetProperties` 重复基类条目；无去重/override 处理 |
| P2-10 | P2 | 死代码 | `TypeRegister.h:302-312` | `select_overload` 对所有显式签名调用均不可编译 |
| P2-11 | P2 | 头文件卫生 | `MethodInfo.h:1-5`；`Config.h:3,107` | 依赖传递包含（tuple/vector/utility/cstdio） |
| P2-12 | P2 | 依赖管理 | `src/nameof.hpp` | vendored 1 279 行第三方库无升级路径、无文档；被 `Object.h→Reflection.h` 全量包含 |
| P2-13 | P2 | 类型封装 | `Type.h:50,62,612-616` | 封装细节（与 P1-13 同源）：构造/拷贝/删除/注册表可变 |
| P2-14 | P2 | 静态初始化 | `Type.h:612-616` | 注册表析构顺序与 DSO 可见性风险 |
| P2-15 | P2 | 日志 | `Config.h:103-131` | 默认无日志、printf 风格、无法对接引擎日志 |
| P2-16 | P2 | 哈希契约 | `ObjectBox.h:31-37`；`Type.h:555-593` | `Boxed::GetHashCode` 与 `Compare` 语义不闭合；浮点 NaN/-0 |
| P2-17 | P2 | ForEach API | `Type.h:603-607` | 接受 `std::function`（可能分配）、回调可变、无快照 |
| P2-18 | P2 | 异常安全 | `TypeRegister.h`；`Type.h:58` | `new` 抛异常时部分状态/泄漏 |
| P2-19 | P2 | 构建 | `CMakeLists.txt:10-21,40` | 全局 `CMAKE_CXX_STANDARD`、`GLOB_RECURSE`、通用选项名 |
| P2-20 | P2 | 打包 | `CMakeLists.txt:30,65-95` | 安装 include 布局与 README 不一致；无 `ARCH_INDEPENDENT`；版本号落后 |
| P2-21 | P2 | 文档 | `README.md` | 快速开始示例不可编译、include 路径错误、缺 `InitCoreType` 说明（详见附录 C） |
| P2-22 | P2 | 合规 | 仓库根目录 | 声称 MIT 但无 LICENSE 文件；vendored nameof 有许可头 |
| P2-23 | P2 | 测试设计 | `tests/test.cpp` | 全局计数器、每用例重复注册、断言过弱、无隔离/死亡测试 |
| P2-24 | P2 | CI 矩阵 | workflow `20-48` | `windows-latest + gcc` 组合没有 `cpp_compiler`；无 macOS/sanitizer/安装验证；`checkout@v3` |
| P2-25 | P2 | 编辑器配置 | `.vscode/settings.json` | 同样使用错误的 `RTTI_BUILD_TEST` |
| P2-26 | P2 | 语义边界 | `Type.h:168-180`；`RTTI.h` | 值类型 `IsAssignableTo(Object)` 语义混淆；内置数值转换图不对称 |
| P2-27 | P2 | 文档一致性 | `AGENTS.md` | "Compare 对称性"表述过度；`InitCoreType` 初始化要求描述不足 |

| ID | 级别 | 位置 | 标题 |
|---|---|---|---|
| P3-01 | P3 | `tests/test.cpp:930,1018` | 章节编号重复 "22." |
| P3-02 | P3 | `Type.h:809-810` | 重复且错位的 `} // namespace rtti` 注释 |
| P3-03 | P3 | `TypeRegister.h:9` | 未使用的 `empty_string` 全局对象 |
| P3-04 | P3 | `MethodInfo.h:7-36` | 匿名命名空间内再写 `static` |
| P3-05 | P3 | `Type.h:16-18,32-34` | 注释掉的 `SourceType` 死成员 |
| P3-06 | P3 | `MethodInfo.h:147-151` | `MethodFlags` 无位运算却按位测试 |
| P3-07 | P3 | `tests/test.cpp:1137-1138` | "209/209" 是断言数而非用例数，易误读 |
| P3-08 | P3 | `System.h:89-92` | `force_consteval` 命名不表达意图 |
| P3-09 | P3 | `System.h:70-87` | `hash_string(size=0)` 用 0 作哨兵，无法表达空前缀 |
| P3-10 | P3 | `Type.h:623` | `m_underlyingType = nullptr` 与构造函数初始化重复 |
| P3-11 | P3 | `.clang-format` | `BreakBeforeBraces` 重复配置 |
| P3-12 | P3 | 全部 | 格式漂移（部分 lambda/初始化列表未遵循 Allman） |

---

## 4. P0 详解

### P0-01 整型 → 枚举转换空指针解引用（崩溃）

**位置**：`src/Type.h:513-517`（触发点 `Type.h:477`），`src/Reflection.h:117-122`

```cpp
if (sourceType->HasFlag(TypeFlags::Integral) && targetType->IsEnum())
{
    ObjectPtr underlyingObj = nullptr;
    return Convert(obj, sourceType->GetUnderlyingType(), underlyingObj) && ...;
}
```

`GetUnderlyingType()` 只对枚举/指针类型非空；对所有整型（`CreateType` 的 Integral 分支传 `nullptr`）返回 `nullptr`，于是 `Convert(obj, nullptr, ...)` 在 `Type.h:477` 对空 `targetType` 调用成员函数。

**证据（probe02，ASan+UBSan）**：

```
enum class Small : uint8_t { A = 1, B = 2 };
cast<Small>(Box(uint8_t(2)))   -> ok=1 v=2       // 恰好走构造路径
cast<Small>(Box(int(2)))       -> runtime error: member call on null pointer of type 'rtti::Type'
                                  at src/Type.h:477:25
                                  AddressSanitizer: SEGV ... in rtti::Type::Convert
```

`Type::CanConvertTo` 对同样的转换返回 `true`（`Type.h:199-203`），所以调用方无法提前规避。

**影响**：任何"把整数传给枚举参数"的反射调用/属性赋值/脚本绑定都会崩溃；这是脚本与序列化层最常见的路径之一。宽度不一致（`uint8_t`/`int64_t` → `int32_t` 底层枚举）时必现。

**修复建议**：

1. 整型→枚举应转到**目标枚举的**底层类型，再走构造：`Type* under = targetType->GetUnderlyingType(); Convert(obj, under, tmp); return Convert(tmp, targetType, target);`
2. 所有 `Convert` 入口先判 `targetType == nullptr` 并返回 `false`。
3. 让 `CanConvertTo` 与 `Convert` 共用同一个谓词，避免"声称可转、实际崩溃"。
4. 增加 `uint8_t/int64_t/unsigned → 各底层宽度枚举` 的正反向测试。

---

### P0-02 动态类型继承编译期类型 = 非法 `static_cast`（UB/堆破坏）

**位置**：`src/DynamicType.h:155-182`（`baseType` 参数与 `Type` 基类）、`src/MethodInfo.h:206-227`（`static_cast<SelfType*>(RawFromPtr(target))`）

`NewDynamicType(..., baseType = type_of<TestBase>())` 创建的是 **`DynamicObject`** 实例（`DynamicType.h:168-173`），C++ 层面与 `TestBase` 无继承关系；但类型系统把 `DynamicType::GetBaseType()` 设为 `TestBase`，于是：

- `type_of<TestBase>()->IsAssignableFrom(dynamicObject)` 返回 `true`；
- 调用继承属性的 getter 时，`MethodInfo` 先通过 `IsAssignableFrom` 校验，然后执行 `static_cast<TestBase*>(RawFromPtr(target))`；
- 该指针实际指向 `DynamicObject`，读取 `TestBase::TestBaseA` 读到的是 `std::map` 的内部字节；对 `std::string` 字段则是越界读/写。

**证据（probe05，ASan+UBSan）**：

```
dynamic object actual C++ type: DynamicObject at 0x6070000000a8
GetRttiType()->GetName() = DynExtendsBase
Base::IsAssignableFrom(obj) = 1
cast produced Base* = 0x6070000000a8 (same address as DynamicObject => not a Base)
  Base::Speak this=0x6070000000a8 name='�'      // std::string 读取垃圾
--- inherited property read through reflection ---
Name = �
```

现有测试 `tests/test.cpp:1082-1084` 只断言 `GetProperty("TestBaseA") != nullptr`，从未调用 `GetValue/SetValue`，因此完全掩盖了该问题；`tests/test.cpp:1095` 还把这个不成立的 `IsAssignableFrom` 固化为"正确行为"。

**影响**：编辑器/脚本一旦用动态类型扩展一个编译期组件并读写继承属性，就会读到垃圾或破坏堆；`cast<TestBase>(dynObject)` 还会返回一个类型完全错误的 `Ptr<TestBase>`，后续任何字段访问都是 UB。这是整个 DynamicType 设计中风险最高的一项。

**修复建议（按优先级）**：

1. **禁止**动态类型继承编译期类型：`NewDynamicType` 在 `baseType` 不是 `DynamicType` 时拒绝并返回 `nullptr`/报错；同步修正 `IsAssignableFrom` 对动态类型的答案。
2. 若必须支持"扩展编译期基类"，需要真正的适配层：`DynamicObject` 内部持有 `Ptr<Base>` 实例，`cast<Base>` 返回该实例，方法调用走该实例，而不是把 `DynamicObject*` 强转。
3. 在 `MethodInfo::Register` 的调用路径增加"目标 RTTI 是动态类型但 SelfType 是编译期类"的显式拒绝，作为纵深防御。
4. 重写该测试：不仅检查属性存在，还要 `GetValue/SetValue` 并断言不发生 UB（sanitizer 下运行）。

---

### P0-03 `Unbox` 无类型校验 + boxed 指针解箱语义错误

**位置**：`src/ObjectBox.h:63-66`（`GetPointer`）、`:84/:100/:110/:123`（assert）、`:127-131`（按值分支无检查）、`src/Type.h:749-753`（`cast<void*>`）

**(a) 按值解箱完全没有 RTTI 检查**

```cpp
else
{
    auto boxed = static_cast<Boxed<T>*>(objbox);   // 未校验实际类型
    return boxed->Unbox();
}
```

唯一相关的 `IsBoxedType()` 校验在 `ObjectBox.h:83` 被注释掉，只剩 `assert(ptr != nullptr)`。

**证据（probe03 / probe16）**：

```
Unbox<int>(Box(3.0f)) = 1077936128   // 3.0f 的位模式，静默类型混淆

# -DNDEBUG -O2
Unbox<int*>(Box(std::string("abcdefgh"))) = 0x1014c80d0, *p = 1684234849
```

`Unbox<int>(MakePtr<Test>())` 会把非 `ObjectBox` 的 `Object` 当成 `Boxed<int>`，读取 `Boxed` 的字段位置，属于 UB；任意脚本传入错误类型的 `ObjectPtr` 都可触发。

**(b) boxed 指针的 `void*` 解箱返回错误值**

`Boxed<T>::GetPointer()` 永远返回 `&object`。对 `T = int*`，它返回 `int**`（指针槽地址）；而 `Unbox<void*>` 的指针分支（`ObjectBox.h:92-96`）直接返回它。

**证据（probe03）**：

```
&val                 = 0x16faba9d0
Unbox<void*>(b)      = 0x6060000016d0   (预期 &val)
cast<void*>(b)       = 0x6060000016b8   (返回 Object*，既不是 &val 也不是指针槽)
Unbox<int*>(b)       = 0x16faba9d0      (正确)
```

同时 `cast<void*>(Ptr<Object>)` 在 `Type.h:749-753` 直接返回 `RawFromPtr(from)`，无法区分"对象地址"与"boxed 原始指针值"。

**影响**：`void*` 是脚本/序列化/原生互操作最常见的类型之一；`Unbox<void*>(Box(somePtr))` 返回悬垂的 box 内部地址（box 释放后即失效），解引用会直接崩溃或读到随机数据。

**修复建议**：

1. 把"解箱"拆成两个明确语义的 API：
   - `bool TryUnbox<T>(const ObjectPtr&, T& out)` / `std::optional<T>`：永远做 RTTI 校验（Release 也做）；
   - `void* GetAddressOfValue()`（仅值类型）与 `void* GetPointerValue()`（仅指针载荷），去掉含糊的 `GetPointer()`。
2. `Unbox<void*>` 的实现必须按载荷区分：`T` 为指针时返回 `boxed->Unbox()`（指针值），否则返回 `&boxed->Unbox()`；`void*` 无法静态区分时，让 `ObjectBox` 提供 `bool IsPointerPayload()` + 两个专用虚函数。
3. 删除 `assert` 守卫，改为返回失败；不把 `type_of<T>()` 放进 `assert`（见 P0-08）。

---

### P0-04 参数转换失败后仍以默认值调用目标（静默错误结果）

**位置**：`src/MethodInfo.h:9-17`、`:230`；`src/PropertyInfo.h:47-62`

```cpp
bool ok = true;
Tuple result{cast<...>(p[I], &ok)...};
if (!ok)
    RTTI_WARNING("argument conversion failed in MakeArgs");
return result;      // 继续用默认构造的 T{} 返回值
```

`MakeArgs` 失败只打一条（且默认被编译掉的）warning，随后 `MethodInfo` 的 lambda 照样 `std::apply(... f(...))`。

**证据**：

- probe04：`TakeInt(int)` 用 `Box(std::string("not an int"))` 调用 → 实际执行 `TakeInt(0)`，且返回 `Box(0)`；
- probe04：`SetValue(obj, Box("hello"))` 写 `int` 属性 → 字段被改成 `0`；
- probe27：`Invoke(o, "hello")` 传给 `const std::string&` → 实际执行 `Say("")`（空串）。

**影响**：最危险的一类 bug——没有异常、没有失败返回，只是"参数变成 0/空串/默认值"。脚本调用、网络/存档反序列化、编辑器改属性都会静默产生错误数据。

**修复建议**：

1. `MakeArgs` 返回 `std::optional<Tuple>`/`bool`，失败时 `Invoke` 立即返回失败状态，**绝不调用目标**；
2. `SetValue` 返回 `bool`（或 `Result`），并在类型不匹配时拒绝写入；
3. 参数个数检查从 `args.size() < sizeof...(Args)`（`MethodInfo.h:190`）改成 `!=`，多余参数同样是调用方错误；
4. 为失败路径增加断言：`Invoke` 返回失败后目标方法不得被调用（可用计数器测试）。

---

### P0-05 公共 API 对空 `Type*` 无保护（崩溃）

**位置**：`src/Type.h:189-218`（`CanConvertTo`）、`:463-477`（`Convert`），以及 `:205`、`:489` 等直接解引用 `targetType`

**证据（probe08）**：

```
type_of<int>()->CanConvertTo(nullptr)
runtime error: member call on null pointer of type 'rtti::Type'  (Type.h:193)
AddressSanitizer: SEGV ... in rtti::Type::CanConvertTo(Type*) const
```

**影响**：`CanConvertTo`/`Convert` 是公开静态 API，`GetEnumValue` 等内部路径也会传入派生指针；任何配置驱动的 "目标类型找不到"（`Find` 返回 null）都会把 null 直接传进来。

**修复建议**：

1. 所有接受 `Type*` 的公共入口做 `nullptr` 检查，返回 `false`/`nullptr` 并记录错误；
2. 尽量把参数改为 `const Type*`，并在 `Type::Find` 的调用点显式处理未找到；
3. 增加 `Convert(nullptr)`、`CanConvertTo(nullptr)`、`NewType(..., base=nullptr)` 单测。

---

### P0-06 `ForEach` 期间注册类型会被遍历到，可无限循环

**位置**：`src/Type.h:603-607`

```cpp
static void ForEach(const std::function<void(Type*)>& callback)
{
    for (auto& entry : GetRegistry())
        callback(entry.second);
}
```

range-for 在循环开始时缓存 `end()`；`std::map` 插入不会使迭代器失效，因此**排序在当前节点之后的新类型会被本次遍历访问**。若回调内部调用 `type_of<T>()`（很常见：按名字懒查类型/序列化器）且每次插入新类型，遍历永不结束。

**证据（probe21c）**：初始 17 个类型，回调中插入 3 个 `zzz_inserted_*` 动态类型，本次 `ForEach` 访问了 20 次（插入的类型被遍历到）；不加保护时每次插入都会继续扩展遍历。

**影响**：类型枚举（编辑器类型浏览器、序列化器注册、代码生成）如果触发懒注册，可能卡死主线程。

**修复建议**：

1. 遍历前把 key/指针快照到 `std::vector`，对快照迭代；或
2. 文档明确禁止在回调中修改注册表，并在 Debug 下用"注册表版本号"检测后断言/报错；
3. 更彻底：注册表初始化后冻结（freeze），运行时 `type_of` 未注册类型直接报错（见 P1-13）。

---

### P0-07 类型注册表数据竞争（多线程 UB）

**位置**：`src/Type.h:58`（`GetRegistry()[name] = this;`）、`:612-616`（普通 `static std::map`）

`type_of<T>()` 的函数内 `static` 初始化对**同一个 T** 是线程安全的，但不同 T 的懒创建会并发写同一个 `std::map`；`Type::Find` 与懒注册之间也没有同步。`AGENTS.md:86` 只以文字要求"单线程启动阶段注册"，没有任何运行时保证。

**证据（probe12，TSan）**：

```
WARNING: ThreadSanitizer: data race
  Read of size 8 ... rtti::Type::Type(...) [Type::GetRegistry 插入]
  Previous write of size 8 ... rtti::Type::Type(...) [另一线程插入]
  Location is heap block ... in std::map ...
```

**影响**：渲染/任务线程只要首次触碰未注册类型（`Box`、`cast`、属性读取都会触发 `type_of`），就可能破坏红黑树；表现为随机崩溃或 `Find` 返回错误指针。对 HaloEngine 这类多线程引擎是硬阻塞问题。

**修复建议**：

1. 注册表加 `std::mutex`（写）与 `std::shared_mutex`（读），或注册阶段用 `std::once_flag`；
2. 更优：初始化完成后把注册表发布为**不可变快照**（`shared_ptr<const Map>`），读路径无锁；懒注册改为"初始化阶段一次性注册所有类型"；
3. 若坚持"运行时动态类型"，对 `NewDynamicType` 加锁并明确其线程约束；
4. 增加 TSan CI 与并发 `type_of` 测试。

---

### P0-08 安全模型完全依赖 `assert`，NDEBUG 下消失（且改变注册副作用）

**位置**：`src/ObjectBox.h:84,100,110,123`；`src/DynamicType.h:67,74`

所有"类型不匹配"防护都是 `assert`。Release（NDEBUG）下：

- `Unbox<T>` 的任何检查都不存在（probe16 已在 Release 复现类型混淆读）；
- `DynamicPropertyInfo::GetValue/SetValue` 只检查 `obj != nullptr`，之后无条件 `static_cast<DynamicObject*>`；
- 更隐蔽的是：`assert(ptr->GetRttiType() == type_of<T>())` 里的 `type_of<T>()` 在 NDEBUG 下**不会执行**，于是 Debug 与 Release 下注册的类型集合不同（例如 `Unbox<int*>` 在 Debug 会注册 `int*` 类型，Release 不会），造成行为/序列化差异。

**影响**：库只在 Debug 下"看起来安全"；Release 是实际交付形态，所有防护失效。

**修复建议**：

1. 引入始终生效的 `RTTI_ASSERT(cond, msg)`（记录错误并按可控策略返回/终止），或直接把校验做成 `if (!cond) return false;`；
2. 禁止在断言表达式里写有副作用的调用（`type_of`、`Box`、`new`）；
3. CI 增加 `-DNDEBUG` + sanitizer 的 Release 测试 job；
4. 提供 `checked_cast`/`try_unbox` 作为公开的安全 API，让 `Unbox` 只作为内部快速路径。

---

### P0-09 可变参数 `Invoke` 把对象实参装箱成值，方法收到错误的 `ObjectPtr`

**位置**：`src/MethodInfo.h:175-179`、`src/ObjectBox.h:69-75`、`src/System.h:94-104`

```cpp
template <typename... Args>
ObjectPtr Invoke(const ObjectPtr& target, Args... args)
{
    return m_func(target, {Box(args)...});   // 所有实参一律 Box
}
```

`is_object<Ptr<T>>` 对 `std::shared_ptr<T>` 为 false（`System.h:98`），因此 `Box(MakePtr<Target>())` 会创建 `Boxed<std::shared_ptr<Target>>`——一个**值类型**——而不是传递原对象。随后 `MakeArgs` 把该 box 当作 `ObjectPtr` 传入形参；方法里 `p->GetRttiType()` 得到的是 `"shared_ptr"`，不是 `Target`。

**证据（probe30）**：

```
[vector overload: {MakePtr<Target>()}]
  received type: Target                 // 正确：vector 重载直接传 ObjectPtr
[variadic overload: Invoke(sink, MakePtr<Target>())]
  received type: shared_ptr             // 错误：对象被装箱成值
```

同一个方法、同一个实参，仅因选择了"便利"的可变参数重载就收到不同对象；若形参是 `Target`（而非 `ObjectPtr`），`cast<Target>` 失败后又触发 P0-04，以空指针调用目标，极可能崩溃。

**影响**：`Invoke(obj, somePtr)` 是最自然的调用写法；它会静默传递错误对象，且脚本层无法从返回的 `nullptr` 区分成功/失败。

**修复建议**：

1. 可变参数重载对 `is_object<T>` 的实参应直接传递 `Ptr<T>`（`TypeWarper<T>::vartype`），不要 `Box`；
2. 或者删除可变参数重载，只保留 `span<const ObjectPtr>` 版本，强制调用方明确装箱；
3. `Box` 对 `ObjectPtr`/`Ptr<T>` 加 `static_assert`/`= delete`，禁止把 handle 当值装箱；
4. 增加"对象实参经两种重载结果一致"的测试。

---

## 5. P1 详解

### P1-01 注册表无所有权、无注销、注册不幂等、永久泄漏

**位置**：`src/Type.h:50-59`、`:612-616`；`src/TypeRegister.h:45-145`；`src/MethodInfo.h:134,284`；`src/DynamicType.h:168-177,265-266`

- `Type` 由 `new Type` 创建，注册表只存裸指针，`~Type` 是 public 但从不调用；`ConstructorInfo`/`MethodInfo`/`PropertyInfo` 也全部 `new` 且无 owner。
- `TypeRegister<T>::New()`/`constructor<>()`/`convert<>()`/`compare<>()` 无条件 `push_back`，重复调用线性累积；`InitCoreType()` 无幂等保护。
- 没有 `Unregister`/`Reset`，也没有"注册 owner/模块"概念。

**证据（probe07 / probe17）**：

```
after 2x InitCoreType: int constructors=4
after 1x register: ctors=1 methods=1 props=1
after 3x register: ctors=3 methods=3 props=3
1 registration  -> live allocs +9 (+664 bytes)
99 more registrations -> live allocs +594 (+66408 bytes)
100 registrations -> ctors=100 methods=100 props=100
```

**影响**：

- 引擎 PIE/重载/脚本热更时重复调用注册函数 → 成员重复、查找命中旧条目、内存持续增长；
- 若将来模块/插件卸载，`MethodInfo` 中的函数指针指向已卸载代码，调用即崩溃；
- 泄漏是"process-lifetime"设计，但没有任何上限或文档化的卸载路径，不适合长生命周期编辑器。

**建议**：注册表持有 `std::unique_ptr<Type>`；提供 `Registry::Freeze()`/`Reset()`；注册函数做幂等（按成员签名字符串去重或先清空再注册）；把 `InitCoreType` 包进 `std::once_flag`；为动态类型提供显式 `DestroyDynamicType`（并保证其上对象先失效）。

---

### P1-02 类型标识：短名碰撞、重命名丢键、LP64 下 `int64_t`/`long` 冲突

**位置**：`src/Reflection.h:87-93`（`GetTypeName` 使用 `NAMEOF_SHORT_TYPE`）、`src/Type.h:58`（同名覆盖）、`src/TypeRegister.h:62-85`（重命名逻辑）、`src/RTTI.h:205-311`

**(a) 命名空间被剥掉**：`ns1::Thing` 与 `ns2::Thing` 都注册为 `"Thing"`。

**证据（probe24）**：

```
ns1::Thing Type name = 'Thing'
ns2::Thing Type name = 'Thing'
Find('Thing') = ns2 ; ns1 被挤出注册表: YES
```

第二个注册静默覆盖第一个，`Find`/序列化永远只能拿到后者；前者仍可通过 `type_of` 使用，但 `Find(its_name)` 不再命中它。

**(b) 重命名丢旧键**：`TypeRegister<T>::New("new_name")` 会 erase 旧键；同一 T 用两个名字注册时，第一个名字永久消失（probe07：`Find("name_one") == nullptr`）。两个不同 T 同名时后者覆盖前者且无任何诊断（`Find("Dup")` 指向 b，a 被孤立）。

**(c) LP64 冲突**：Linux/glibc x86_64 等 LP64 平台上 `int64_t == long`、`uint64_t == unsigned long`（macOS 的 libc 把 `int64_t` 定义为 `long long`，所以本机未触发；GitHub Actions 的 Ubuntu 会触发）。`InitCoreType` 先 `TypeRegister<int64_t>::New("int64_t")`，后 `TypeRegister<long>::New("long")` 对同一个 `Type` 重命名，结果 `Type::Find("int64_t") == nullptr`。机制已用"同一 T 连续改名"复现。

**影响**：跨平台存档/网络协议按类型名解析会在 Linux 上失效；不同命名空间的同名组件在编辑器里互相覆盖；这是类型系统身份设计问题，不是小 bug。

**建议**：

1. 使用**全限定名**（`NAMEOF_FULL_TYPE` 或编译器 `__PRETTY_FUNCTION__` 保留命名空间）作为默认键；
2. 注册表插入时检测同名冲突：若已有映射且指向不同 `Type*`，直接报错/拒绝，而不是覆盖；
3. 显式注册名作为稳定 ID（与显示名分离），并提供迁移/别名机制；
4. `InitCoreType` 用 `if constexpr`/平台无关的固定名单，避免同一类型重复注册；
5. 增加"两个命名空间同名类""重复名""跨平台内置名"测试。

---

### P1-03 自定义智能指针契约不成立

**位置**：`src/Config.h:78-81`、`src/System.h:36-40`、`src/TypeRegister.h:29-36`

README/AGENTS.md 把 `RTTI_PTR_ALIAS` + `pointer_ops` 作为一等扩展点，但：

**(a) `PtrFromRaw<T>` 对 `T != Object` 不可编译**：默认 `from_raw` 返回 `raw->shared_from_this()`，其类型是 `shared_ptr<Object>`，无法转换为 `shared_ptr<T>`。

**证据（probe26）**：

```
PtrFromRaw(P*) -> error: no viable conversion from 'shared_ptr<rtti::Object>' to 'shared_ptr<P>'
PtrFromRaw(Q*) -> error: ... to 'shared_ptr<Q>'
```

**(b) 构造函数绕过 pointer_ops**：`TypeRegister` 的 `ctor<T>` 使用 `Ptr<T>(new T(...))`（`TypeRegister.h:33`），而 `MakePtr` 使用 `pointer_ops<T>::make`。对自定义 intrusive 指针（尤其不提供裸指针构造、要求 `MakeIntrusive` 的实现），`.constructor<>()` 直接编译失败或绕过分配策略；同时 `new + shared_ptr` 是两次分配，与 `make_shared` 语义不一致。

**影响**：HaloEngine 若使用自定义 `Ptr`（AGENTS.md 提到自定义分配器），`type_of`/`Box`/`MakePtr` 能工作，但 `TypeRegister::constructor` 与 `PtrFromRaw` 不能；用户会误以为整套扩展点可用。

**建议**：`ctor` 改用 `MakePtr<T>`；`PtrFromRaw` 改为 `pointer_ops<T>::from_raw` 返回 `RTTI_PTR_ALIAS(T)`（默认实现用 `std::static_pointer_cast<T>(raw->shared_from_this())`）；补自定义 pointer ops 的编译/运行测试。

---

### P1-04 `cast<>` 契约不一致且组合受限

**位置**：`src/Type.h:672-810`

文档中的转换矩阵（`Type.h:656-670`）与实现不一致，且大量自然写法被 `static_assert` 拒绝：

| 调用 | 结果 | 说明 |
|---|---|---|
| `cast<int>(MakePtr<B>())` | **编译失败** | 对象→值只接受 `From == ObjectPtr`（`Type.h:757`） |
| `cast<int*>(Box(&v))` | **编译失败** | `static_assert` 条件对 `To=int*` 不成立（`Type.h:758`） |
| `cast<int>(3)` / `cast<int*>(3)` | `cast<int*>(3)` 编译成功但返回 `int`（`remove_pointer_t<TTo>`） | 静默类型错误 |
| `cast<void*>(Box(&v))` | 返回 `Object*`（box 的地址），不是 `&v` | P0-03 |
| `cast<const std::string&>(obj)` | 返回 `std::string` 值，引用被丢弃 | 目标类型引用语义丢失 |
| `cast<TestBase*>(Ptr<Test>)` | 编译失败（`static_assert(!is_same_v<TTo*, To>)`） | 原始指针目标不被支持 |

**建议**：把 `cast` 收敛为三个显式函数：`cast_object<To>`（对象↔对象）、`unbox<T>`（ObjectPtr→值，带 `bool`）、`box(value)`；或重写 `cast` 使 `type_t`/`remove_pointer_t` 的组合在所有文档化组合上都有明确且可编译的语义，并为每个组合写编译期测试。

---

### P1-05 方法/属性签名限制过强；`New({})` 歧义

**位置**：`src/MethodInfo.h:29-35`（`GetParameterInfo` 的 static_assert）、`src/TypeRegister.h:147-198`

**(a) 非 const 引用参数无法注册**：

```cpp
static_assert(std::is_reference_v<T> == std::is_const_v<std::remove_reference_t<T>>);
```

要求"引用必须 const，值必须非 const"。任何 `void F(int& out)` 的注册都会编译失败（probe10）：

```
error: static assertion failed due to requirement 'std::is_reference_v<int &> == std::is_const_v<int>'
```

因此**反射无法表达 out 参数**；`MakeArgs` 也会把 `T&` 参数退化为值拷贝（tuple 用 `remove_cr`），即使放开断言，写出也传不回调用方。

**(b) getter/setter 签名强耦合**：`R (U::*)()` 要求 setter 精确为 `void (U::*)(R)`。常见写法 `std::string GetName(); void SetName(const std::string&);` 无法注册（probe23b）：

```
error: no matching member function for call to 'property'
```

同理 `void Set(int)` 与 `int Get() const` 可以，但 `void Set(const int&)` 不行。README 只写 `.property("Value", &Get, &Set)`，没有说明这条约束。

**(c) `TypeRegister<T>::New({})` 歧义**：`New` 同时有 `const std::map&` 与 `const std::string&` 重载，`New({})` 两者都可初始化 → 编译歧义（probe23）。用户必须写 `New()`，容易踩坑。

**建议**：`GetParameterInfo` 允许 `T&`/`const T&`/值；`MakeArgs` 对 `T&` 参数返回引用绑定（或将 out 参数改为返回值）；`property` 增加"getter 返回 R、setter 接受 `const R&`"的组合；`New` 拆成 `New()`/`SetName()`/`SetAttributes()` 避免 braced-init 歧义。

---

### P1-06 转换/比较不继承；对象类型不可比较；对象 comparer 不能编译

**位置**：`src/Type.h:499-517,555-593`；`src/TypeRegister.h:11-24`

- `Type::Convert` 只看 `obj->GetRttiType()`（最派生类型）的 `m_typeConvertors`；派生类型**不会继承**基类注册的转换。probe25：`cast<int>(B)` 成功（7），`cast<int>(D : B)` 失败（`ok=0`）。
- `Type::Compare` 同理，且对对象类型没有**默认** comparer，未注册时 `compare(objA, objB)` 恒为 `Failed`；`Boxed` 有 `GetHashCode` 却没有 `Equals`，哈希/相等契约不闭合。
- `.compare<U>()` 的 `comparer<T, U>` 实现（`TypeRegister.h:23`）写的是 `static_cast<U>(t)`，其中 `t = cast<T>(left)` 对对象 `T` 返回 `Ptr<T>`；因此 `TypeRegister<Obj>::compare<int>()` 直接编译失败（probe07 初次编译即复现）。对象类型只有 `.compare<ObjectPtr>()` 这类"handle 目标"能编译，语义退化为指针身份比较——库文档却把 `compare` 描述为通用能力。

**建议**：沿 `GetBaseType()` 链查找 convertor/comparer；为对象类型提供基于 `GetHashCode`/指针身份或用户回调的默认相等；把 `comparer` 改为 `cast<T>` 后对对象取 `Ptr<T>`、对值取 `T` 的分支实现；补派生类型转换/比较测试。

---

### P1-07 `Compare` 结果随对象地址/注册顺序变化

**位置**：`src/Type.h:555-593`（尤其 `568-571`）

```cpp
if (rightType != nullptr && std::less<Type*>{}(rightType, leftType))
    return Type::Compare(right, left);
```

两侧类型都注册了互相的 comparer 时（例如 `int` 与 `float` 在 `InitCoreType` 中互相 `compare`），最终使用哪一侧的 comparer 由 **Type 对象的堆地址**决定；`comparer<T,U>` 会做有损转换（`static_cast<U>`）。因此同一组值在不同分配顺序/不同进程/不同构建下可能得到不同结果。

**证据（probe06 vs probe06b）**：

```
正常顺序（先注册 int32，再 float）：
  compare(3.9f, 3) = NotEquals        // 使用 comparer<int32,float>
先创建 float Type，再 InitCoreType：
  compare(3.9f, 3) = Equals           // 使用 comparer<float,int32>，3.9f -> 3
```

两种顺序下 `compare(a,b)` 与 `compare(b,a)` 都一致（canonicalization 生效），但**语义**不同；测试 `tests/test.cpp:454` 只检查对称性，因此永远通过。

**建议**：不要用地址做规范顺序；定义稳定的类型优先级（注册序号）或规定"必须存在唯一的方向性转换"；对双向 comparer 的情况显式报错；对数值比较使用不会静默截断的转换（先提升到公共类型），并补传递性/跨类型测试。

---

### P1-08 DynamicType API 不完整且违反 LSP

**位置**：`src/DynamicType.h:143-246`

| 问题 | 证据/后果 |
|---|---|
| `GetConstructor() const override` 隐藏基类的两个 `GetConstructor(initializer_list<...>)` 重载 | `dt->GetConstructor({type_of<int>()})` 直接编译失败（probe14），而非返回 null |
| 只 override `GetMethod(name)`，未 override `GetMethod(name, args)` | `Type* t = dt; t->GetMethod("Add", {int,int})` 返回 `nullptr`（probe25），但 `dt->GetMethod("Add")` 非空；同一对象经不同静态类型得到不同结果 |
| 基类 `m_constructors`/`m_properties` 对 `DynamicType` 为空 | 基类 `CanConvertTo`/`Convert` 的构造路径、`GetConstructor(list)` 完全看不到动态成员 |
| `m_schema` 在 `NewDynamicType` 路径下从未填充 | 死成员；两套构造路径（public ctor vs factory）行为分叉 |
| `CreateInstance` 只匹配参数个数、忽略类型 | `DynamicType.h:239-245`；任意类型实参都会进入同一构造 |
| 用户构造的 `DynamicObject` 不会设置 `m_effectiveType` | 只有默认构造工厂设置；`NewDynamicConstructor` 创建的对象 RTTI 是 `DynamicObject`，`IsAssignableFrom`/属性查找随之失效 |
| `NewDynamicConstructor` 无挂载入口 | `NewDynamicType` 没有 constructors 参数（`DynamicType.h:254-259`），工厂返回的对象只能拿来看 attributes |
| `DynamicMethodInfo` 的 owner 不更新 | 测试传 `nullptr` 作为 owner，`dt->GetMethod("Add")->OwnerType()` 仍为 null |
| `DynamicConstructorInfo`/`DynamicMethodInfo` 的 `m_func` 在初始化列表中被二次 move | `ConstructorInfo(..., [f=std::move(func)]...)` 先执行，随后 `m_func(std::move(func))` 得到空 function（`DynamicType.h:96-99,118-121`）；当前未被使用，但一旦使用即 `bad_function_call` |

**建议**：补齐虚函数/`using Type::GetConstructor; using Type::GetMethod;`；动态构造器/方法统一由 `DynamicType` 拥有并设置 owner；`CreateInstance` 做 schema 类型校验；`m_schema` 删除或改为唯一数据源；`m_func` 删除（基类已存）；让"经 Type* / 经 DynamicType*"的行为完全一致。

---

### P1-09 DynamicObject/DynamicPropertyInfo 类型不安全

**位置**：`src/DynamicType.h:14-81`

- `DynamicObject::m_properties` 是 **public** 的 `map<string, ObjectPtr>`，任何代码可绕过 schema 写入任意类型；
- `DynamicPropertyInfo::GetValue` 用 `m_properties[m_key]`（`DynamicType.h:69`），在 const 读操作中插入 null 条目；
- `SetValue` 不校验 `value` 的 RTTI 是否等于 `propertyType`；
- 属性按名字存储，**跨类型共享同一键空间**；`GetValue` 只 `static_cast<DynamicObject*>`，不检查对象是否属于该动态类型，因此把 A 类型的属性用在 B 类型对象上会静默读写同一 map 键。

**证据（probe11）**：

```
map size before GetValue = 0
GetValue on unset property = 0 (并打印 conversion 错误)
map size after  GetValue = 1   <-- getter 插入 null
wrote a string to an int property, map now holds type 'string'
GetValue<int> now = 0 (with error log)
```

**建议**：`m_properties` 改 private + 访问器；`GetValue` 用 `find` 并返回 schema 默认值（不插入）；`SetValue` 校验类型并返回 `bool`；让 `DynamicPropertyInfo` 校验 `obj->GetRttiType()` 是否为本类型或其子类（至少是 dynamic 类型）；默认实例化时按 schema 初始化默认值。

---

### P1-10 错误模型：失败与 void 成功无法区分，静默失败多

**位置**：`src/PropertyInfo.h:47-62`、`src/Type.h:681-805`、`src/MethodInfo.h:170-179,188-273`、`src/Config.h:103-131`

- `MethodInfo::Invoke` 对 void 方法成功返回 `nullptr`（`MethodInfo.h:250`），对 4 类错误也返回 `nullptr`（`:193,205,215,224`）；调用方无法判断。
- `PropertyInfo::SetValue` 对只读属性**什么都不做**且返回 void（probe04 D）；对类型错误则经 P0-04 写成 0。
- `cast<T>(from)` 的 `pOK` 参数在 `PropertyInfo::GetValue<T>`、`Type::Create<T>` 等封装里被忽略；失败只留下一行默认关闭的日志。
- `RTTI_DEBUG/WARNING/ERROR` 默认展开为空宏（未定义 `RTTI_ENABLE_LOG`），测试是唯一打开它的地方（`tests/test.cpp:3`）；打开后走 `printf`（`Config.h:107`），无法对接 spdlog/引擎日志，也不带级别过滤。

**建议**：引入轻量 `Result<T>`/`std::optional`/`bool` 返回；`Invoke` 返回 `{status, ObjectPtr}`；`SetValue` 返回 `bool`；`cast` 的 `ok` 必须强制使用（`[[nodiscard]]`）；日志默认走可注入的回调（`RTTI_LOG_HANDLER`），而不是 printf。

---

### P1-11 性能：反射热路径的分配与查找成本

**实测（probe18，Release `-O2 -DNDEBUG`，单元测试机器）**：

| 操作 | 成本 |
|---|---|
| `Box(int)` | 1 次堆分配（`make_shared<Boxed<int>>`） |
| `MethodInfo::Invoke(obj, 1, 2)` | **4 次分配/调用**（2 个 Box + vector + 其他） |
| 直接调用 `obj->Add(1,2)` | 0 |
| `GetProperty("Prop199")`（200 个属性） | **约 656 ns/次**（线性字符串比较） |
| `GetMethod("Add")`（1 个方法） | 约 14 ns/次 |
| 重复注册 100 次 | 成员条目 ×100，扫描成本同比放大 |

**热点来源**：

1. `Box`/`Boxed` 每次参数、返回值、属性读取都分配一个 `shared_ptr` 控制块 + `Boxed<T>`；
2. `GetProperty`/`GetMethod` 线性扫描 `std::string` 比较，且沿基类链重复（`Type.h:341-460`）；
3. `GetProperties()`/`GetMethods()` 每次返回新 vector（拷贝基类条目）；
4. `MethodInfo::Invoke` 构造 `std::vector<ObjectPtr>` 与逐参数 tuple 拷贝；`MakeArgs` 又拷贝一次；
5. `std::function` 类型擦除 + `std::apply` 逐参数 `cast`，每次 cast 都可能走 `Type::Convert` 的构造/convertor 扫描；
6. `nameof.hpp` 被 `Object.h → Reflection.h` 全量包含，所有包含 RTTI 的 TU 都付出其解析/模板实例化成本；
7. `ForEach` 接受 `const std::function&`，lambda 可能堆分配。

**建议（按收益排序）**：

1. 名字索引：注册时计算 `hash(name)`，每个 Type 维护 `unordered_map<uint64_t, PropertyInfo*>`；`GetProperty/GetMethod` O(1)，长名不分配（透明 hash/`string_view`）。
2. 参数容器：`Invoke` 用 `std::span<const ObjectPtr>` 或固定容量 `small_vector`，避免 vector 分配；`Box` 对基础类型用 `std::variant`/内联存储（或直接传值给方法）。
3. 去掉 `std::function`：用成员指针 + 静态函数指针 + 标志位的小型 callable，或 `small_function`（≤16/32 字节内联）。
4. 缓存：`PropertyInfo*`/`MethodInfo*` 由调用方缓存（脚本绑定层一次性解析），库提供稳定指针。
5. 去重：注册时按 `(name, signature)` 去重，避免重复注册放大扫描。
6. 把 nameof 从 `Object.h` 的公共包含链移除（只在需要名字的 TU 引入，或提供更轻量的类型名提取）。

---

### P1-12 CI 质量门实际失效

**位置**：`.github/workflows/cmake-multi-platform.yml:64`、`CMakeLists.txt:40`

```yaml
- cmake ... -DRTTI_BUILD_TEST=ON ...     # workflow
option(BUILD_RTTI_TESTS "Build RTTI tests" OFF)   # CMakeLists
```

选项名不一致，`RTTI_BUILD_TEST` 只是未使用变量（现有 `build/CMakeCache.txt` 可见 `RTTI_BUILD_TEST:UNINITIALIZED=ON`）；`enable_testing()`/`add_test` 都在 `if(BUILD_RTTI_TESTS)` 内，因此：

- 库是 INTERFACE，无源文件可编译；
- `ctest` 找不到任何测试，直接以 0 退出 → CI 绿；
- 1 144 行测试从未被 CI 编译过。

同一错误名还出现在 `README.md:362` 和 `.vscode/settings.json`（`cmake.configureArgs`），只有 `AGENTS.md:13` 正确。

**建议**：统一为 `RTTI_BUILD_TESTS`（带项目前缀，避免作为子项目时污染父工程）；CI 增加 `ctest --output-on-failure --no-tests=error`；增加构建成功断言（测试可执行文件存在）；把 P0 探针固化为回归测试。

---

### P1-13 `Type` 可栈构造/拷贝，析构不注销 → 注册表悬垂指针

**位置**：`src/Type.h:50-59`（public 构造函数内注册）、`:62`（public 虚析构）、`:612-616`（裸指针 map）

`Type` 的构造函数直接执行 `GetRegistry()[name] = this;`，但析构函数既不注销也不拥有；构造函数和析构函数都是 public，且隐式拷贝构造会浅拷贝成员指针。于是：

```cpp
{ Type stack("StackType", 4, TypeFlags::None, nullptr, type_of<ObjectBox>(), {}); }
Type* p = Type::Find("StackType");   // 指向已销毁的栈对象
p->GetName();                        // UAF
```

**证据（probe31，ASan）**：

```
Find(StackType) = 0x16b3b2860 (dangling)
AddressSanitizer: stack-use-after-scope ... in main
```

同类问题：`Type copy = *type_of<int>();` 会产生一个不在注册表、却与原件共享 `vector<...*>` 的对象；对任一 `Type` 调用 `delete` 会让注册表悬垂。

**建议**：构造函数/析构函数改为 protected，仅允许 registry 工厂通过 `unique_ptr` 创建；删除拷贝/移动；注册表持有所有权并在销毁时注销；`Type*` 对外只暴露 const 视图（或稳定 ID + 句柄）。

---

### P1-14 对象转换协议依赖未声明的 `ConvertTo<T>` 成员

**位置**：`src/TypeRegister.h:94-138`（`convert<U>` 的对象分支）、`tests/test.cpp:168-177`

`TypeRegister<MyClass>::convert<int>()` 在 `is_object<T>` 分支里直接调用：

```cpp
Ptr<T> tobj = cast<Ptr<T>>(obj);
tobj->template ConvertTo<TU>(uobj);   // T 必须自己声明这个模板成员
```

`ConvertTo` 不在 `Object` 基类、不在任何 concept/接口中，也没有编译期检测；不写的用户会收到一段深藏在 STL 里的模板报错。README 的"自定义类型转换"只写 `.convert<int>()`（README:162），完全没提必须实现 `template <typename U> bool ConvertTo(U&)`。测试靠一个特化（`test.cpp:172-177`）打补丁。此外该分支对 `tobj` 为空没有检查（`cast` 失败返回 null → 空指针调用）。

**建议**：定义正式的转换接口（如 `virtual bool ConvertTo(Type*, ObjectPtr&)` 或注册回调 `TypeRegister<T>::convert<U>([](const T&, U&){...})`），在 `Object` 中提供默认实现或 concept 检查；更新 README；对空 `tobj` 返回 false。

---

### P1-15 `Box(Ptr<T>)` 与 `type_of<Ptr<T>>` 语义矛盾，且类型名碰撞

**位置**：`src/ObjectBox.h:69-75`、`src/Reflection.h:7-28`、`src/System.h:97-104`

- `type_of<Ptr<T>>()` 通过 `inner_type` 解包为 `T` 的 Type（`Reflection.h:13-17`）；
- 但 `Box(MakePtr<T>())` 的 `T` 被推导为 `std::shared_ptr<T>`，`is_object<shared_ptr<T>>` 为 false，于是被当作**值类型**装箱，Type 名为 `"shared_ptr"`（nameof 丢弃模板参数）。

**证据（probe30）**：`Invoke(sink, MakePtr<Target>())` 中方法收到 `shared_ptr` 类型；同时所有 `Box(Ptr<X>)` 对不同的 `X` 都会生成名为 `"shared_ptr"` 的不同 `Type`，在注册表中互相覆盖（P1-02 的又一处触发点）。

**建议**：给 `Box` 增加 `static_assert(!is_object_ptr<T>)` 或对 handle 类型走对象路径；`GetTypeName` 对模板类型保留实参（或使用全限定名）；在 `TypeWarper` 中把 `Ptr<T>` 明确归入引用类型。

---

## 6. P2 详解（分组）

### 6.1 命名与 API 一致性（P2-01/02/03）

见第 10 节完整表。核心问题：

- 访问器三套风格：`GetName()/GetParameters()` vs `OwnerType()/ReturnType()/PropertyType()` vs `IsStatic()/CanRead()`；
- 创建/查找动词五套：`type_of`、`CreateType`、`NewType`、`NewDynamicType`、`CreateInstance`、`Create`；
- 成员前缀两套：`Type`/`MethodInfo`/`DynamicType` 用 `m_`，`MethodBase`/`PropertyInfo` 不用（`name/owner/rettype/arguments`）；
- 拼写：`warper`（wrapper）、`TypeConvertor`/`Convertor`（Converter）、`GetEnumInfos`、`remove_cr`；
- 语义：`Compare` 不排序却叫 Compare、返回三态；`GetUnderlyingType` 同时表示"指针 pointee"和"枚举底层"；`IsValueType` 定义为"基类 == ObjectBox"；`IsSubClassOf` 不自反；`ParameterInfo::IsConst` 被 static_assert 强制等于 `IsRef`（冗余）。
- 宏：`HASH`、`TYPE_DECLARE`、`RETURN`（局部）无 `RTTI_` 前缀，极易与引擎宏冲突；`System.h:2-6` 重定义保留标识符 `__PRETTY_FUNCTION__`/`__debugbreak`（UB，且后者未使用）。

### 6.2 属性系统（P2-06）

`Attributable` 只存 `std::map<size_t, std::any>`：

- 只有哈希键，没有名字，无法枚举/调试/检测哈希碰撞（两个不同属性名碰撞会静默合并）；
- `GetAttribute` 按值返回 `std::any`（拷贝），`GetAttribute<T>` 再拷贝一次值；
- `TypeRegister<T>::New(attributes)` 直接 `m_attributes = attributes` 覆盖旧值（不是合并）；
- 无运行时添加/删除属性的 API；
- `GetAttribute<std::nullptr_t>` 被显式 delete（`Attributable.h:53-54`），API 奇怪且未解释。

**建议**：属性表改成 `map<size_t, Attribute{name, any}>` 或在 Debug 保存 name；提供 `TryGetAttribute<T>(id, T&)`；`New` 改为合并；公开 `AddAttribute`。

### 6.3 枚举 API（P2-07）

- `GetEnumValue(name)` 线性扫描 `map<string, ObjectPtr>`（`Type.h:134-144`），应改为 `m_enumValues.find(name)`；
- `GetEnumName` 用 `Compare` 线性扫描，O(n) × 转换成本；别名（多个名字同值）时返回**字母序第一个**，与注册顺序无关；
- `CanConvertTo` 声称 int→enum 可转，`Convert` 实际崩溃（P0-01）；
- 枚举→整型只注册到 `underlying_type` 的 convertor 是"单跳"，跨宽度依赖数值 convertor，缺少系统性测试。

### 6.4 尺寸与布局语义（P2-08）

- `Type::m_size` 是 `uint32_t`（`Type.h:53`），`GetSize()` 返回 `size_t`；大于 4 GB 的类型会截断（虽然罕见，但类型不一致是缺陷）；
- `DynamicType` 传入 size 0（`DynamicType.h:161`），`GetSize()` 对动态类型无意义；
- `Boxed<T>` 的 Type 的 size 是 `sizeof(T)`，而实际对象是 `sizeof(Boxed<T>)`；`GetSize` 到底指 payload 还是 box 没有文档。序列化/序列化器若按它 memcpy 会错。

### 6.5 查找与覆盖（P2-09）

- `GetMethods()`/`GetProperties()` 直接拼接基类条目，不做去重；同名 override 会出现两份（probe22：`F` 出现两次，owner 分别是 D 和 B）；
- `GetMethod(name)` 返回最派生的第一个匹配（正确），但无"仅声明于本类型"的 API；
- 无参数名、无 default 值、无 `noexcept`/ref-qualifier/可变参数支持；
- `GetMethod(name, {ParameterInfo})` 与 `GetMethod(name, {Type*})` 两个重载容易混淆，后者不比较引用/const。

### 6.6 `select_overload` 不可用（P2-10）

`TypeRegister.h:302-312` 的两个重载参数类型分别是 `Signature*` 与 `Signature(ClassType::*)`。显式指定签名（唯一用途）时：

```cpp
select_overload<int (C::*)(int)>(&C::f)   // 全部 no matching function
```

probe29 对普通成员重载、const 成员均失败。该 helper 目前是死代码；建议改为 `template <typename Sig> Sig select_overload(Sig f)`（配合显式签名），并加编译测试。

### 6.7 头文件卫生与依赖（P2-11/12）

- `MethodInfo.h` 使用 `std::tuple`、`std::apply`、`std::vector`、`std::function`，但只 include `System.h/Object.h/ObjectBox.h/Attributable.h`；目前靠 libc++ 的传递包含，换标准库/版本可能断。
- `Config.h` 的 `RTTI_LOG` 使用 `printf`，但只 include `<memory>`；打开日志且触发实例化时依赖传递包含 `<cstdio>`（本机 libc++ 恰好通过）。
- `nameof.hpp`（Neargye 0.10.3，MIT，1 279 行）直接 vendored 在 `src/`，没有版本说明、升级脚本或第三方目录；它被 `Reflection.h` 包含，而 `Object.h` 又包含 `Reflection.h`，导致**任何包含 RTTI 的 TU 都编译整份 nameof**。

### 6.8 类型封装与静态生命周期（P2-13/14）

- `Type` 构造函数 public（`Type.h:50`）、析构 public、隐式拷贝构造（浅拷贝 `vector<...*>`）；拷贝出的 `Type` 不在注册表、与原件共享成员指针，且可被 `delete` 使注册表悬垂；
- `GetRegistry()` 返回可变的 `std::map&`，任何外部代码可 `clear()`/`erase()`；
- 注册表是函数内 static，类型对象是裸 `new`（不析构），但若用户全局对象析构时调用 `Find`，注册表可能已先析构；
- 作为共享库（DSO）使用时，`inline` 函数的 static 是否唯一取决于符号可见性；插件各自一份注册表会导致类型身份不一致。

**建议**：`Type` 构造函数私有 + factory friend；删除拷贝；析构 protected；`GetRegistry` 返回 const 视图或只暴露 `Find/ForEach`；注册表改为显式 `Registry` 单例（可 `Reset`/`Freeze`），并在文档中声明 DSO 约束。

### 6.9 日志与哈希（P2-15/16）

- 日志默认关闭、printf、无级别过滤；`RTTI_ERROR` 的参数在关闭时不会被求值（好），但打开后字符串拼接会分配。
- `Boxed<T>::GetHashCode` 用 `std::hash<T>`（若可默认构造），而 `Compare` 用 `comparer<T,U>`；在 libc++ 实现下 `float` 的 `0.0f` 与 `-0.0f` 位模式不同、哈希不同但 `==` 相等；跨类型相等（int/double）哈希必然不同。若用于哈希容器会违反 equals/hash 契约。

### 6.10 异常安全（P2-18）

`TypeRegister` 的多次 `push_back`/`new` 没有事务性：`new ConstructorInfo` 成功、`push_back` 抛 `bad_alloc` 时对象泄漏；`Type` 构造在 `GetRegistry()[name]=this` 时若 map 分配抛异常，`new Type` 的对象泄漏（未被任何 owner 持有）。虽然内存耗尽场景罕见，但"部分注册"会让类型处于半初始化状态（有属性没方法），且无回滚。

### 6.11 构建与打包（P2-19/20）

- `CMakeLists.txt:10-12` 在库目录设置全局 `CMAKE_CXX_STANDARD/EXTENSIONS`（作为 FetchContent 子项目时影响该目录下所有目标；应只 `target_compile_features`）。
- `file(GLOB_RECURSE)` 收集头文件：新增头文件必须重新 configure。
- 选项名 `BUILD_RTTI_TESTS` 无项目前缀，易与父工程冲突（见 P1-12）。
- `INSTALL_INTERFACE:include/RTTI`（`:30`）是硬编码相对路径，而安装头文件到 `${CMAKE_INSTALL_INCLUDEDIR}/RTTI`（`:72`）；若用户覆盖 `CMAKE_INSTALL_INCLUDEDIR`（如 `include/halo/third_party`），导入目标的 include 路径就错了。
- `write_basic_package_version_file` 未加 `ARCH_INDEPENDENT`；header-only 包的版本文件按指针宽度校验，交叉编译/多架构消费会误判不兼容。
- `project(RTTI VERSION 1.0.0)` 与仓库最新 tag `0.9.5` 不一致。

### 6.12 文档、合规与测试工程（P2-21/22/23/24/25/27）

- README 的快速开始代码**不可编译**（见附录 C）：`rtti::create<>` 不存在、`prop->Get/Set` 不存在、动态类型 schema 用了错误的 `map<string, Type*>`、include 写 `<RTTI/RTTI.h>` 但安装目标是 `include/RTTI` 指向内部（实测 `<RTTI/RTTI.h>` 在 find_package 消费时找不到）；构建命令选项/二进制名也错。
- 仓库声称 MIT 但**没有 LICENSE 文件**；vendored `nameof.hpp` 自带 MIT 头（合规尚可，但项目整体许可证缺失）。
- 测试：全局计数器、每个用例重复 `RegisterAllTypes()`（正好触发 P1-01）、断言大量使用 `>=`（掩盖重复注册）、无隔离/死亡测试/sanitizer/Release 运行。
- CI：`windows-latest + gcc` 组合在 include 表中没有对应 `cpp_compiler`（会传空 `-DCMAKE_CXX_COMPILER=`）；无 macOS job（而这是项目主力平台）；无 ASan/UBSan/TSan；无 `cmake --install` + `find_package` 消费测试；`actions/checkout@v3` 已过时。
- `.vscode/settings.json` 使用错误的 `RTTI_BUILD_TEST`。

### 6.13 其余 P2（P2-17、P2-26）

**P2-17 `ForEach` 的接口形态**：参数是 `const std::function<void(Type*)>&`，lambda 捕获较大时可能堆分配；回调拿到可变的 `Type*`（可修改注册表/类型状态）；没有 const 迭代、没有快照参数。建议改为模板 `template <typename F> void ForEach(F&&)`，并提供 `ForEachSnapshot`。

**P2-26 值/引用语义与数值转换图**：

- `IsAssignableTo` 对值类型返回 `type == Object || type == ObjectBox`（`Type.h:168-180`），即"值类型可赋值给 Object"，但值类型本身并非 Object，只是 box 是；这让 `IsAssignableFrom` 的答案混淆了 payload 与 box。
- 数值转换图是手工枚举且不对称：`bool → int` 有（`RTTI.h:19-30`），但 `int → bool` 没有（probe25 实测 `cast<bool>(Box(1))` 失败）；`char/wchar_t/char16_t/char32_t/long double` 未注册，`cast<char>(Box(65))` 失败（probe25）。`TypeFlags::Integral`/`Floating` 只描述类型类别，不代表可转换。
- `Convert` 只支持单跳（注释已说明），但 `CanConvertTo` 对构造器路径的检查会让部分两跳场景"看起来可以"，行为不一致。
- 建议：把内置数值转换改为"类别 + 宽度/符号"驱动的泛化实现（或至少补齐常见类型），并提供 `CanConvert`/`Convert` 的一致性测试矩阵。

---

## 7. P3（清单）

- `tests/test.cpp:930,1018` 两个 "22." 章节号。
- `src/Type.h:809-810` 两个 `} // namespace rtti` 注释，其中第一个实际闭合的是 `cast` 函数，注释误导。
- `src/TypeRegister.h:9` 未使用的 `const std::string empty_string;`（每个 TU 一次动态初始化）。
- 匿名命名空间内的函数再写 `static`（`MethodInfo.h:10,21,30`）冗余。
- `Type.h:16-18,32-34` 注释掉的 `SourceType` 成员。
- `MethodFlags` 是 `enum class` 却用 `(int32_t)` 位测试（`MethodInfo.h:165-168`），且只有两个值。
- `tests` 汇总打印 "209 / 209" 是断言数，不是用例数（`test.cpp:1137-1138`）。
- `force_consteval` 名称不表意；`hash_string` 用 `size == 0` 当"整串"哨兵。
- `Type.h:623` `m_underlyingType = nullptr` 与构造函数初始化列表重复。
- `.clang-format` 中 `BreakBeforeBraces` 出现两次。
- 若干 lambda / 初始化列表未遵循仓库自己的 Allman 风格（如 `TypeRegister.h:97-136`）。
- `System.h:2-6` 定义后从未使用的 `__debugbreak`。

---

## 8. 架构设计评估

### 8.1 结构概览

```
Object (引用类型根, enable_shared_from_this)
  └── ObjectBox (可装箱值类型的抽象基类)
        └── Boxed<T> (持有 T 的实例)
Type (元数据中心, 全局注册表 <name, Type*>)
  ├── m_constructors : vector<ConstructorInfo*>   (裸指针, 拥有)
  ├── m_methods      : vector<MethodInfo*>        (裸指针, 拥有)
  ├── m_properties   : vector<PropertyInfo*>      (裸指针, 拥有)
  ├── m_typeConvertors : vector<TypeConvertor>    (函数指针)
  ├── m_objectComparers: vector<ObjectComparer>   (函数指针)
  ├── m_baseType     : Type*                      (裸指针, 不拥有)
  └── m_underlyingType: Type*                     (裸指针, 不拥有)

MethodBase (Attributable) ── ConstructorInfo / MethodInfo (std::function 擦除)
PropertyInfo (Attributable) ── getter/setter MethodInfo*
DynamicType : Type; DynamicObject : Object; Dynamic*Info : 上述类的子类
```

设计意图清晰：用 C# 式 `Type` 对象把"编译期类型信息 + 运行时调用"集中起来，值类型通过 `Boxed<T>` 统一进入 `ObjectPtr` 世界，`TYPE_DECLARE` + `TypeRegister` 提供低样板注册。这是合理的轻量反射方案，尤其适合"注册在启动期、查询/调用在运行时"的编辑器/脚本场景。

### 8.2 值得肯定的部分

1. **零宏侵入的可选扩展点**：`RTTI_PTR_ALIAS`/`RTTI_OBJECT_BASE`/`pointer_ops` 的思路正确（实现有 bug，见 P1-03）。
2. **`MethodInfo::Register` 的 callable 限制**：用 `is_allowed_method_callable_v` 在编译期拒绝带捕获 lambda，方向正确，能防住一类悬垂。
3. **`cast` 的编译期分支**用 `if constexpr` + `static_assert` 把大量错误前移到编译期，意图好（但组合覆盖不全）。
4. **`HASH` 强制常量求值**、属性用哈希键避免运行时字符串分配，思路正确。
5. **不依赖 `typeid`/`dynamic_cast`**（`-fno-rtti` 实测可编译测试），便于嵌入关闭 RTTI 的引擎。
6. **测试对 happy path 的覆盖面不错**：值/引用类型、继承、枚举、属性、方法、动态类型、属性 attribute 都有用例。

### 8.3 结构性风险（按严重度）

1. **动态类型跨 C++ 对象模型继承**（P0-02）：`Type` 的继承链是"元数据继承"，`DynamicObject` 的 C++ 继承链是"Object"；两者不一致时，`IsAssignableFrom` 的答案与 `static_cast` 的安全性脱钩。这是必须从设计上隔离的边界：要么动态类型只能继承动态类型，要么为跨域继承实现真正的委托/包装。
2. **类型身份 = 短名字符串**（P1-02）：注册表以 `std::map<std::string, Type*>` 为唯一索引，名字可变、可覆盖、去命名空间、平台相关。类型身份应是一个不可变的稳定 ID（全限定名或 GUID/哈希 + 冲突检测），显示名与之分离。
3. **注册表无所有权/无生命周期**（P1-01）：裸 `Type*` + 永久泄漏 + 无注销，使"热重载/插件"这类引擎核心能力不可用；`MethodInfo` 的函数指针在模块卸载后悬垂。
4. **全局可变状态 + 懒初始化**（P0-07）：`type_of` 在任何线程首次调用都会写注册表；这对多线程引擎是最危险的模式。应改为"启动期一次性构建 + 运行时只读快照"。
5. **安全依赖 assert**（P0-08）：类型擦除 API 的边界检查在 Release 消失；反射输入通常来自脚本/文件（不可信），必须默认校验。
6. **错误模型缺失**（P1-10）：`nullptr` 同时表示"void 成功""失败""空值"，调用方无法做正确决策；这是 P0-04 能长期存在的土壤。
7. **DynamicType 与 Type 的虚函数契约不完整**（P1-08）：部分 override + 非虚重载隐藏，导致 LSP 破坏；持有 `Type*` 与 `DynamicType*` 行为不同。
8. **两套类型世界（编译期/动态）只做了表面整合**：基类容器为空、`m_schema` 死成员、构造器无法挂载，说明 DynamicType 是"后加的旁路"，不是一等公民。
9. **类型对象的生命周期与注册表脱钩**（P1-13）：`Type` 可被栈上构造/拷贝/删除，注册表只存裸指针；`Find` 可以返回悬垂指针（ASan 已复现）。
10. **handle 与 value 的边界不清晰**（P0-09/P1-15）：`Box(Ptr<T>)` 把共享指针当值装箱，`type_of<Ptr<T>>` 却解包为 `T`；可变参数 `Invoke` 因此静默传错对象。

### 8.4 建议的目标架构（若继续演进）

- **Registry**：`TypeRegistry` 单例，启动期 `Register` + `Freeze`；内部 `unordered_map<string_view, unique_ptr<Type>>` + 冲突检测；提供只读快照供并发读；`type_of<T>` 用 `std::once_flag` 保证同 T 唯一。
- **Identity**：类型全限定名 + 稳定 hash；注册时可显式指定稳定 ID；重命名只改显示名。
- **Box**：`Boxed<T>` 之外提供 `InlineValue`（variant/小对象）避免每次分配；`TryUnbox` 全量校验。
- **Invoke**：`CallResult { Status, ObjectPtr }`；参数用 `span<const ObjectPtr>`；失败绝不调用目标。
- **Dynamic**：`DynamicType` 仅允许动态基类；若需扩展编译期类型，用 `DynamicObject` 持有 `Ptr<Base>` 的适配器；动态属性在对象构造时按 schema 初始化并做类型校验。
- **Concurrency**：注册期单线程 + `Freeze` 后只读；动态类型创建加锁并明确生命周期。
- **Naming/API**：统一 `Get*` 访问器；`New` 只用于工厂；宏统一 `RTTI_` 前缀；去掉保留标识符重定义。

---

## 9. 性能评估（详细）

### 9.1 测量方法与数据

环境：Apple clang 21，`-O2 -DNDEBUG`，探针内联在库头文件中；分配计数通过替换全局 `operator new/delete` 统计活跃分配（probe18）。

| 场景 | 结果 |
|---|---|
| `Box(int)` | 1.00 alloc/box |
| `Invoke(obj, 1, 2)`（2 参数、返回 int） | 4.00 allocs/call |
| 直接 `obj->Add(1,2)` | 0.00 allocs/call |
| `GetProperty("Prop199")`，200 个属性 | ≈656 ns/lookup |
| `GetMethod("Add")`，1 个方法 | ≈14 ns/lookup |
| 100 次重复注册 | +594 活跃分配、+66 408 B、成员条目 ×100 |

> 这些数字用于相对比较，不构成基准结论；但数量级足以说明热路径不适合逐帧脚本调用。

### 9.2 逐项分析

1. **装箱分配**：`MakePtr<Boxed<T>>` = `make_shared` 一次分配（控制块 + 对象），每个参数/返回值/属性读一次；`MethodInfo::Invoke` 的 4 次分配中 2 次来自 Box，1 次来自 `std::vector<ObjectPtr>`，1 次来自 function/其他。脚本每帧数百次调用即数百次分配。
2. **查找**：`GetProperty` 线性扫描 + `std::string::operator==`；200 个属性时最坏 200 次比较。`GetMethod` 同样线性，且沿基类链可能重复扫描。`Type::Find` 是 `map<string>` O(log n)，但每次调用 `const char*` 会构造临时 `std::string`（长名分配）。
3. **重复注册放大**：注册不幂等时，扫描列表无界增长；`CreateInstance`/`Convert`/`CanConvertTo` 都是线性遍历构造器/convertor，重复注册会成倍增加每次调用的比较次数。
4. **`std::function` 擦除**：每次调用一次间接跳转 + 可能的堆分配（捕获较大时）；`std::apply` + tuple 拷贝增加一次参数复制。
5. **nameof 编译期成本**：`Object.h` → `Reflection.h` → `nameof.hpp` 的全量包含使每个使用 RTTI 的 TU 都实例化 nameof 的模板；1 279 行 + 大量 `__PRETTY_FUNCTION__` 处理，显著拖慢编译。
6. **属性读取的返回装箱**：`PropertyInfo::GetValue` 返回 `ObjectPtr`，即使调用方只想要 `int`，也要经过 Box→cast→Unbox 的分配与转换。
7. **`GetProperties/GetMethods` 每次新建 vector**：序列化/编辑器每帧枚举会持续分配。

### 9.3 优化优先级

| 优先级 | 优化 | 预期 |
|---|---|---|
| 高 | 名字 hash 索引（每 Type 一个 unordered_map） | 属性/方法查找从 O(n) 到 O(1)，656 ns → 几十 ns |
| 高 | 注册幂等 + 去重 | 消除重复条目带来的线性放大 |
| 高 | 参数容器 `span` + 避免每次 vector | 每次 Invoke 少 1 次分配 |
| 中 | 基础类型内联装箱（variant 或 tagged union） | 常见脚本调用 0 分配 |
| 中 | 去掉 `std::function`（小函数对象/成员指针联合） | 少一次间接 + 可能分配 |
| 中 | 缓存 PropertyInfo*/MethodInfo* 于绑定层 | 运行时零查找 |
| 低 | nameof 延迟/隔离 | 编译时间下降 |
| 低 | `GetProperties` 返回 span/缓存 | 枚举场景少分配 |

---

## 10. 命名与 API 一致性专项

### 10.1 命名问题表

| 现状 | 问题 | 建议 |
|---|---|---|
| `TypeWarper`, `warper`, `warper_t` | 拼写错误（wrapper）；概念不清晰 | `TypeWrapper`, `wrapper` |
| `TypeConvertor`, `Convertor`, `m_typeConvertors` | Converter 拼写错误 | `TypeConverter`, `Converter` |
| `GetEnumInfos` / `m_enumValues` | "Infos" 语义错；字段与访问器不一致 | `GetEnumValues` |
| `IsSubClassOf` | C# 风格；且对自身返回 false（不自反） | `IsSubclassOf`/`IsDerivedFrom`，明确自反性 |
| `OwnerType()`, `ReturnType()`, `PropertyType()` vs `GetName()`, `GetParameters()` | 访问器前缀不统一 | 统一 `Get*` 或统一无前缀 |
| `GetParameters()` 非 const | const 对象无法调用；返回 const 引用却非 const 方法 | 加 const |
| `type_of`, `cast`, `convert`, `compare`, `is_comparable`, `hash`, `hash_string` | 与 PascalCase API 混用 snake_case | 统一一种（建议核心模板保持 snake，但与 C# 风格访问器分开文档化） |
| `NewType` / `CreateType` / `NewDynamicType` / `CreateInstance` / `Create` | 四套创建动词 | `New` 仅工厂、`Create` 仅实例 |
| `remove_cr` | 含义不明（remove const/reference） | `remove_cvref` |
| `HasFlag` | C# 风格；`TypeFlags` 无位运算支持 | `HasFlag` 可保留，补按位或运算符 |
| `ParameterInfo::IsConst` + static_assert | 与 `IsRef` 冗余（强制同真值） | 去掉一个，或允许四种组合 |
| `MethodFlags` + `IsStatic` | 只为两个状态引入 flags | `bool IsStatic` |
| `ObjectBox` / `Boxed` / `vartype` / `warper` | 四层概念命名重叠 | 文档化一个概念模型，重命名 |
| `DynamicPropertyDesc` vs `PropertyInfo` | desc/info 两套元数据概念 | 统一命名 |
| `HASH` / `TYPE_DECLARE` / `RETURN` | 无前缀宏，易碰撞；`RETURN` 局部未污染全局（已 undef） | `RTTI_HASH` / `RTTI_TYPE_DECLARE` |
| `__PRETTY_FUNCTION__` / `__debugbreak` 重定义 | 保留标识符，UB | 用 `RTTI_PRETTY_FUNCTION` 宏包装 |
| 成员：`Type::m_name` vs `MethodBase::name`/`PropertyInfo::name` | `m_` 前缀不统一 | 统一 `m_` |
| 文件：`System.h`, `Reflection.h`, `ObjectBox.h`, `Type.h` | 通用名，作为 `-I src` 暴露易冲突；`Reflection.h` 实际是类型分派 | 建 `src/RTTI/` 子目录，重命名 `TypeTraits.h`/`Dispatch.h` |

### 10.2 语义命名问题（重点）

- **`Compare`**：名字暗示排序（`<`/`>`），实际返回 `Equals/NotEquals/Failed`；且选择 comparer 的规则不可见（地址序）。建议改名 `Equals`/`TryCompare`，或返回 `int` 做真正排序。
- **`IsValueType`**：实现是"基类 == ObjectBox"，而非"不是 Object 派生"；`int` 是值类型，`Boxed<int>` 也是值类型——语义正确但实现耦合到基类。
- **`GetUnderlyingType`**：枚举底层与指针 pointee 共用，调用方需先判断 `IsEnum/IsPointer`；建议拆成 `GetEnumUnderlyingType`/`GetPointeeType`。
- **`IsAssignableTo(Object)` 对值类型返回 true**：值类型本身不是 Object，但 box 是；这使 `IsAssignableFrom` 的答案混淆了"payload"与"box"。
- **`OwnerType`**：对 `NewDynamicMethod(nullptr, ...)` 返回 null；语义是"注册时传入的 owner"而非"实际所属"，容易误导。
- **`GetSize`**：payload 大小 vs box 大小未定义（见 P2-08）。
- **`New` 返回对象而不是引用/单例**：`TypeRegister<T>::New()` 返回临时值，链式方法返回引用到临时，合法但反直觉；`auto& reg = ...New()` 不可编译。
- **`GetRegistry`**：名字像只读，实际返回可变引用。

### 10.3 宏与全局污染

- `HASH`、`TYPE_DECLARE` 是极短大写名；HaloEngine 或其他依赖若定义同名宏会直接冲突（宏无作用域）。
- `System.h:2-6` 对 `__PRETTY_FUNCTION__`（GCC/Clang 预定义）与 `__debugbreak`（保留名）做 `#define`，属于 UB；在包含顺序不同/编译器不同时可能出现难以排查的替换。
- `RTTI_LOG` 直接 `printf`，与引擎"禁止 printf/统一日志"的规范冲突（AGENTS.md）。
- `RTTI_*` 宏本身有前缀，正确；问题是 `HASH`/`TYPE_DECLARE`。

---

## 11. 健壮性与边界条件

### 11.1 空值清单

| 入口 | 当前行为 | 期望 |
|---|---|---|
| `Type::CanConvertTo(nullptr)` | SEGV（P0-05，probe08） | 返回 false + 报错 |
| `Type::Convert(obj, nullptr, ...)` | 可能 SEGV（P0-01 路径） | 返回 false |
| `Type::CanConvertTo<T>()` 目标类型未注册 | `type_of<T>` 懒创建，通常非空 | 可接受 |
| `PropertyInfo::GetValue(nullptr)` | MethodInfo 报错 → `cast<T>(nullptr)` 返回 0/空 | 返回失败状态 |
| `PropertyInfo::SetValue(nullptr, ...)` | 同上，静默 | 返回 false |
| `Unbox<T>(nullptr)` | `assert`（Release 直接解引用） | `TryUnbox` 返回 false |
| `PtrFromRaw(nullptr)` | `raw->shared_from_this()` → UB | 返回空指针 |
| `NewType(name, ..., base=nullptr)` | 允许，产生无基类型对象 | 允许仅 Object，其他校验 |
| `NewDynamicType` 同名 | 静默覆盖旧 Type（DynamicType.h:268） | 拒绝 + 报错 |
| `TypeRegister::New` 空名字 | 允许注册 `""` | 拒绝 |

### 11.2 `assert` 清单（全部在 Release 消失）

| 位置 | 内容 | 风险 |
|---|---|---|
| `ObjectBox.h:84` | `ptr != nullptr` | 空解引用 |
| `ObjectBox.h:100` | `ptr->GetRttiType() == type_of<T>()` | 类型混淆读 |
| `ObjectBox.h:110` | 非指针 box 与 `T` 的匹配 | 类型混淆读 |
| `ObjectBox.h:123` | 引用解箱类型匹配 | 类型混淆读 |
| `DynamicType.h:67,74` | `obj != nullptr` | 空解引用 |
| `ObjectBox.h:83` | 注释掉的 `IsBoxedType()` | 本应是最重要的校验 |

此外，`assert` 表达式内的 `type_of<T>()` 有副作用（P0-08），导致 Debug/Release 注册集合不同。

### 11.3 分支/边界条件表

| 场景 | 当前行为 | 问题 |
|---|---|---|
| 方法参数多于声明 | `args.size() < sizeof...(Args)` 允许；多余参数被忽略（probe04 B） | 应为 `!=`；掩盖调用方 bug |
| 构造参数个数不符 | `!=` 拒绝（正确） | — |
| 参数转换失败 | 以默认值调用（P0-04） | 静默错误 |
| 整数宽度 ≠ 枚举底层 | 崩溃（P0-01） | 崩溃 |
| 枚举 → 更宽整型 | 经底层 + convertor，可用 | 依赖注册表，缺测试 |
| `int → bool` | 不可转换（probe25） | 转换图不对称 |
| `char/wchar_t/long double` | 未注册，无 convertor（probe25 `char` 失败） | 内置类型覆盖不全 |
| 派生类使用基类 convertor | 失败（probe25 D:B） | 不符合直觉 |
| `Type` 名称重复 | 静默覆盖/丢键（P1-02） | 数据错乱 |
| `Type` 栈对象离开作用域 | 注册表保留悬垂指针（P1-13，probe31） | UAF |
| 可变参数 `Invoke(obj, Ptr)` | 对象被装箱成 `shared_ptr` 值（P0-09） | 静默传错对象 |
| `Box(Ptr<T>)` | 产生名为 `"shared_ptr"` 的值类型（P1-15） | 类型名碰撞/语义矛盾 |
| 动态属性未设置 | 读取插入 null、返回 0 + 错误日志（probe11） | getter 有副作用 |
| 动态属性写错类型 | 接受（probe11） | 类型混乱 |
| 动态类型 `GetSize` | 0 | 无意义 |
| 动态类型 `GetConstructor(list)` | 编译失败（name hiding） | API 不完整 |
| `DynamicType::CreateInstance` | 只按参数个数匹配 | 类型错误也接受 |
| `ForEach` 中插入 | 新类型被遍历（P0-06） | 可能死循环 |
| 重复注册 | 线性累积（P1-01） | 泄漏/膨胀 |
| `Type` size > 4 GB | uint32 截断（P2-08） | 理论上错误 |
| 线程首次 `type_of` | 数据竞争（P0-07） | 崩溃风险 |
| `InitCoreType` 多次 | 重复注册（probe07） | 膨胀 |
| `RTTI_LOG` 未定义 `RTTI_ENABLE_LOG` | 空宏，失败不可见 | 静默错误 |
| 日志打开且无 `<cstdio>` | 依赖传递包含 | 可移植性 |

### 11.4 错误传播模型

当前有三种并行风格：

1. 返回 `nullptr`（`Find`、`GetProperty`、`Invoke`、`CreateInstance`）；
2. 返回 `bool` + 输出参数（`Convert`、`cast` 的 `pOK`）；
3. 日志 + 返回默认值（`cast` 失败、`GetValue<T>`）。

它们互不组合：`Invoke` 的 nullptr 无法区分 void 成功/失败；`GetValue<T>` 丢弃 `ok`；`SetValue` 完全不报告。建议统一为 `Result<T>`（或 `std::optional` + `Status`），并在 API 文档中明确"失败不修改输出参数/不调用目标"。

### 11.5 线程与生命周期

- 注册表：无锁（P0-07）。
- `Type`/`MethodInfo`/`PropertyInfo`：创建后只读（除 `TypeRegister::New` 改名/改属性），但没有任何"冻结"标记；运行期注册会与遍历/查找竞争。
- 对象：`ObjectPtr` 共享所有权；`Boxed<T>` 返回的引用/指针（`Unbox<T&>`、`Unbox<T*>`、`GetPointer`）生命周期绑定到 `ObjectPtr`，API 未文档化该约束（`ObjectBox.h:53-66`）。
- 动态对象的 `m_effectiveType` 是裸 `Type*`，若未来支持删除动态类型会悬垂。
- 全局注册表析构顺序、DSO 重复实例（P2-14）。

### 11.6 异常安全

- `Type`/`MethodInfo` 构造期间 `new` 抛 `bad_alloc` 会泄漏已构造部分（P2-18）。
- `std::function` 拷贝可能抛；`Invoke` 无 `noexcept`，异常会穿透反射边界到脚本宿主。
- `Box`/`Unbox` 对 `T` 的拷贝构造可能抛，未做保证。
- 建议：注册 API 提供 `TryRegister` 返回状态；关键路径 `noexcept` 化需谨慎（分配不可避免），至少在文档中说明。

---

## 12. 测试评估

### 12.1 现状

- 单文件 `tests/test.cpp`（1 144 行），自研 70 行断言宏（`EXPECT/EXPECT_EQ/EXPECT_NE/EXPECT_STREQ`），27 个 `run_test`，209 条断言；Debug 全绿。
- 覆盖：核心类型查询、枚举、对象注册、装箱、cast、compare、attributes、方法调用、属性访问、Handle 模式、错误路径（少量）、void*/string、继承、Create/CanConvertTo、DynamicType（基础/实例/属性/方法/attributes/边界/子类）。
- 每个用例都调用 `RegisterAllTypes()`（`test.cpp:213-224`），部分再调用 `InitCoreType()`；这正是重复注册的温床。

### 12.2 覆盖矩阵（✅ 有，⚠️ 弱，❌ 无）

| 能力/场景 | 状态 | 说明 |
|---|---|---|
| 类型查询/Find/ForEach | ✅ | 未测 Find 冲突/重名 |
| 值/引用类型判定 | ✅ | — |
| 枚举值/名称/属性 | ✅ | 未测别名、未知值 |
| 装箱/解箱（值） | ✅ | 未测错误类型解箱 |
| 装箱/解箱（原始指针/`void*`） | ⚠️ | 只测 `IsPointer()`（`test.cpp:373`），未测 `GetPointer/Unbox<void*>` |
| cast 对象↔对象 | ✅ | 未测失败返回、原始指针不相关类型 |
| cast 对象→值 | ⚠️ | 仅经 `Type::Convert`，未测 `cast<int>(Ptr<T>)`（编译失败） |
| cast 值→值 | ✅ | 未测 `int*`/引用目标 |
| 转换失败（`ok=false`） | ❌ | 全部断言 `EXPECT_TRUE(ok)` |
| 方法调用成功 | ✅ | — |
| 可变参数 `Invoke` 传对象 | ❌ | 与 vector 重载结果不同（P0-09，probe30） |
| `Box(Ptr<T>)` | ❌ | 装箱成值类型、类型名 "shared_ptr"（P1-15） |
| `Type` 生命周期（栈/拷贝/delete） | ❌ | 注册表悬垂（P1-13，probe31） |
| 对象 `.convert<U>()` 的 `ConvertTo` 契约 | ❌ | 未定义时编译失败（P1-14） |
| 方法调用参数错误 | ❌ | 未测（会触发 P0-04） |
| 方法参数个数过多 | ❌ | 未测（会被静默接受） |
| 非 const 引用参数/out 参数 | ❌ | 无法注册（P1-05） |
| 属性 get/set | ✅ | 未测只读 SetValue、类型错误 SetValue |
| 只读属性 | ⚠️ | 只测 `CanWrite()==false`，未测 `SetValue` 行为 |
| 继承属性/方法 | ✅ | 未测 override 去重（`GetMethods` 重复） |
| 属性 attribute | ✅ | 未测缺失/类型错误/碰撞 |
| Compare | ⚠️ | 只测对称/相等，未测传递性、跨类型有损、顺序依赖 |
| 对象类型 Compare | ❌ | 恒 Failed |
| DynamicType 基础 | ✅ | — |
| DynamicType 属性读写 | ✅ | 未测未初始化/错类型/跨类型键冲突 |
| DynamicType 方法（带参数查找） | ❌ | `GetMethod(name,args)` 经 `Type*` 返回 null（P1-08） |
| DynamicType 构造器挂载 | ❌ | `NewDynamicConstructor` 无挂载入口 |
| DynamicType 继承动态类型 | ✅ | 仅属性，未测方法/转换 |
| DynamicType 继承编译期类型 | ⚠️ | 只断言属性非空且 `IsAssignableFrom==true`（`test.cpp:1082-1095`），**未调用**（P0-02） |
| 重复注册/幂等 | ❌ | 测试主动重复注册但断言用 `>=` 掩盖 |
| 命名空间同名类型 | ❌ | — |
| 线程安全 | ❌ | — |
| Release/NDEBUG | ❌ | 测试仅 Debug |
| sanitizer（ASan/UBSan/TSan） | ❌ | — |
| 自定义 pointer ops / Object base | ❌ | 文档承诺但无测试 |
| `-fno-rtti` | ❌ | 本次人工验证通过，但无 CI |
| 安装/find_package 消费 | ❌ | 本次人工验证（见附录 C） |
| 错误日志/状态 | ❌ | 测试定义 `RTTI_ENABLE_LOG` 但不校验 |
| 大属性/方法规模性能 | ❌ | — |
| 死亡测试（应崩溃/断言） | ❌ | — |

### 12.3 测试设计问题

1. **断言过弱**：`ctors.size() >= 2`（`test.cpp:310`）、`props.size() >= 3`（`:744`）、`allProps.size() >= 2`（`:1056`）——重复注册刚好让这些"永远成立"，掩盖 P1-01。
2. **无隔离**：所有用例共享全局注册表，顺序依赖；`RegisterAllTypes` 在每个用例里重复执行，已使 `int` 的构造器数量翻倍（probe07）。
3. **无失败路径断言**：没有一条 `EXPECT_FALSE(ok)`；`Type::Convert` 返回 false 的分支（`Type.h:519`）从未执行。
4. **无死亡测试**：P0-01 这类崩溃无法被普通断言捕获；应使用 fork/`EXPECT_DEATH` 或 sanitizer CI。
5. **无 NDEBUG/Release 运行**：P0-03/P0-08 在 Release 才暴露。
6. **测试自身有未使用变量/弱检查**：`run_test` 返回值被忽略；`g_tests` 只用于打印；汇总按断言数而非用例数。
7. **`test_dynamic_subclass_compile_time` 把危险行为固化为正确**（P0-02），需要重写而不是保留。
8. **框架无超时**：ForEach/循环类问题（P0-06）会挂死 CI；应给每个测试加超时（CTest `TIMEOUT`）。

### 12.4 必须新增的测试（按优先级）

1. 枚举/整型全宽度矩阵转换（含失败期望），覆盖 P0-01。
2. 动态类型继承编译期类型的拒绝或安全适配（P0-02）。
3. `Unbox<T>` 错误类型（Release + Debug）、`Unbox<void*>`/`Unbox<int*>`/`GetPointer` 语义（P0-03）。
4. 参数转换失败：目标方法不得被调用、属性不得被修改、`Invoke` 返回失败（P0-04）。
5. `CanConvertTo(nullptr)`/`Convert(nullptr)`/`PropertyInfo` 空对象（P0-05）。
6. `ForEach` 中注册类型的检测/超时（P0-06）。
7. TSan 并发 `type_of`；ASan/UBSan 全量测试（P0-07）。
8. `-DNDEBUG` 下全部测试与 sanitizer（P0-08）。
9. 重复注册幂等（成员数量精确相等）、`InitCoreType` 多次（P1-01）。
10. 命名空间同名、同名覆盖、重命名、跨平台内置名（P1-02）。
11. 自定义 pointer_ops/Object base 的编译与运行（P1-03）。
12. `cast` 组合矩阵（编译期 `static_assert` + 运行期）（P1-04）。
13. 非 const 引用参数（放开后）、getter/setter 签名组合、`New({})`（P1-05）。
14. 派生类型继承转换/比较；对象相等；`comparer` 编译（P1-06）。
15. `Compare` 的顺序无关性（多次进程/不同注册顺序）（P1-07）。
16. DynamicType 的 `GetConstructor(list)`、`GetMethod(name,args)`、owner、构造挂载、未初始化属性、错类型写入、跨类型键（P1-08/09）。
17. `PtrFromRaw`、`ctor` 分配策略（P1-03）。
18. 可变参数 `Invoke` 传对象与 vector 重载结果一致（P0-09）。
19. 栈上 `Type`/`Type` 拷贝/`delete` 后的 `Find` 行为（P1-13）。
20. 对象 `.convert<U>()` 在未定义 `ConvertTo` 时的编译期诊断（P1-14）。
21. `Box(Ptr<T>)` 的类型名/语义（P1-15）。
22. 安装 + `find_package` 消费 + `#include <RTTI/RTTI.h>`（文档一致性）（P2-20/21）。
23. CI：Debug/Release × ASan/UBSan/TSan × macOS/Linux/Windows；`ctest --no-tests=error`。

### 12.5 CI 建议配置（最小可用）

```yaml
jobs:
  test:
    strategy:
      matrix:
        os: [macos-latest, ubuntu-latest, windows-latest]
        build: [Debug, Release]
        sanitizer: [none, address+undefined]   # TSan 单独 job
    steps:
      - uses: actions/checkout@v4
      - run: cmake -B build -DRTTI_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=${{ matrix.build }}
      - run: cmake --build build --parallel
      - run: ctest --test-dir build --output-on-failure --no-tests=error
  tsan:
    runs-on: macos-latest
    steps: ... -DCMAKE_CXX_FLAGS="-fsanitize=thread" ...
  install:
    steps: cmake --install build --prefix ...; consumer find_package(RTTI); #include <RTTI.h>
  no-rtti:
    steps: -DCMAKE_CXX_FLAGS="-fno-rtti"
```

---

## 13. 构建 / 打包 / 文档

### 13.1 CMake

- **选项名**：`BUILD_RTTI_TESTS`（无前缀）+ 文档用了三个不同名字（`RTTI_BUILD_TEST`/`BUILD_RTTI_TESTS`/`RTTI_BUILD_TEST`），CI 因此空跑（P1-12）。
- **全局标准**：`set(CMAKE_CXX_STANDARD 20)` 应改为 `target_compile_features(rtti INTERFACE cxx_std_20)` 已有，删除全局 set 即可。
- **GLOB**：`file(GLOB_RECURSE)` 收集头文件，新增头文件需重新 configure；建议显式列表或 `CONFIGURE_DEPENDS`。
- **GNUInstallDirs**：`include(GNUInstallDirs)` 在 `install()` 之前，顺序正确；但 `INSTALL_INTERFACE:include/RTTI` 与安装目录 `${CMAKE_INSTALL_INCLUDEDIR}/RTTI` 硬编码重复，覆盖 `CMAKE_INSTALL_INCLUDEDIR` 时不一致（P2-20）。
- **导出**：`install(TARGETS rtti EXPORT ...)` 对 INTERFACE 库可用（实测安装成功），但 `ARCHIVE/LIBRARY/RUNTIME DESTINATION` 对 INTERFACE 无意义，可删；`write_basic_package_version_file` 建议 `ARCH_INDEPENDENT`。
- **版本**：`project(... VERSION 1.0.0)` 与 tag `0.9.5` 不一致。
- **include 布局**：安装后头文件在 `include/RTTI/*.h`，目标 include 目录是 `include/RTTI`，所以消费者必须 `#include <RTTI.h>`；README 的 `<RTTI/RTTI.h>` 不成立（附录 C）。作为 FetchContent 时 include 目录是 `src/`，同样只有 `<RTTI.h>` 可用。
- **通用头文件名**：`src/Config.h`、`System.h`、`Type.h`、`Object.h` 通过 `-I src` 暴露给消费者，极易与其他依赖同名；建议 `src/RTTI/*.h` + `#include <RTTI/Type.h>` 内部自洽，安装到 `include/`（而非 `include/RTTI` 作为 include 根）。

### 13.2 安装与消费实测

```
cmake --install build --prefix _review_scratch/prefix
# 安装：include/RTTI/*.h, lib/cmake/rtti/RTTIConfig.cmake, RTTITargets.cmake, RTTIConfigVersion.cmake

consumer: find_package(RTTI REQUIRED); target_link_libraries(consumer PRIVATE rtti::rtti)
  #include <RTTI.h>        -> 编译通过
  #include <RTTI/RTTI.h>   -> 编译失败（目标 include 目录是 include/RTTI）
```

即 README 推荐的 include 写法与包的实际布局矛盾。

### 13.3 README/AGENTS.md 不一致（详见附录 C）

最严重的三处：

1. `rtti::create<MyClass>(42)`（README:103）**不存在**；
2. `prop->Get(obj, value)` / `prop->Set(obj, 100)`（README:114,117）**不存在**，实际是 `GetValue<T>/SetValue(obj, Box(...))`；
3. 动态类型示例 `std::map<std::string, Type*> schema`（README:176）无法匹配 `NewDynamicType(const map<string, DynamicPropertyDesc>&)`，编译失败。

另外：快速开始从未调用 `InitCoreType()`（必需，否则内置数值转换/比较不注册）；"仅静态库"与 INTERFACE header-only 矛盾；构建命令/二进制名错误；"属性名 PascalCase"与动态示例的小写属性名不一致。

### 13.4 合规

- 无 `LICENSE` 文件（README 声称 MIT）。
- `src/nameof.hpp` 是 Neargye nameof 0.10.3（MIT，保留版权头），但未在 README/CLAUDE 中说明来源、版本与升级方式；作为 `src/` 的一部分被当作项目代码维护，存在合规与维护风险。

---

## 14. 整改路线图

| 阶段 | 内容 | 预估 | 阻塞关系 |
|---|---|---|---|
| **Phase 0（立即，1–3 天）** | P0-01 枚举转换；P0-03 checked unbox + void* 语义；P0-04 失败不调用；P0-05 空指针；P0-08 用运行时校验替换 assert；P0-09 可变参数 Invoke | 2–3 天 | 无 |
| **Phase 1（1 周）** | P0-02 禁止/隔离跨域继承；P0-06 ForEach 快照；P0-07 注册表加锁/冻结；P1-01 所有权+幂等；P1-13 Type 封装；P1-12 CI 正确选项 | 4–5 天 | Phase 0 |
| **Phase 2（1–2 周）** | P1-02 类型身份/全限定名/冲突检测；P1-03 pointer_ops；P1-04 cast 收敛；P1-05 签名限制；P1-06/07 转换/比较继承与确定性；P1-08/09 动态 API 完整化；P1-10 错误模型；P1-14/15 转换协议与 handle 语义 | 7–10 天 | Phase 1 |
| **Phase 3（持续）** | P1-11 性能（hash 索引/span/去 function）；P2 命名/打包/文档；P3 清理；CI 矩阵扩展 | 3–5 天 + 持续 | Phase 2 |
| **验收** | 12.4 的 19 组测试 + ASan/UBSan/TSan/NDEBUG CI 全绿；100 次重复注册成员数不变；`find_package` 消费通过；`-fno-rtti` 通过 | — | 全部 |

> 若短期内必须继续在 HaloEngine 中使用：建议锁定当前 commit，禁止使用 DynamicType 跨编译期继承、`Unbox<void*>`、`cast<Enum>(整数)`，并在引擎侧封装安全的 `TryGet/TrySet/TryInvoke` 作为过渡层；同时把 P0-01/03/04 以 patch 形式 cherry-pick 到本地依赖。

---

## 15. 附录 A：关键复现（代码与输出）

> 所有探针均在 `rtti-src/_review_scratch/` 内编译，命令模板：
> `clang++ -std=c++20 -I src -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer probe.cpp -o probe`

### A.1 整型 → 枚举崩溃（P0-01）

```cpp
enum class Small : uint8_t { A = 1, B = 2 };
InitCoreType();
TypeRegister<Small>::New("Small").value("A", Small::A).value("B", Small::B);
bool ok = false;
Small v = cast<Small>(Box(2), &ok);          // int 输入
```

输出：

```
--- cast<Small>(Box(uint8_t(2))) ---   ok=1 v=2
--- cast<Small>(Box(int(2))) ---
src/Type.h:477:25: runtime error: member call on null pointer of type 'rtti::Type'
AddressSanitizer: SEGV ... in rtti::Type::Convert(...)
```

等价地，任何 `uint8_t/int64_t/unsigned` 实参配 `int32_t` 底层枚举都会触发。

### A.2 动态类型跨编译期基类（P0-02）

```cpp
class Base : public Object {
    TYPE_DECLARE(Object)
public:
    static void RegisterRTTI() {
        TypeRegister<Base>::New().property("Name", &Base::Name).method("Speak", &Base::Speak);
    }
    std::string Name = "base";
    void Speak() { printf("name='%s'\n", Name.c_str()); }
};

auto* dt = NewDynamicType("DynExtendsBase", {{"extra", type_of<float>()}}, {}, {}, type_of<Base>());
auto obj = dt->CreateInstance({});
auto b = cast<Base>(obj);        // 类型系统说 OK
b->Speak();                       // 实际对象是 DynamicObject
dt->GetProperty("Name")->GetValue<std::string>(obj);
```

输出：

```
dynamic object actual C++ type: DynamicObject at 0x6070000000a8
Base::IsAssignableFrom(obj) = 1
cast produced Base* = 0x6070000000a8 (same address as DynamicObject => not a Base)
  Base::Speak this=0x6070000000a8 name='�'
Name = �
```

### A.3 `Unbox` 类型混淆与 boxed `void*`（P0-03/P0-08）

```cpp
int val = 42;
auto b = Box(&val);
printf("%p %p %p\n", Unbox<void*>(b), cast<void*>(b), Unbox<int*>(b));

auto fb = Box(3.0f);
printf("%d\n", Unbox<int>(fb));

// -DNDEBUG -O2
auto sb = Box(std::string("abcdefgh"));
int* p = Unbox<int*>(sb);
printf("%p %d\n", (void*)p, *p);
```

输出：

```
&val = 0x16faba9d0
Unbox<void*>(b) = 0x6060000016d0   (box 内部指针槽)
cast<void*>(b)  = 0x6060000016b8   (Object*)
Unbox<int*>(b)  = 0x16faba9d0      (正确)
Unbox<int>(Box(3.0f)) = 1077936128

# NDEBUG
Unbox<int*>(Box(string)) = 0x1014c80d0, *p = 1684234849
```

### A.4 参数转换失败仍执行（P0-04）

```cpp
auto* m = t->GetMethod("TakeInt");          // int TakeInt(int)
m->Invoke(obj, {Box(std::string("not an int"))});
t->GetProperty("Value")->SetValue(obj, Box(std::string("hello")));   // int 属性
t->GetProperty("ReadOnly")->SetValue(obj, Box(99));                  // 只读
```

输出：

```
TakeInt called with v=0
Value after invalid SetValue = 0
SetValue on read-only property: no error reported (CanWrite=0)
```

### A.5 注册表重复注册/重命名（P1-01/P1-02）

```cpp
InitCoreType(); InitCoreType();
printf("%zu\n", type_of<int>()->GetConstructors().size());   // 4（应为 2）
Twice::RegisterRTTI(); Twice::RegisterRTTI(); Twice::RegisterRTTI();
printf("%zu %zu %zu\n", t->GetConstructors().size(), t->GetMethods().size(), t->GetProperties().size());
// 3 3 3（应为 1 1 1）

TypeRegister<long>::New("name_one");
TypeRegister<long>::New("name_two");
printf("%p %p\n", Type::Find("name_one"), Type::Find("name_two"));  // 0x0 / valid
auto* a = NewType("Dup", 1, ...); auto* b = NewType("Dup", 2, ...);
printf("%p\n", Type::Find("Dup"));   // 第二个；第一个被永久孤立
```

分配统计（100 次注册）：`+594` 活跃分配、`+66 408 B`、成员各 100 份。

### A.6 线程竞争（P0-07，TSan）

```cpp
std::thread t1([]{ for (int i=0;i<200;i++) type_of<T0>(); });
std::thread t2([]{ for (int i=0;i<200;i++) type_of<T1>(); });
```

输出：

```
WARNING: ThreadSanitizer: data race
  Read of size 8 ... rtti::Type::Type(...) (map insertion)
  Previous write of size 8 ... rtti::Type::Type(...)
```

### A.7 Compare 顺序依赖（P1-07）

```cpp
Type* f = type_of<float>();   // 先创建 float
InitCoreType();               // 再创建 int32
printf("%s\n", R(compare(Box(3.9f), Box(3))));
```

输出：

```
float=0x103396190 int32=0x1033968d0 (float allocated first)
compare(3.9f, 3) = Equals
compare(3, 3.9f) = Equals
```

作为对照，正常顺序（先 int32 后 float）同一调用的结果是 `NotEquals`。

### A.8 可变参数 `Invoke` 传对象（P0-09）

```cpp
class Sink : public Object {
    TYPE_DECLARE(Object)
public:
    void Take(const ObjectPtr& p) { printf("received: %s\n", p->GetRttiType()->GetName().c_str()); }
};
auto sink = MakePtr<Sink>();
auto* m = type_of<Sink>()->GetMethod("Take");
m->Invoke(sink, {MakePtr<Target>()});      // vector 重载
m->Invoke(sink, MakePtr<Target>());        // 可变参数重载
```

输出：

```
[vector overload: {MakePtr<Target>()}]
  received type: Target
[variadic overload: Invoke(sink, MakePtr<Target>())]
  received type: shared_ptr        // Boxed<shared_ptr<Target>>，不是 Target
```

### A.9 栈上 `Type` 导致注册表悬垂（P1-13）

```cpp
{ Type stack("StackType", 4, TypeFlags::None, nullptr, type_of<ObjectBox>(), {}); }
Type* p = Type::Find("StackType");
printf("name = %s\n", p->GetName().c_str());
```

输出（ASan）：

```
Find(StackType) = 0x16b3b2860 (dangling)
ERROR: AddressSanitizer: stack-use-after-scope ... in main
```

### A.10 其余探针一览

| 探针 | 验证点 | 结论 |
|---|---|---|
| probe01 | macOS `int64_t` 与 `long` 是不同类型 | 说明 LP64 冲突是平台相关（Linux 触发） |
| probe12 | TSan 注册表竞争 | 确认 P0-07 |
| probe14 | `DynamicType::GetConstructor(list)` | 编译失败（名字隐藏） |
| probe21c | `ForEach` 期间插入类型 | 新类型被遍历，可能无限循环 |
| probe23 | `New({})` 歧义；getter/setter 签名 | 编译失败 |
| probe24 | 命名空间同名类型 | 后者覆盖前者 |
| probe25 | int→bool、char→int、D:B 转换、动态方法签名查找 | 全部失败/为空 |
| probe26 | `PtrFromRaw<P/Q>` | 编译失败 |
| probe27 | 字符串字面量参数、void 失败 | 空串执行；返回值不可区分 |
| probe28/06/06b | Compare 有损/顺序依赖 | 结果随注册顺序变化 |
| probe29 | `select_overload` | 显式签名全部不可编译 |
| probe30 | 可变参数 `Invoke` 传对象；`Box(Ptr<T>)` | 传递 `Boxed<shared_ptr>`，类型名 "shared_ptr" |
| probe31 | 栈上 `Type` 析构后 `Find` | ASan stack-use-after-scope |

---

## 16. 附录 B：编译期限制矩阵（实测）

| 写法 | 结果 |
|---|---|
| `TypeRegister<X>::New({})` | 歧义（`New(map)` vs `New(string)`） |
| `.method("F", &X::F)`（`F(int&)`） | `static_assert` 失败：非 const 引用参数 |
| `.method("F", &X::F)`（`F(const int)` 按值） | 通过（顶层 const 被丢弃） |
| `.property("P", &Get, &Set)`（`string Get()` + `void Set(const string&)`） | 无匹配重载 |
| `.property("P", &X::field)`（字段） | 通过；内部 getter/setter 的 `IsStatic()` 错误为 true（P3） |
| `cast<int>(MakePtr<B>())` | `static_assert`：From 必须是 `ObjectPtr` |
| `cast<int*>(Box(&v))` | `static_assert` 失败 |
| `cast<void*>(Box(&v))` | 编译通过，返回 Object*（语义错） |
| `cast<TestBase*>(Ptr<Test>)` | `static_assert` 失败 |
| `cast<HandleBase>(Box(ht))` | 通过（测试覆盖） |
| `.compare<int>()`（对象类型） | 编译失败（`static_cast<int>(Ptr<T>)`） |
| `.compare<ObjectPtr>()`（对象类型） | 可编译（指针身份比较），但语义有限 |
| `NewDynamicType(name, schema, ..., type_of<Base>())` | 可编译，运行期 UB（P0-02） |
| `dt->GetConstructor({type_of<int>()})` | 编译失败（名字隐藏） |
| `((Type*)dt)->GetMethod("Add", {int,int})` | 返回 nullptr（未 override） |
| `PtrFromRaw(T*)`（T≠Object） | 编译失败 |
| `Unbox<void*>(Box(&v))` | 可编译，返回值错误 |
| `select_overload<Sig>(&C::f)` | 编译失败（全部形式） |
| `Type stack("X", ...)` 后 `Find("X")` | 编译通过，返回悬垂指针（ASan stack-use-after-scope） |
| `Invoke(obj, MakePtr<T>())`（可变参数） | 编译通过，方法收到 `Boxed<shared_ptr<T>>` |
| `Box(MakePtr<T>())` | 编译通过，产生名为 `"shared_ptr"` 的值类型 |
| `-fno-rtti` 编译 `tests/test.cpp` | 通过 |
| find_package 后 `#include <RTTI.h>` | 通过 |
| find_package 后 `#include <RTTI/RTTI.h>` | 失败 |

---

## 17. 附录 C：文档与实现不一致清单

| 文档位置 | 文档声称 | 实际 |
|---|---|---|
| `README.md:57,172,327,336` | `#include <RTTI/RTTI.h>` | 安装后 include 根是 `include/RTTI`，应 `#include <RTTI.h>`（实测） |
| `README.md:103` | `auto obj = rtti::create<MyClass>(42);` | 不存在 `create`；应 `type_of<MyClass>()->Create<MyClass>(42)` |
| `README.md:114,117` | `prop->Get(obj, value)` / `prop->Set(obj, 100)` | 不存在；应 `GetValue<T>(obj)` / `SetValue(obj, Box(100))` |
| `README.md:162` | `.convert<int>()` "允许 MyClass 转换为 int" | 对象类型还必须在类里定义未声明的 `template <typename U> bool ConvertTo(U&)`，否则编译失败（P1-14） |
| `README.md:176` | `std::map<std::string, Type*> schema;` 传给 `NewDynamicType` | 参数是 `map<string, DynamicPropertyDesc>`；该 map 不能隐式转换 |
| `README.md:362` | `cmake .. -DRTTI_BUILD_TEST=ON` | 选项是 `BUILD_RTTI_TESTS` |
| `README.md:364` | `./RTTI-Test` | 目标是 `rtti_test` |
| `README.md:14` | "仅静态库" | INTERFACE header-only，无静态库 |
| `README.md` 快速开始 | 未调用 `InitCoreType()` | 内置类型转换/比较不会注册 |
| `README.md:14,120` | "属性名 PascalCase" | 动态类型示例/测试使用 `x`/`y`/`name` 小写 |
| `README.md` 动态类型 | `map<string, Type*>` schema | 实际 `DynamicPropertyDesc` |
| `AGENTS.md:96` | 枚举↔整数互转 | int→enum 在宽度不匹配时崩溃（P0-01） |
| `AGENTS.md` "Compare 对称性" | 保证 `Compare(A,B) == Compare(B,A)` | 对称成立，但 comparer 选择随地址变化，语义不确定（P1-07） |
| `AGENTS.md` "MethodInfo 安全性" | 只接受函数指针/成员指针/无捕获 lambda | 正确；但非 const 引用参数会被另一条 static_assert 拒绝（P1-05） |
| `AGENTS.md:113` | `-fno-rtti` 兼容 | 本次实测通过；`nameof.hpp` 的 `NAMEOF_*_RTTI` 宏若被使用会引入 `typeid` |
| README 安装 | `cmake` 裸命令 + `find_package` | 未说明 `RTTI_DIR`/`CMAKE_PREFIX_PATH` |
| 仓库根 | README 写 MIT License | 无 LICENSE 文件 |
| `.vscode/settings.json` | `RTTI_BUILD_TEST=ON` | 同 CI 错误 |

---

## 18. 审查边界声明

- 本报告仅基于 `build/_deps/rtti-src` 内的代码与文档，未查看 HaloEngine 其他目录的调用方式，因此"引擎侧影响"均为基于 API 形态的推断，而非对集成代码的断言。
- 未在 Windows/Linux 实际编译；跨平台结论基于标准类型定义与等价机制复现（`int64_t`/`long` 的注册表冲突用"同一 T 两次重命名"复现了完全相同的代码路径）。
- 性能数字来自单机探针，用于相对量级比较，不构成基准承诺。
- 所有探针已在报告完成后删除；复现代码见附录 A。

---

**报告结束**
