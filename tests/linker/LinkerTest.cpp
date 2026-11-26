#include "linker/Linker.hpp"

#include "linker/ObjectRecords.hpp"

#include <array>
#include <gtest/gtest.h>
#include <utility>

namespace linker {
namespace {

std::array<ObjectModule, 2> relocationExample() {
    ObjectModule code;
    code.header = {"MAIN", 0x16, {}};
    code.references = {{"EXT", {}}};
    code.text = {{0,
                  {0x4B, 0xB0, 0x00, 0x03, 0x00, 0x02, 0x50, 0x00, 0x00, 0x01,
                   0x01, 0x10, 0x00, 0x2A, 0x03, 0x20, 0x01, 0x03, 0x40, 0x02},
                  {}}};
    code.modifications = {
        {1, 5, ModificationSign::Add, "EXT", {}},
        {1, 5, ModificationSign::Add, "EXT", {}},
        {4, 6, ModificationSign::Subtract, "EXT", {}},
        {4, 6, ModificationSign::Subtract, "EXT", {}},
        {7, 6, ModificationSign::Add, "EXT", {}},
        {7, 6, ModificationSign::Subtract, "MAIN", {}},
    };
    ObjectModule library;
    library.header = {"LIB", 4, {}};
    library.definitions = {{"EXT", 1, {}}};
    library.text = {{0, {0xB4, 0x10, 0xAA, 0xBB}, {}}};
    library.entry.offset = 0;
    return {code, library};
}

void expectFailure(const LinkResult& result, ObjectDiagnosticCode code) {
    const auto* failure = std::get_if<ObjectFailure>(&result);
    ASSERT_NE(failure, nullptr);
    ASSERT_FALSE(failure->diagnostics.empty());
    EXPECT_EQ(failure->diagnostics.front().code, code);
}

ObjectModule section(std::string name, std::uint32_t length) {
    ObjectModule module;
    module.header = {std::move(name), length, {}};
    return module;
}

TEST(LinkerTest, AbsoluteImageFillsReservationsAndReportsTheLoadedEntry) {
    ObjectModule code;
    code.header.name = "MAIN";
    code.header.length = 5;
    code.text = {{1, {0xB4, 0x00}, {}}};
    code.entry.offset = 1;
    const std::array modules{code};

    const auto result = link(modules, AbsoluteLinkMode{0x100});

    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(result));
    const auto& image = std::get<AbsoluteLinkOutput>(result);
    EXPECT_EQ(image.bytes, (std::vector<std::uint8_t>{0, 0xB4, 0, 0, 0}));
    EXPECT_EQ(image.loadAddress, 0x100U);
    EXPECT_EQ(image.initialPc, 0x101U);
}

TEST(LinkerTest, AppliesSignedLoadTermsAndPreservesOtherBytesAtTwoAddresses) {
    const auto modules = relocationExample();
    // EXT resolves to 0x17. The fields are 3+2*(load+0x17),
    // 0x250-2*(load+0x17), and 1+(load+0x17)-load.
    const auto first = link(modules, AbsoluteLinkMode{0x100});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(first));
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(first),
              (AbsoluteLinkOutput{{0x4B, 0xB0, 0x02, 0x31, 0,    0,    0x22, 0,   0,
                                   0x18, 0x01, 0x10, 0,    0x2A, 0x03, 0x20, 1,   0x03,
                                   0x40, 2,    0,    0,    0xB4, 0x10, 0xAA, 0xBB},
                                  0x100,
                                  0x116}));
    const auto second = link(modules, AbsoluteLinkMode{0x80});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(second));
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(second),
              (AbsoluteLinkOutput{{0x4B, 0xB0, 0x01, 0x31, 0,    1,    0x22, 0,   0,
                                   0x18, 0x01, 0x10, 0,    0x2A, 0x03, 0x20, 1,   0x03,
                                   0x40, 2,    0,    0,    0xB4, 0x10, 0xAA, 0xBB},
                                  0x80,
                                  0x96}));
}

