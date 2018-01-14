#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/index.hpp"
#include "irkit/coding.hpp"

namespace {

using Posting = irkit::_Posting<std::uint16_t, double>;
using IndexT = irkit::Index<std::uint16_t, std::string, std::uint16_t, std::uint16_t>;

struct FakeScore {
    template<class Freq>
    double operator()(Freq tf, Freq df, std::size_t collection_size) const
    {
        return tf;
    }
};

std::vector<char> flatten(std::vector<std::vector<char>> vectors)
{
    std::vector<char> result;
    for (std::vector<char>& vec : vectors) {
        result.insert(result.end(), vec.begin(), vec.end());
    }
    return result;
}

irkit::VarByte<std::uint16_t> vb;

class IndexReading : public ::testing::Test {
protected:
    irkit::Index<std::uint16_t, std::string, std::uint16_t, std::uint16_t>
        index;
    IndexReading()
        : index({"b", "c", "z"},  // term_map_
              {2, 1, 1},  // term_dfs_
              flatten({vb.encode({0, 1}),
                  vb.encode({1}),
                  vb.encode({0})}),  // doc_ids_
              {0, 2, 3},  // doc_ids_off_
              flatten({vb.encode({1, 2}),
                  vb.encode({1}),
                  vb.encode({2})}),  // doc_counts_
              {0, 2, 3},  // doc_counts_off_
              {"Doc1", "Doc2", "Doc3"})
    {
    }
};


TEST_F(IndexReading, offsets)
{
    EXPECT_EQ(index.offset("b", index.doc_ids_off_), 0);
    EXPECT_EQ(index.offset(0, index.doc_ids_off_), 0);
    EXPECT_EQ(index.offset("c", index.doc_ids_off_), 2);
    EXPECT_EQ(index.offset(1, index.doc_ids_off_), 2);
    EXPECT_EQ(index.offset("z", index.doc_ids_off_), 3);
    EXPECT_EQ(index.offset(2, index.doc_ids_off_), 3);
    EXPECT_EQ(index.offset("b", index.doc_counts_off_), 0);
    EXPECT_EQ(index.offset(0, index.doc_counts_off_), 0);
    EXPECT_EQ(index.offset("c", index.doc_counts_off_), 2);
    EXPECT_EQ(index.offset(1, index.doc_counts_off_), 2);
    EXPECT_EQ(index.offset("z", index.doc_counts_off_), 3);
    EXPECT_EQ(index.offset(2, index.doc_counts_off_), 3);
}

TEST_F(IndexReading, posting_range)
{
    auto bystring = index.posting_range("b", FakeScore{});
    auto byid = index.posting_range(0, FakeScore{});
    std::vector<Posting> bystring_actual;
    std::vector<Posting> byid_actual;
    std::vector<Posting> expected = {{0, 1.0}, {1, 2.0}};
    for (const auto& posting : bystring) {
        bystring_actual.push_back(posting);
    }
    for (const auto& posting : byid) {
        byid_actual.push_back(posting);
    }
    EXPECT_THAT(bystring_actual, ::testing::ElementsAreArray(expected));
    EXPECT_THAT(byid_actual, ::testing::ElementsAreArray(expected));
}


class IndexLoading : public ::testing::Test {
protected:
    irkit::fs::path index_dir;
    std::unique_ptr<IndexT> index;
    IndexLoading()
    {
        index_dir = irkit::fs::temp_directory_path() / "IndexLoadingTest";
        if (irkit::fs::exists(index_dir)) {
            irkit::fs::remove_all(index_dir);
        }
        irkit::fs::create_directory(index_dir);
        write_bytes(irkit::index::terms_path(index_dir),
            {'b', '\n', 'c', '\n', 'z', '\n'});
        write_bytes(
            irkit::index::term_doc_freq_path(index_dir), vb.encode({2, 1, 1}));
        write_bytes(
            irkit::index::doc_ids_off_path(index_dir), vb.encode({0, 2, 1}));
        write_bytes(irkit::index::doc_ids_path(index_dir),
            flatten({vb.encode({0, 1}), vb.encode({1}), vb.encode({0})}));
        write_bytes(
            irkit::index::doc_counts_off_path(index_dir), vb.encode({0, 2, 1}));
        write_bytes(irkit::index::doc_counts_path(index_dir),
            flatten({vb.encode({1, 2}), vb.encode({1}), vb.encode({2})}));
        std::string titles = "Doc1\nDoc2\nDoc3\n";
        std::vector<char> titles_array(titles.begin(), titles.end());
        write_bytes(irkit::index::titles_path(index_dir), titles_array);
        index.reset(new IndexT(index_dir));
    }
    ~IndexLoading() { irkit::fs::remove_all(index_dir); }
    void write_bytes(irkit::fs::path file, const std::vector<char>& bytes)
    {
        std::ofstream ofs(file);
        ofs.write(bytes.data(), bytes.size());
        ofs.close();
    }
};

TEST_F(IndexLoading, load)
{
    EXPECT_EQ(index->collection_size(), 3);

    std::string e_terms = "bcz";
    std::string actual_terms;
    for (const auto& term : index->terms_) {
        actual_terms.append(*term);
    }
    EXPECT_EQ(actual_terms, e_terms);

    for (const auto& entry : index->term_map_) {
        std::cout << *entry.first << std::endl;
    }

    std::vector<std::pair<std::string, std::uint16_t>> e_term_map = {
        {"b", 0}, {"c", 1}, {"z", 2}};
    std::vector<std::pair<std::string, std::uint16_t>> a_term_map;
    for (const auto& term : index->terms_) {
        a_term_map.push_back({*term, index->term_map_[term.get()]});
    }
    std::sort(a_term_map.begin(), a_term_map.end());
    EXPECT_THAT(a_term_map, ::testing::ElementsAreArray(e_term_map));

    std::vector<std::uint16_t> e_term_dfs_ = {2, 1, 1};
    EXPECT_THAT(index->term_dfs_, ::testing::ElementsAreArray(e_term_dfs_));

    std::vector<char> e_doc_ids =
        flatten({vb.encode({0, 1}), vb.encode({1}), vb.encode({0})});
    EXPECT_THAT(index->doc_ids_, ::testing::ElementsAreArray(e_doc_ids));

    std::vector<char> e_doc_counts =
        flatten({vb.encode({1, 2}), vb.encode({1}), vb.encode({2})});
    EXPECT_THAT(index->doc_counts_, ::testing::ElementsAreArray(e_doc_counts));

    std::vector<std::uint16_t> e_doc_ids_off = {0, 2, 3};
    EXPECT_THAT(
        index->doc_ids_off_, ::testing::ElementsAreArray(e_doc_ids_off));

    std::vector<std::uint16_t> e_doc_counts_off = {0, 2, 3};
    EXPECT_THAT(
        index->doc_counts_off_, ::testing::ElementsAreArray(e_doc_counts_off));
}

TEST_F(IndexLoading, offset)
{
    EXPECT_EQ(index->offset("b", index->doc_ids_off_), 0);
    EXPECT_EQ(index->offset(0, index->doc_ids_off_), 0);
    EXPECT_EQ(index->offset("c", index->doc_ids_off_), 2);
    EXPECT_EQ(index->offset(1, index->doc_ids_off_), 2);
    EXPECT_EQ(index->offset("z", index->doc_ids_off_), 3);
    EXPECT_EQ(index->offset(2, index->doc_ids_off_), 3);
    EXPECT_EQ(index->offset("b", index->doc_counts_off_), 0);
    EXPECT_EQ(index->offset(0, index->doc_counts_off_), 0);
    EXPECT_EQ(index->offset("c", index->doc_counts_off_), 2);
    EXPECT_EQ(index->offset(1, index->doc_counts_off_), 2);
    EXPECT_EQ(index->offset("z", index->doc_counts_off_), 3);
    EXPECT_EQ(index->offset(2, index->doc_counts_off_), 3);
}

//TEST_F(IndexLoading, posting_ranges)
//{
//    auto bystring = index->posting_range("b", FakeScore{});
//    auto byid = index->posting_range(0, FakeScore{});
//    std::vector<Posting> bystring_actual;
//    std::vector<Posting> byid_actual;
//    std::vector<Posting> expected = {{0, 1.0}, {1, 2.0}};
//    for (const auto& posting : bystring) {
//        bystring_actual.push_back(posting);
//    }
//    for (const auto& posting : byid) {
//        byid_actual.push_back(posting);
//    }
//    EXPECT_THAT(bystring_actual, ::testing::ElementsAreArray(expected));
//    EXPECT_THAT(byid_actual, ::testing::ElementsAreArray(expected));
//}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

