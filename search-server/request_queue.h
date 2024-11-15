#pragma once

#include "search_server.h"
#include "document.h"

#include <stack>
#include <vector>

using namespace std;

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate);
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status);

    vector<Document> AddFindRequest(const string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        string raw_query;
        vector<Document> result;
        bool is_relevant;
    };
    const SearchServer& search_server_;
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int no_result = 0;
};

template <typename DocumentPredicate>
vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
    const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
    bool is_relevant;
    if (requests_.size() >= min_in_day_) {
        if (requests_.front().is_relevant == false) {
            -- no_result;
        }
        requests_.pop_front();
        }
    if (result.empty()) {
        is_relevant = false;
        requests_.push_back({raw_query, result, is_relevant});
        ++no_result;
    } else {
        is_relevant = true;
        requests_.push_back({raw_query, result, is_relevant});
    }
return result;
}