TEST(LinkerTest, SerializedCombinedObjectRelocatesLikeTheOriginalModules) {
    const auto modules = relocationExample();
    const auto result = link(modules, RelocatableLinkMode{});
    ASSERT_TRUE(std::holds_alternative<RelocatableLinkOutput>(result));
    const auto& relocatable = std::get<RelocatableLinkOutput>(result);
    EXPECT_EQ(relocatable.records,
              "HMAIN  00000000001A\n"
              "T000000144BB000310002220000180110002A032001034002\n"
              "T00001604B410AABB\n"
              "M00000105+MAIN  \nM00000105+MAIN  \n"
              "M00000406-MAIN  \nM00000406-MAIN  \n"
              "E000016\n");
    EXPECT_TRUE(relocatable.module.definitions.empty());
    EXPECT_TRUE(relocatable.module.references.empty());
    const auto parsed = parseObject(relocatable.records, "combined.obj");
    ASSERT_TRUE(std::holds_alternative<ObjectModule>(parsed));
    const std::array combined{std::get<ObjectModule>(parsed)};
    for (const auto load : {0U, 0x80U, 0x100U}) {
        SCOPED_TRACE(load);
        const auto direct = link(modules, AbsoluteLinkMode{load});
        const auto reloaded = link(combined, AbsoluteLinkMode{load});
        ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(direct));
        ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(reloaded));
        EXPECT_EQ(std::get<AbsoluteLinkOutput>(reloaded), std::get<AbsoluteLinkOutput>(direct));
    }
}

TEST(LinkerTest, SplitsLongTextIntoRecordsAndPreservesTrailingReservationsAndImplicitEntry) {
    auto first = section("First", 35);
    first.text = {{0, std::vector<std::uint8_t>(16, 0xAA), {}},
                  {16, std::vector<std::uint8_t>(16, 0xBB), {}}};
    first.modifications = {{29, 5, ModificationSign::Add, "FIRST", {}}};
    const std::array modules{first, section("TAIL", 5)};
    const auto result = link(modules, RelocatableLinkMode{});
    ASSERT_TRUE(std::holds_alternative<RelocatableLinkOutput>(result));
    const auto& linked = std::get<RelocatableLinkOutput>(result);
    EXPECT_EQ(linked.records,
              "HFirst 000000000028\n"
              "T0000001EAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBBBBBBBBB\n"
              "T00001E02BBBB\nM00001D05+First \nE\n");
    EXPECT_EQ(linked.module.header.length, 40U);
    EXPECT_FALSE(linked.module.entry.offset);
    const auto parsed = parseObject(linked.records, "split.obj");
    ASSERT_TRUE(std::holds_alternative<ObjectModule>(parsed));
    const auto loaded = link(std::array{std::get<ObjectModule>(parsed)}, AbsoluteLinkMode{1});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(loaded));
    const auto& image = std::get<AbsoluteLinkOutput>(loaded);
    ASSERT_EQ(image.bytes.size(), 40U);
    EXPECT_EQ(image.initialPc, 1U);
    EXPECT_EQ(image.bytes[29], 0xBBU);
    EXPECT_EQ(image.bytes[30], 0xBBU);
    EXPECT_EQ(image.bytes[31], 0xBCU);
}

TEST(LinkerTest, ValidatesLoadImageAndEntryAtTheAddressSpaceBoundary) {
    auto one = section("ONE", 1);
    one.entry.offset = 0;
    one.text = {{0, {0xAB}, {}}};
    const auto lastByte = link(std::array{one}, AbsoluteLinkMode{0xFFFFF});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(lastByte));
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(lastByte),
              (AbsoluteLinkOutput{{0xAB}, 0xFFFFF, 0xFFFFF}));
    expectFailure(link(std::array{one}, AbsoluteLinkMode{0x100000}),
                  ObjectDiagnosticCode::ValueOutOfRange);
    expectFailure(link(std::array{one, section("EXTRA", 1)}, AbsoluteLinkMode{0xFFFFF}),
                  ObjectDiagnosticCode::ValueOutOfRange);

    auto full = section("FULL", 0x100000);
    full.entry.offset = 0xFFFFF;
    const auto atZero = link(std::array{full}, AbsoluteLinkMode{0});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(atZero));
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(atZero).initialPc, 0xFFFFFU);
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(atZero).bytes, std::vector<std::uint8_t>(0x100000));
    expectFailure(link(std::array{full}, AbsoluteLinkMode{1}),
                  ObjectDiagnosticCode::ValueOutOfRange);
    full.entry.offset = 0x100000;
    expectFailure(link(std::array{full}, AbsoluteLinkMode{0}),
                  ObjectDiagnosticCode::ValueOutOfRange);
}

TEST(LinkerTest, RejectsFinalFormatFourOverflowWithoutWrappingTheAddress) {
    auto code = section("MAIN", 3);
    code.text = {{0, {0xBF, 0xFF, 0xFF}, {}}};
    const ObjectPosition position{"main.obj", 3, 1};
    code.modifications = {{0, 5, ModificationSign::Add, "MAIN", position}};
    const std::array modules{code};
    const auto exact = link(modules, AbsoluteLinkMode{0});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(exact));
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(exact).bytes,
              (std::vector<std::uint8_t>{0xBF, 0xFF, 0xFF}));
    const auto failed = link(modules, AbsoluteLinkMode{1});
    expectFailure(failed, ObjectDiagnosticCode::ValueOutOfRange);
    const auto* failure = std::get_if<ObjectFailure>(&failed);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->diagnostics.front().position, position);
}

