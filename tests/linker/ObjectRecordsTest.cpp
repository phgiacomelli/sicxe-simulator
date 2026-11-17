#include "linker/ObjectRecords.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>

namespace linker {
namespace {

template <typename Result>
void expectError(const Result& result, ObjectDiagnosticCode code, ObjectPosition position) {
    const auto* failure = std::get_if<ObjectFailure>(&result);
    ASSERT_NE(failure, nullptr);
    ASSERT_EQ(failure->diagnostics.size(), 1U);
    EXPECT_EQ(failure->diagnostics[0].code, code);
    EXPECT_EQ(failure->diagnostics[0].position, position);
    EXPECT_FALSE(failure->diagnostics[0].message.empty());
}

TEST(ObjectRecordsTest, ParsesAReservedSectionWithAnOptionalEntry) {
    const auto result = parseObject("HMAIN  000000000020\nE\n", "main.obj");
    const auto* module = std::get_if<ObjectModule>(&result);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->header.name, "MAIN");
    EXPECT_EQ(module->header.length, 0x20U);
    EXPECT_EQ(module->header.position, (ObjectPosition{"main.obj", 1, 1}));
    EXPECT_TRUE(module->text.empty());
    EXPECT_FALSE(module->entry.offset);
    EXPECT_EQ(module->entry.position, (ObjectPosition{"main.obj", 2, 1}));
}

TEST(ObjectRecordsTest, ParsesAllRecordTypesAndNormalizesUnnamedModifications) {
    const auto result = parseObject("HCOPY  000000000020\n"
                                    "DVALUE 000010\n"
                                    "REXT   OTHER \n"
                                    "T000000064B1000000000\n"
                                    "T00001003000001\n"
                                    "M00000105+EXT   \n"
                                    "M00000105-OTHER \n"
                                    "M00000306\n"
                                    "E000000\n",
                                    "copy.obj");
    const auto* module = std::get_if<ObjectModule>(&result);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(module->definitions.size(), 1U);
    EXPECT_EQ(module->definitions[0], (DefinedSymbol{"VALUE", 0x10, {"copy.obj", 2, 2}}));
    ASSERT_EQ(module->references.size(), 2U);
    EXPECT_EQ(module->references[0], (ReferencedSymbol{"EXT", {"copy.obj", 3, 2}}));
    EXPECT_EQ(module->references[1], (ReferencedSymbol{"OTHER", {"copy.obj", 3, 8}}));
    ASSERT_EQ(module->text.size(), 2U);
    EXPECT_EQ(module->text[0].bytes, (std::vector<std::uint8_t>{0x4B, 0x10, 0, 0, 0, 0}));
    EXPECT_EQ(module->text[1].offset, 0x10U);
    EXPECT_EQ(module->text[1].bytes, (std::vector<std::uint8_t>{0, 0, 1}));
    ASSERT_EQ(module->modifications.size(), 3U);
    EXPECT_EQ(module->modifications[0],
              (ModificationRecord{1, 5, ModificationSign::Add, "EXT", {"copy.obj", 6, 1}}));
    EXPECT_EQ(module->modifications[1].sign, ModificationSign::Subtract);
    EXPECT_EQ(module->modifications[1].symbol, "OTHER");
    EXPECT_EQ(module->modifications[2],
              (ModificationRecord{3, 6, ModificationSign::Add, "COPY", {"copy.obj", 8, 1}}));
    EXPECT_EQ(module->entry.offset, 0U);
}

TEST(ObjectRecordsTest, RendersCanonicalRecordsWithoutChangingBytesHolesOrModificationOrder) {
    const auto parsed = parseObject("HCopy  000000000020\r\nDVALUE 000010\rReXt   OTHER \n"
                                    "T000000064b1000000000\nT000010030000ff\n"
                                    "M00000105+eXt   \nM00000105-OTHER \nM00000306\nE000000",
                                    "copy.obj");
    const auto* module = std::get_if<ObjectModule>(&parsed);
    ASSERT_NE(module, nullptr);
    const auto rendered = renderObject(*module);
    const auto* text = std::get_if<std::string>(&rendered);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(*text,
              "HCopy  000000000020\nDVALUE 000010\nReXt   OTHER \n"
              "T000000064B1000000000\nT000010030000FF\n"
              "M00000105+eXt   \nM00000105-OTHER \nM00000306+Copy  \nE000000\n");
    const auto reparsed = parseObject(*text, "copy.obj");
    const auto* same = std::get_if<ObjectModule>(&reparsed);
    ASSERT_NE(same, nullptr);
    EXPECT_EQ(*same, *module);
}

TEST(ObjectRecordsTest, ParseAndRenderRejectOverlappingTextInsteadOfPublishingAPartialModule) {
    const auto parsed =
        parseObject("HMAIN  000000000004\nT00000003AABBCC\nT00000202DDEE\nE\n", "overlap.obj");
    const auto* failure = std::get_if<ObjectFailure>(&parsed);
    ASSERT_NE(failure, nullptr);
    ASSERT_EQ(failure->diagnostics.size(), 1U);
    EXPECT_EQ(failure->diagnostics[0].code, ObjectDiagnosticCode::OverlappingText);
    EXPECT_EQ(failure->diagnostics[0].position, (ObjectPosition{"overlap.obj", 3, 2}));
    ObjectModule invalid;
    invalid.header = {"MAIN", 4, {"overlap.obj", 1, 1}};
    invalid.text = {{0, {0xAA, 0xBB, 0xCC}, {"overlap.obj", 2, 1}},
                    {2, {0xDD, 0xEE}, {"overlap.obj", 3, 1}}};
    const auto rendered = renderObject(invalid);
    const auto* rejected = std::get_if<ObjectFailure>(&rendered);
    ASSERT_NE(rejected, nullptr);
    EXPECT_EQ(rejected->diagnostics[0].code, ObjectDiagnosticCode::OverlappingText);
}

TEST(ObjectRecordsTest, RejectsModificationsTouchingAHoleOrUsingAnUndeclaredReference) {
    expectError(parseObject("HMAIN  000000000006\nT000000020310\nT00000303000000\n"
                            "M00000105\nE\n",
                            "hole.obj"),
                ObjectDiagnosticCode::MissingText,
                {"hole.obj", 4, 2});
    expectError(parseObject("HMAIN  000000000003\nREXT   \nT00000003000000\n"
                            "M00000006+OTHER \nE\n",
                            "external.obj"),
                ObjectDiagnosticCode::UndeclaredReference,
                {"external.obj", 4, 11});
    ObjectModule invalid;
    invalid.header = {"MAIN", 6, {"hole.obj", 1, 1}};
    invalid.text = {{0, {3, 0x10}, {"hole.obj", 2, 1}}, {3, {0, 0, 0}, {"hole.obj", 3, 1}}};
    invalid.modifications = {{1, 5, ModificationSign::Add, "MAIN", {"hole.obj", 4, 1}}};
    expectError(renderObject(invalid), ObjectDiagnosticCode::MissingText, {"hole.obj", 4, 2});
}

TEST(ObjectRecordsTest, RejectsRecordPhasesOutOfOrder) {
    expectError(
        parseObject("HMAIN  000000000003\nT00000003000000\nDVALUE 000000\nE\n", "order.obj"),
        ObjectDiagnosticCode::UnexpectedRecord,
        {"order.obj", 3, 1});
    expectError(
        parseObject("HMAIN  000000000003\nT00000003000000\nM00000006\nREXT   \nE\n", "order.obj"),
        ObjectDiagnosticCode::UnexpectedRecord,
        {"order.obj", 4, 1});
}

TEST(ObjectRecordsTest, RejectsDuplicateNamesWithoutDistinguishingCase) {
    expectError(parseObject("HMAIN  000000000003\nDValue 000000\nDvalue 000001\nE\n", "names.obj"),
                ObjectDiagnosticCode::DuplicateName,
                {"names.obj", 3, 2});
    expectError(parseObject("HMAIN  000000000003\nRmain  \nE\n", "names.obj"),
                ObjectDiagnosticCode::DuplicateName,
                {"names.obj", 2, 2});
    ObjectModule invalid;
    invalid.header = {"TOOLONG", 3, {"writer.obj", 1, 1}};
    expectError(renderObject(invalid), ObjectDiagnosticCode::InvalidName, {"writer.obj", 1, 8});
    invalid.header.name = "MAIN";
    invalid.definitions = {{"VALUE", 3, {"writer.obj", 2, 2}}};
    expectError(renderObject(invalid), ObjectDiagnosticCode::ValueOutOfRange, {"writer.obj", 2, 8});
}

struct MalformedCase {
    const char* name;
    const char* text;
    ObjectDiagnosticCode code;
    std::size_t record;
    std::size_t column;
};

const MalformedCase MalformedObjects[] = {
    {"Empty", "", ObjectDiagnosticCode::MissingRecord, 1, 1},
    {"NoHeader", "E\n", ObjectDiagnosticCode::MissingRecord, 1, 1},
    {"NoEnd", "HMAIN  000000000003\n", ObjectDiagnosticCode::MissingRecord, 2, 1},
    {"ShortHeader", "HMAIN  00000000000\nE\n", ObjectDiagnosticCode::InvalidRecordLength, 1, 19},
    {"LongHeader", "HMAIN  000000000003X\nE\n", ObjectDiagnosticCode::InvalidRecordLength, 1, 20},
    {"BadOriginHex", "HMAIN  00000G000003\nE\n", ObjectDiagnosticCode::InvalidHex, 1, 13},
    {"NonZeroOrigin", "HMAIN  000001000003\nE\n", ObjectDiagnosticCode::NonZeroOrigin, 1, 8},
    {"LengthOverflow", "HMAIN  000000100001\nE\n", ObjectDiagnosticCode::ValueOutOfRange, 1, 14},
    {"NumericName", "H1MAIN 000000000003\nE\n", ObjectDiagnosticCode::InvalidName, 1, 2},
    {"EmptyName", "H      000000000003\nE\n", ObjectDiagnosticCode::InvalidName, 1, 2},
    {"InnerSpace", "HMA IN 000000000003\nE\n", ObjectDiagnosticCode::InvalidName, 1, 4},
    {"UnknownRecord",
     "HMAIN  000000000003\nXanything\nE\n",
     ObjectDiagnosticCode::InvalidRecordType,
     2,
     1},
    {"LowercaseTag",
     "HMAIN  000000000003\nt0000000100\nE\n",
     ObjectDiagnosticCode::InvalidRecordType,
     2,
     1},
    {"EmptyRecord", "HMAIN  000000000003\n\nE\n", ObjectDiagnosticCode::InvalidRecordType, 2, 1},
    {"DuplicateHeader",
     "HMAIN  000000000003\nHOTHER 000000000003\nE\n",
     ObjectDiagnosticCode::UnexpectedRecord,
     2,
     1},
    {"DuplicateEnd", "HMAIN  000000000003\nE\nE\n", ObjectDiagnosticCode::UnexpectedRecord, 3, 1},
    {"AfterEnd",
     "HMAIN  000000000003\nE\nT0000000100\n",
     ObjectDiagnosticCode::UnexpectedRecord,
     3,
     1},
    {"EmptyDefinition",
     "HMAIN  000000000003\nD\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     2},
    {"ShortDefinition",
     "HMAIN  000000000003\nDVALUE 00000\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     13},
    {"EmptyReference",
     "HMAIN  000000000003\nR\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     2},
    {"ShortReference",
     "HMAIN  000000000003\nREXT\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     5},
    {"BadDefinitionHex",
     "HMAIN  000000000003\nDVALUE 00000Z\nE\n",
     ObjectDiagnosticCode::InvalidHex,
     2,
     13},
    {"DefinitionOutside",
     "HMAIN  000000000003\nDVALUE 000003\nE\n",
     ObjectDiagnosticCode::ValueOutOfRange,
     2,
     8},
    {"DuplicateReference",
     "HMAIN  000000000003\nREXT   ext   \nE\n",
     ObjectDiagnosticCode::DuplicateName,
     2,
     8},
    {"DefinitionReferenceConflict",
     "HMAIN  000000000003\nDVALUE 000000\nRvalue \nE\n",
     ObjectDiagnosticCode::DuplicateName,
     3,
     2},
    {"ShortTextHeader",
     "HMAIN  000000000003\nT00000\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     7},
    {"ZeroText",
     "HMAIN  000000000003\nT00000000\nE\n",
     ObjectDiagnosticCode::InvalidTextLength,
     2,
     8},
    {"TextTooLong",
     "HMAIN  000000000020\nT0000001F\nE\n",
     ObjectDiagnosticCode::InvalidTextLength,
     2,
     8},
    {"ShortPayload",
     "HMAIN  000000000003\nT000000030000\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     14},
    {"ExtraPayload",
     "HMAIN  000000000003\nT000000010000\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     12},
    {"BadPayloadHex",
     "HMAIN  000000000003\nT00000001G0\nE\n",
     ObjectDiagnosticCode::InvalidHex,
     2,
     10},
    {"TextOutside",
     "HMAIN  000000000003\nT0000030100\nE\n",
     ObjectDiagnosticCode::ValueOutOfRange,
     2,
     2},
    {"TextOffsetOverflow",
     "HMAIN  000000100000\nT1000000100\nE\n",
     ObjectDiagnosticCode::ValueOutOfRange,
     2,
     2},
    {"EntryOutside", "HMAIN  000000000003\nE000003\n", ObjectDiagnosticCode::ValueOutOfRange, 2, 2},
    {"ExtraEntry",
     "HMAIN  000000000003\nE000000X\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     8},
    {"ShortEntry", "HMAIN  000000000003\nE0000\n", ObjectDiagnosticCode::InvalidRecordLength, 2, 6},
    {"ShortModification",
     "HMAIN  000000000003\nM00000\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     2,
     7},
    {"BadModificationHex",
     "HMAIN  000000000003\nT00000003000000\nM0000000Z\nE\n",
     ObjectDiagnosticCode::InvalidHex,
     3,
     9},
    {"InvalidSign",
     "HMAIN  000000000003\nT00000003000000\nM00000006*MAIN  \nE\n",
     ObjectDiagnosticCode::InvalidModification,
     3,
     10},
    {"UnsupportedWidth",
     "HMAIN  000000000003\nT00000003000000\nM00000004\nE\n",
     ObjectDiagnosticCode::InvalidModification,
     3,
     8},
    {"PatchOutside",
     "HMAIN  000000000003\nT00000003000000\nM00000106\nE\n",
     ObjectDiagnosticCode::ValueOutOfRange,
     3,
     2},
    {"UndeclaredExternal",
     "HMAIN  000000000003\nT00000003000000\nM00000006+EXT   \nE\n",
     ObjectDiagnosticCode::UndeclaredReference,
     3,
     11},
    {"ShortNamedModification",
     "HMAIN  000000000003\nT00000003000000\nM00000006+\nE\n",
     ObjectDiagnosticCode::InvalidRecordLength,
     3,
     11},
    {"PatchWithoutText",
     "HMAIN  000000000003\nM00000006\nE\n",
     ObjectDiagnosticCode::MissingText,
     2,
     2},
};

class MalformedObjectTest : public testing::TestWithParam<MalformedCase> {};

TEST_P(MalformedObjectTest, ReportsThePhysicalRecordAndByteColumnWithoutPartialSuccess) {
    const auto& example = GetParam();
    expectError(parseObject(example.text, "bad.obj"),
                example.code,
                {"bad.obj", example.record, example.column});
}

INSTANTIATE_TEST_SUITE_P(InvalidRecords,
                         MalformedObjectTest,
                         testing::ValuesIn(MalformedObjects),
                         [](const testing::TestParamInfo<MalformedCase>& item) {
                             return item.param.name;
                         });

TEST(ObjectRecordsTest, AcceptsTheLastByteAndPatchesEndingAtTheAddressSpaceLimit) {
    const std::string source = "HMAIN  000000100000\nDLAST  0FFFFF\nT0FFFFD03ABCDEF\n"
                               "M0FFFFD05\nM0FFFFD06\nE0FFFFF\n";
    const auto parsed = parseObject(source);
    const auto* module = std::get_if<ObjectModule>(&parsed);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->header.length, 0x100000U);
    EXPECT_EQ(module->definitions[0].offset, 0xFFFFFU);
    EXPECT_EQ(module->entry.offset, 0xFFFFFU);
    EXPECT_EQ(module->text[0].bytes, (std::vector<std::uint8_t>{0xAB, 0xCD, 0xEF}));
    const auto rendered = renderObject(*module);
    ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
    EXPECT_EQ(std::get<std::string>(rendered),
              "HMAIN  000000100000\nDLAST  0FFFFF\nT0FFFFD03ABCDEF\n"
              "M0FFFFD05+MAIN  \nM0FFFFD06+MAIN  \nE0FFFFF\n");
}

TEST(ObjectRecordsTest, ModificationCanCrossAdjacentTextRecordsWithoutPatchingTheirBytes) {
    const auto result =
        parseObject("HMAIN  000000000003\nT00000001AF\nT00000102BCDE\nM00000005\nE\n");
    const auto* module = std::get_if<ObjectModule>(&result);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->text[0].bytes, (std::vector<std::uint8_t>{0xAF}));
    EXPECT_EQ(module->text[1].bytes, (std::vector<std::uint8_t>{0xBC, 0xDE}));
    EXPECT_TRUE(std::holds_alternative<std::string>(renderObject(*module)));
}

TEST(ObjectRecordsTest, AcceptsAnEmptySectionButDoesNotInventTextOrAnEntry) {
    const auto result = parseObject("HEMPTY 000000000000\nE");
    const auto* module = std::get_if<ObjectModule>(&result);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->header.length, 0U);
    EXPECT_TRUE(module->text.empty());
    EXPECT_FALSE(module->entry.offset);
    const auto rendered = renderObject(*module);
    ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
    EXPECT_EQ(std::get<std::string>(rendered), "HEMPTY 000000000000\nE\n");
}

TEST(ObjectRecordsTest, PreservesTextOrderWhenOffsetsAreNotSorted) {
    const std::string source = "HMAIN  000000000010\nT00000F01AA\nT0000000100\nE\n";
    const auto result = parseObject(source);
    const auto* module = std::get_if<ObjectModule>(&result);
    ASSERT_NE(module, nullptr);
    const auto rendered = renderObject(*module);
    ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
    EXPECT_EQ(std::get<std::string>(rendered), source);
}

TEST(ObjectRecordsTest, AcceptsExactlyThirtyTextBytesWithTrailingReservedSpace) {
    const std::string source = "HMAIN  000000000020\nT0000001E"
                               "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D\nE\n";
    const auto result = parseObject(source);
    const auto* module = std::get_if<ObjectModule>(&result);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(module->text.size(), 1U);
    EXPECT_EQ(module->text[0].bytes.size(), 30U);
    EXPECT_EQ(module->text[0].bytes.back(), 0x1DU);
    const auto rendered = renderObject(*module);
    ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
    EXPECT_EQ(std::get<std::string>(rendered), source);
}

TEST(ObjectRecordsTest, WriterGroupsDefinitionsAndReferencesIntoFixedWidthFields) {
    ObjectModule module;
    module.header = {"MAIN", 7, {}};
    for (unsigned int index = 0; index < 7; ++index)
        module.definitions.push_back({"D" + std::to_string(index), index, {}});
    for (unsigned int index = 0; index < 13; ++index)
        module.references.push_back({"R" + std::to_string(index), {}});
    const auto rendered = renderObject(module);
    const auto* text = std::get_if<std::string>(&rendered);
    ASSERT_NE(text, nullptr);
    std::istringstream input(*text);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);)
        lines.push_back(line);
    ASSERT_EQ(lines.size(), 6U);
    EXPECT_EQ(lines[0], "HMAIN  000000000007");
    EXPECT_EQ(lines[1].size(), 73U);
    EXPECT_EQ(lines[2], "DD6    000006");
    EXPECT_EQ(lines[3].size(), 73U);
    EXPECT_EQ(lines[4], "RR12   ");
    EXPECT_EQ(lines[5], "E");
    const auto reparsed = parseObject(*text);
    const auto* same = std::get_if<ObjectModule>(&reparsed);
    ASSERT_NE(same, nullptr);
    ASSERT_EQ(same->definitions.size(), 7U);
    ASSERT_EQ(same->references.size(), 13U);
    EXPECT_EQ(same->definitions[6].name, "D6");
    EXPECT_EQ(same->definitions[6].offset, 6U);
    EXPECT_EQ(same->references[12].name, "R12");
}

TEST(ObjectRecordsTest, OwnsAllInputNamesPositionsAndBytesAndRenderDoesNotMutateTheModel) {
    ObjectParseResult parsed;
    {
        std::string source =
            "HMAIN  000000000003\nREXT   \nT00000003ABCDEF\nM00000006+EXT   \nE000000\n";
        std::string file = "owned.obj";
        parsed = parseObject(source, file);
        source.assign(4096, '*');
        file.assign("changed");
    }
    const auto* module = std::get_if<ObjectModule>(&parsed);
    ASSERT_NE(module, nullptr);
    const auto snapshot = *module;
    const auto rendered = renderObject(*module);
    EXPECT_EQ(*module, snapshot);
    EXPECT_EQ(module->header.position.file, "owned.obj");
    EXPECT_EQ(module->references[0], (ReferencedSymbol{"EXT", {"owned.obj", 2, 2}}));
    EXPECT_EQ(module->text[0].bytes, (std::vector<std::uint8_t>{0xAB, 0xCD, 0xEF}));
    EXPECT_EQ(module->modifications[0].symbol, "EXT");
    parsed = ObjectFailure{};
    const auto* text = std::get_if<std::string>(&rendered);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(*text, "HMAIN  000000000003\nREXT   \nT00000003ABCDEF\nM00000006+EXT   \nE000000\n");
}

TEST(ObjectRecordsTest, InvocationsDoNotShareDeclaredNamesOrRecordOrderState) {
    const std::string source =
        "HMAIN  000000000003\nREXT   \nT00000003000000\nM00000006+EXT   \nE\n";
    EXPECT_TRUE(std::holds_alternative<ObjectModule>(parseObject(source)));
    expectError(
        parseObject("HMAIN  000000000003\nT00000003000000\nM00000006+EXT   \nE\n", "second.obj"),
        ObjectDiagnosticCode::UndeclaredReference,
        {"second.obj", 3, 11});
    EXPECT_TRUE(std::holds_alternative<ObjectModule>(parseObject(source)));
}

TEST(ObjectRecordsTest, RejectsEveryTruncatedPrefixBeforeTheRequiredEndRecord) {
    const std::string body = "HMAIN  000000000003\nDVALUE 000000\nREXT   \n"
                             "T00000003000000\nM00000006+EXT   \n";
    for (std::size_t length = 0; length <= body.size(); ++length) {
        SCOPED_TRACE(length);
        const auto result = parseObject(std::string_view{body}.substr(0, length), "prefix.obj");
        const auto* rejected = std::get_if<ObjectFailure>(&result);
        ASSERT_NE(rejected, nullptr);
        ASSERT_EQ(rejected->diagnostics.size(), 1U);
        EXPECT_EQ(rejected->diagnostics[0].position.file, "prefix.obj");
        EXPECT_GE(rejected->diagnostics[0].position.record, 1U);
        EXPECT_GE(rejected->diagnostics[0].position.column, 1U);
    }
}

TEST(ObjectRecordsTest, ReportsEmbeddedNulAndNonAsciiNamesAsInvalidBytes) {
    std::string source = "HMAIN  000000000001\nT00000001A0\nE\n";
    source[source.find("A0") + 1] = '\0';
    expectError(
        parseObject(source, "nul.obj"), ObjectDiagnosticCode::InvalidHex, {"nul.obj", 2, 11});
    expectError(parseObject("HM\xC3\xA9IN 000000000003\nE\n", "name.obj"),
                ObjectDiagnosticCode::InvalidName,
                {"name.obj", 1, 3});
}

TEST(ObjectRecordsTest, MixedTerminatorsAndFinalNewlineDoNotInventAnExtraRecord) {
    for (const auto* source : {"HMAIN  000000000001\nE\n",
                               "HMAIN  000000000001\r\nE\r\n",
                               "HMAIN  000000000001\rE",
                               "HMAIN  000000000001\r\nE\n"}) {
        SCOPED_TRACE(source);
        const auto parsed = parseObject(source, "lines.obj");
        const auto* module = std::get_if<ObjectModule>(&parsed);
        ASSERT_NE(module, nullptr);
        EXPECT_EQ(module->entry.position, (ObjectPosition{"lines.obj", 2, 1}));
        const auto rendered = renderObject(*module);
        ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
        EXPECT_EQ(std::get<std::string>(rendered), "HMAIN  000000000001\nE\n");
    }
}

TEST(ObjectRecordsTest, ReferenceMatchingIgnoresCaseWithoutReorderingRepeatedModifications) {
    const std::string source = "HMain  000000000003\nRExt   \nT00000003ABCDEF\n"
                               "M00000006+ext   \nM00000006-ext   \nM00000005+MAIN  \nE\n";
    const auto parsed = parseObject(source);
    const auto* module = std::get_if<ObjectModule>(&parsed);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(module->modifications.size(), 3U);
    const auto rendered = renderObject(*module);
    ASSERT_TRUE(std::holds_alternative<std::string>(rendered));
    EXPECT_EQ(std::get<std::string>(rendered), source);
}

TEST(ObjectRecordsTest, WriterRejectsInvalidHeaderDefinitionAndReferenceValues) {
    ObjectModule module;
    module.header = {"", 3, {"writer.obj", 1, 1}};
    expectError(renderObject(module), ObjectDiagnosticCode::InvalidName, {"writer.obj", 1, 2});
    module.header.name = "MAIN";
    module.header.length = 0x100001;
    expectError(renderObject(module), ObjectDiagnosticCode::ValueOutOfRange, {"writer.obj", 1, 14});
    module.header.length = 3;
    module.definitions = {{"DUP", 0, {"writer.obj", 2, 2}}, {"dup", 1, {"writer.obj", 3, 2}}};
    expectError(renderObject(module), ObjectDiagnosticCode::DuplicateName, {"writer.obj", 3, 2});
    module.definitions.resize(1);
    module.references = {{"DUP", {"writer.obj", 4, 2}}};
    expectError(renderObject(module), ObjectDiagnosticCode::DuplicateName, {"writer.obj", 4, 2});
    module.references[0].name = "_BAD";
    expectError(renderObject(module), ObjectDiagnosticCode::InvalidName, {"writer.obj", 4, 2});
    module.references[0].name = "TOOLONG";
    expectError(renderObject(module), ObjectDiagnosticCode::InvalidName, {"writer.obj", 4, 8});
}

TEST(ObjectRecordsTest, WriterRejectsEmptyOversizedAndOutOfBoundsText) {
    ObjectModule module;
    module.header = {"MAIN", 64, {"writer.obj", 1, 1}};
    module.text = {{0, {}, {"writer.obj", 2, 1}}};
    expectError(
        renderObject(module), ObjectDiagnosticCode::InvalidTextLength, {"writer.obj", 2, 8});
    module.text[0].bytes.assign(31, 0);
    expectError(
        renderObject(module), ObjectDiagnosticCode::InvalidTextLength, {"writer.obj", 2, 8});
    module.text[0].bytes = {0};
    module.text[0].offset = 0xFFFFFFFFU;
    expectError(renderObject(module), ObjectDiagnosticCode::ValueOutOfRange, {"writer.obj", 2, 2});
}

TEST(ObjectRecordsTest, WriterRejectsInvalidModificationAndEntryValues) {
    ObjectModule module;
    module.header = {"MAIN", 3, {"writer.obj", 1, 1}};
    module.text = {{0, {0xAB, 0xCD, 0xEF}, {"writer.obj", 2, 1}}};
    module.modifications = {{0, 5, ModificationSign::Add, "MAIN", {"writer.obj", 3, 1}}};
    module.entry = {0, {"writer.obj", 4, 1}};
    ASSERT_TRUE(std::holds_alternative<std::string>(renderObject(module)));
    auto invalid = module;
    invalid.modifications[0].halfBytes = 4;
    expectError(
        renderObject(invalid), ObjectDiagnosticCode::InvalidModification, {"writer.obj", 3, 8});
    invalid = module;
    invalid.modifications[0].sign = static_cast<ModificationSign>(99);
    expectError(
        renderObject(invalid), ObjectDiagnosticCode::InvalidModification, {"writer.obj", 3, 10});
    invalid = module;
    invalid.modifications[0].symbol = "EXT";
    expectError(
        renderObject(invalid), ObjectDiagnosticCode::UndeclaredReference, {"writer.obj", 3, 11});
    invalid.modifications[0].symbol = "BAD!";
    expectError(renderObject(invalid), ObjectDiagnosticCode::InvalidName, {"writer.obj", 3, 14});
    invalid = module;
    invalid.modifications[0].offset = 1;
    expectError(renderObject(invalid), ObjectDiagnosticCode::ValueOutOfRange, {"writer.obj", 3, 2});
    invalid = module;
    invalid.text.clear();
    expectError(renderObject(invalid), ObjectDiagnosticCode::MissingText, {"writer.obj", 3, 2});
    invalid = module;
    invalid.entry.offset = 3;
    expectError(renderObject(invalid), ObjectDiagnosticCode::ValueOutOfRange, {"writer.obj", 4, 2});
}

} // namespace
} // namespace linker
