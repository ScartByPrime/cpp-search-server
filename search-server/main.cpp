#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }
    void SetDocumentCount(const int& count) {
        document_count_ = count;
    }
    void AddDocument(int document_id, const string& document) {
        const vector<string> document_parsed = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / document_parsed.size();
        for (const string& word : document_parsed){
            word_to_document_freqs_[word][document_id]+= inv_word_count;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const set<string> query_words = ParseQueryNoMinus(raw_query);
        const set<string> minus_words = SetMinusWords(raw_query);
        auto matched_documents = FindAllDocuments(query_words, minus_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    
    set<string> SetMinusWords (const string& query) const {
        set<string> minus_words;
        const vector<string> query_words = SplitIntoWordsNoStop(query);
        string source_word;
        for (const string& word : query_words) {
            if (word[0] == '-') {
              string source_word = word;
              source_word.erase(source_word.begin());
              minus_words.insert(source_word);
            }
        }
        return minus_words;
    }
    

private:
    map<string, map<int, double>> word_to_document_freqs_;

    set<string> stop_words_;
    
    int document_count_ = 0;

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (stop_words_.count(word) == 0) {
                words.push_back(word);
            }
        }
        return words;
    }

    set<string> ParseQueryNoMinus(const string& text) const {
        set<string> query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] != '-') {
                query_words.insert(word);
            }
        }
        return query_words;
    }
    
    vector<Document> FindAllDocuments(const set<string>& query_words, const set<string>& minus_words) const {
        map<int, double> document_relevance;
        for (const string& word : query_words) {
            double idf = ComputeWordIDF(word);
            if (word_to_document_freqs_.contains(word)) {
                for (const auto& [document_id, tf] : word_to_document_freqs_.at(word)) {
                document_relevance[document_id] += tf * idf;
            }
            } else {
                continue;
            }
        }
        
        for (const string& minus_word : minus_words) {
            if (word_to_document_freqs_.contains(minus_word)) {
            for (const auto &[document_id, term_freq] : word_to_document_freqs_.at(minus_word)) {
                document_relevance.erase(document_id);
            }
            } else {
                continue;
            }
        }
        vector<Document> matched_documents;
          for (const auto& [k, v] : document_relevance) {
            matched_documents.push_back({k, v});
          }
        return matched_documents;
    }
    
    double ComputeWordIDF(const string& word) const {
        if (word_to_document_freqs_.contains(word)) {
            return log(document_count_ * 1.0 / word_to_document_freqs_.at(word).size());
        } else {
            return 0.0;
        }
    } 
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());
    
    const int document_count = ReadLineWithNumber();
    search_server.SetDocumentCount(document_count);
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();
    
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}