TEST(LinkerTest, ExactPreLoadValuesCanBeRepairedByAbsoluteLoadButCannotBeSerialized) {
    auto code = section("MAIN", 6);
    code.references = {{"EXT", {}}};
    code.text = {{0, {0xBF, 0xFF, 0xFF, 0x10, 0, 0}, {}}};
    code.modifications = {
        {0, 5, ModificationSign::Add, "EXT", {}},
        {0, 5, ModificationSign::Subtract, "MAIN", {}},
        {0, 5, ModificationSign::Subtract, "MAIN", {}},
        {3, 5, ModificationSign::Add, "MAIN", {}},
        {3, 5, ModificationSign::Add, "MAIN", {}},
        {3, 5, ModificationSign::Subtract, "EXT", {}},
    };
    const std::array modules{code, section("EXT", 1)};
    // Before load: 0x100005 and -6. At load six they become 0xFFFFF and zero.
    const auto repaired = link(modules, AbsoluteLinkMode{6});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(repaired));
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(repaired).bytes,
              (std::vector<std::uint8_t>{0xBF, 0xFF, 0xFF, 0x10, 0, 0, 0}));
    expectFailure(link(modules, RelocatableLinkMode{}), ObjectDiagnosticCode::ValueOutOfRange);
    expectFailure(link(modules, AbsoluteLinkMode{5}), ObjectDiagnosticCode::ValueOutOfRange);
    code.modifications.erase(code.modifications.begin(), code.modifications.begin() + 3);
    expectFailure(link(std::array{code, modules.back()}, RelocatableLinkMode{}),
                  ObjectDiagnosticCode::ValueOutOfRange);
}

TEST(LinkerTest, ZeroCoefficientStillRequiresTheExactFormatFourValueToFit) {
    auto code = section("MAIN", 3);
    code.references = {{"EXT", {}}};
    code.text = {{0, {0x1F, 0xFF, 0xFF}, {}}};
    code.modifications = {{0, 5, ModificationSign::Add, "EXT", {}},
                          {0, 5, ModificationSign::Subtract, "MAIN", {}}};
    const std::array modules{code, section("EXT", 1)};
    expectFailure(link(modules, AbsoluteLinkMode{0x100}), ObjectDiagnosticCode::ValueOutOfRange);
    expectFailure(link(modules, RelocatableLinkMode{}), ObjectDiagnosticCode::ValueOutOfRange);
}

TEST(LinkerTest, WordsWrapAtTwentyFourBitsInBothDirections) {
    auto code = section("MAIN", 6);
    code.text = {{0, {0xFF, 0xFF, 0xFF, 0, 0, 0}, {}}};
    code.modifications = {{0, 6, ModificationSign::Add, "MAIN", {}},
                          {3, 6, ModificationSign::Subtract, "MAIN", {}}};
    const auto result = link(std::array{code}, AbsoluteLinkMode{1});
    ASSERT_TRUE(std::holds_alternative<AbsoluteLinkOutput>(result));
    EXPECT_EQ(std::get<AbsoluteLinkOutput>(result).bytes,
              (std::vector<std::uint8_t>{0, 0, 0, 0xFF, 0xFF, 0xFF}));
}

TEST(LinkerTest, PropagatesLinkPassFailuresAndDistinguishesEmptyModuleFromEmptyImage) {
    const std::span<const ObjectModule> noModules;
    expectFailure(link(noModules, RelocatableLinkMode{}), ObjectDiagnosticCode::MissingRecord);
    const std::array empty{section("EMPTY", 0)};
    const auto relocatable = link(empty, RelocatableLinkMode{});
    ASSERT_TRUE(std::holds_alternative<RelocatableLinkOutput>(relocatable));
    EXPECT_EQ(std::get<RelocatableLinkOutput>(relocatable).records, "HEMPTY 000000000000\nE\n");
    expectFailure(link(empty, AbsoluteLinkMode{0}), ObjectDiagnosticCode::ValueOutOfRange);

    const auto modules = relocationExample();
    const std::array unresolved{modules.front()};
    expectFailure(link(unresolved, AbsoluteLinkMode{0}), ObjectDiagnosticCode::UndeclaredReference);
    expectFailure(link(unresolved, RelocatableLinkMode{}),
                  ObjectDiagnosticCode::UndeclaredReference);
}

} // namespace
} // namespace linker
