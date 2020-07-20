#include <gtest/gtest.h>

#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

using CollectionTest = QLObjectTest;

TEST_F(CollectionTest, GetItemTest) {
  auto list = MakeListObj(MakeString("a"), MakeString("b"), MakeInt(1));
  auto subscript_or_s = list->GetSubscriptMethod();
  ASSERT_OK(subscript_or_s);
  auto subscript_fn = subscript_or_s.ConsumeValueOrDie();
  auto result_or_s = subscript_fn->Call({{}, {ToQLObject(MakeInt(1))}}, ast);
  ASSERT_OK(result_or_s);
  auto element = result_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(element->node(), String());
  auto str = static_cast<StringIR*>(element->node());
  EXPECT_EQ(str->str(), "b");
}

TEST_F(CollectionTest, OutOfRangeError) {
  auto list = MakeListObj(MakeString("a"), MakeString("b"), MakeInt(1));
  auto subscript_or_s = list->GetSubscriptMethod();
  ASSERT_OK(subscript_or_s);
  auto subscript_fn = subscript_or_s.ConsumeValueOrDie();
  auto result_or_s = subscript_fn->Call({{}, {ToQLObject(MakeInt(3))}}, ast);
  ASSERT_NOT_OK(result_or_s);
  EXPECT_THAT(result_or_s.status(), HasCompilerError("list index out of range"));
}

TEST_F(CollectionTest, WrongIndexTypeError) {
  auto list = MakeListObj(MakeString("a"), MakeString("b"), MakeInt(1));
  auto subscript_or_s = list->GetSubscriptMethod();
  ASSERT_OK(subscript_or_s);
  auto subscript_fn = subscript_or_s.ConsumeValueOrDie();
  auto result_or_s = subscript_fn->Call({{}, {ToQLObject(MakeString("blah"))}}, ast);
  ASSERT_NOT_OK(result_or_s);
  EXPECT_THAT(result_or_s.status(), HasCompilerError("list indices must be integers, not String"));
}

// Other tests make sure List works, tuple test just in case.
TEST_F(CollectionTest, TupleIndex) {
  auto list = MakeTupleObj(MakeString("a"), MakeString("b"), MakeInt(1));
  auto subscript_or_s = list->GetSubscriptMethod();
  ASSERT_OK(subscript_or_s);
  auto subscript_fn = subscript_or_s.ConsumeValueOrDie();
  auto result_or_s = subscript_fn->Call({{}, {ToQLObject(MakeInt(1))}}, ast);
  ASSERT_OK(result_or_s);
  auto element = result_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(element->node(), String());
  auto str = static_cast<StringIR*>(element->node());
  EXPECT_EQ(str->str(), "b");
}

// Test that Subscript Method can be saved even after List is gone.
TEST_F(CollectionTest, SaveSubscriptMethodToUseElsewhere) {
  // The object where we will save the get_item.
  std::shared_ptr<FuncObject> subscript_fn;
  {
    auto list = MakeListObj(MakeString("a"), MakeString("b"), MakeInt(1));
    auto subscript_or_s = list->GetSubscriptMethod();
    ASSERT_OK(subscript_or_s);
    subscript_fn = subscript_or_s.ConsumeValueOrDie();
    // Test 1, we can call inside the scope. (Sanity test).
    auto result_or_s = subscript_fn->Call({{}, {ToQLObject(MakeInt(1))}}, ast);
    ASSERT_OK(result_or_s);
    auto element = result_or_s.ConsumeValueOrDie();
    ASSERT_MATCH(element->node(), String());
    auto str = static_cast<StringIR*>(element->node());
    EXPECT_EQ(str->str(), "b");
  }
  // Test 2, we can call outside the scope and get the same result, even though the list is
  // deallocated.
  auto result_or_s = subscript_fn->Call({{}, {ToQLObject(MakeInt(1))}}, ast);
  ASSERT_OK(result_or_s);
  auto element = result_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(element->node(), String());
  auto str = static_cast<StringIR*>(element->node());
  EXPECT_EQ(str->str(), "b");
}

TEST_F(CollectionTest, ObjectAsCollectionWithCollection) {
  auto list = MakeTupleObj(MakeString("a"), MakeString("b"), MakeString("c"));
  std::vector<QLObjectPtr> objects = ObjectAsCollection(list);
  EXPECT_EQ(objects.size(), 3);
  std::vector<std::string> object_strings;
  for (const auto& o : objects) {
    object_strings.push_back(GetArgAs<StringIR>(o, "arg").ConsumeValueOrDie()->str());
  }
  EXPECT_THAT(object_strings, ElementsAre("a", "b", "c"));
}

TEST_F(CollectionTest, ObjectAsCollectionWithNonCollection) {
  std::vector<QLObjectPtr> objects = ObjectAsCollection(ToQLObject(MakeString("a")));
  EXPECT_EQ(objects.size(), 1);
  std::vector<std::string> object_strings;
  for (const auto& o : objects) {
    object_strings.push_back(GetArgAs<StringIR>(o, "arg").ConsumeValueOrDie()->str());
  }
  EXPECT_THAT(object_strings, ElementsAre("a"));
}
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
