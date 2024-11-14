#pragma once

#include <iostream>
#include <iterator>
#include <vector>

using namespace std;

template <typename It>
class IteratorRange {
public:
    IteratorRange(It begin, It end)
        : first_(begin), last_(end) {}

    It begin() const {
        return first_;
    }

    It end() const {
        return last_;
    }

    size_t size() const {
        return distance(first_, last_);
    }

private:
    It first_;
    It last_;
};

   
template <typename It>
class Paginator {
public:
    explicit Paginator(It begin, It end, int page_size) {
        while (begin != end) {
            It page_end = (distance(begin, end) > page_size) ? next(begin, page_size) : end;
            pages_.push_back({begin, page_end});
            begin = page_end;
        }
    }
    typename vector<IteratorRange<It>>::const_iterator begin() const {
        return pages_.begin();
    }

    typename vector<IteratorRange<It>>::const_iterator end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    vector<IteratorRange<It>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename It>
ostream& operator<<(ostream& out, const IteratorRange<It>& range) {
    for (auto it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}