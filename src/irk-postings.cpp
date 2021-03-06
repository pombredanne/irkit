// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! \file
//! \author     Michal Siedlaczek
//! \copyright  MIT License

#include <iostream>
#include <numeric>

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/te.hpp>
#include <irkit/algorithm/accumulate.hpp>
#include <irkit/algorithm/group_by.hpp>
#include <irkit/algorithm/query.hpp>
#include <irkit/coding/vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/parsing/stemmer.hpp>

#include "cli.hpp"
#include "run_query.hpp"

using boost::filesystem::path;
using irk::Inverted_Index_Mapped_Source;
using irk::inverted_index_view;
using irk::cli::optional;
using irk::index::term_id_t;

template<class PostingListT>
void print_postings(
    const PostingListT& postings,
    bool use_titles,
    const irk::inverted_index_view& index)
{
    for (const auto& posting : postings) {
        std::cout << posting.document() << "\t";
        if (use_titles) {
            std::cout << index.titles().key_at(posting.document()) << "\t";
        }
        std::cout << posting.payload() << "\n";
    }
}

template<typename Iter>
void print_postings_multiple(
    Iter first_posting_list,
    Iter last_posting_list,
    bool use_titles,
    const irk::inverted_index_view& index)
{
    auto postings = merge(first_posting_list, last_posting_list);
    using payload_type = decltype(postings.begin()->payload());
    group_by(
        postings.begin(),
        postings.end(),
        [](const auto& p) { return p.document(); })
        .aggregate_groups(
            [](const auto& acc, const auto& posting) {
                return acc + posting.payload();
            },
            payload_type(0))
        .for_each([use_titles, &index](const auto& id, const auto& payload) {
            std::cout << id << "\t";
            if (use_titles) {
                std::cout << index.titles().key_at(id) << "\t";
            }
            std::cout << payload << "\n";
        });
}

template<class Range>
int64_t
count_postings(const Range& terms, const irk::inverted_index_view& index)
{
    return std::accumulate(
        std::begin(terms),
        std::end(terms),
        int64_t(0),
        [&index](int64_t acc, const std::string& term) {
            return acc + index.postings(term).size();
        });
}

template<class Range, class Args>
void process_query(
    Range& terms,
    const irk::inverted_index_view& index,
    const Args& args,
    bool count)
{
    irk::cli::stem_if(not args.nostem, terms);
    if (count) {
        std::cout << count_postings(terms, index) << '\n';
    } else {
        if (args.score_function_defined()) {
            if (args.score_function[0] == '*') {
                auto postings = irk::cli::postings_on_fly(
                    terms, index, args.score_function);
                print_postings_multiple(
                    postings.begin(), postings.end(), false, index);
            } else {
                auto postings = query_scored_postings(index, terms);
                print_postings_multiple(
                    postings.begin(), postings.end(), false, index);
            }
        } else {
            auto postings = query_postings(index, terms);
            print_postings_multiple(
                postings.begin(), postings.end(), false, index);
        }
    }
}

struct Index_Like {
    void count_postings(gsl::span<std::string const> terms, std::ostream& out) const
    {
        boost::te::call([](auto const& self,
                           gsl::span<std::string const> terms,
                           auto& out) { self.count_postings(terms, out); },
                        *this,
                        terms,
                        out);
    }
};

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app(
        "Print information about term and its posting list",
        irk::cli::index_dir_opt{},
        irk::cli::nostem_opt{},
        irk::cli::score_function_opt{},
        irk::cli::id_range_opt{},
        irk::cli::terms_pos{optional});
    app->add_flag("-c,--count", "Count postings");
    CLI11_PARSE(*app, argc, argv);

    bool count = app->count("--count") == 1u;
    if (count) {
        //boost::te::poly<Index_Like> index = irk::Runnable_Index::from(args->index_dir);
        //if (not args->terms.empty()) {
        //    irk::cli::stem_if(not args->nostem, args->terms);
        //    index.count_postings(args->terms, std::cout);
        //} else {
        //    irk::for_each_query(std::cin, true, [&](int qid, auto terms) {
        //        index.count_postings(terms, std::cout);
        //    });
        //}
        return 0;
    } else {
        std::vector<std::string> scores;
        if (args->score_function_defined() && args->score_function[0] != '*') {
            scores.push_back(args->score_function);
        }
        auto data = irk::Inverted_Index_Mapped_Source::from(fs::path{args->index_dir}, scores);
        irk::inverted_index_view index(irtl::value(data));

        if (not args->terms.empty()) {
            process_query(args->terms, index, *args, count);
            return 0;
        }

        for (const std::string& query_line : irk::io::lines_from_stream(std::cin)) {
            std::vector<std::string> terms;
            boost::split(terms, query_line, boost::is_any_of("\t "), boost::token_compress_on);
            process_query(terms, index, *args, true);
        }
        return 0;
    }
}
