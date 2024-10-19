#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
            for (const string& word : stop_words_){
                if (!IsValidWord(word)) {
                    throw invalid_argument("Одно или несколько стоп-слов содержат недопустимые символы"s);
                }
            }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("Попытка добавить документ с отрицательным индексом"s);
        } else if (documents_.count(document_id)) {
            throw invalid_argument("Индекс добавляемого документа уже присутствует в системе"s);
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        document_ids_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const  {
        Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) const {
        if (index >= 0 && index < document_ids_.size()) {
            return document_ids_.at(index);
        }
        throw out_of_range("Индекс документа выходит за пределы допустимого диапазона"s);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return make_tuple(matched_words, documents_.at(document_id).status);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    vector<int> document_ids_;
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Одно или несколько слов добавляемого документа содержат недопустимые символы"s);
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

   QueryWord ParseQueryWord(string text) const {
        if (!IsValidWord(text)) {
            throw invalid_argument("Одно или несколько слов поискового запроса содержат недопустимые символы"s);
        }
        bool is_minus = false;
        // double minus and space after minus check
        if ((text[0] == '-' && text[1] == '-') || text == "-") {
            throw invalid_argument("Одно или несколько минус-слов поискового запроса имеет недопустимый формат"s);
        } else if (text[0] == '-') {  
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
        QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto &[document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto &[document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto &[document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }

    static bool IsValidWord(const string& word) {
    // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

// FRAMEWORK BEGIN

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TestFunc>
void RunTestImpl(const TestFunc& func, const string& test_name) {
    func();
    cerr << test_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

// FRAMEWORK END

// UNIT TESTS BEGIN

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer search_server1("  in  about the   "s);
        search_server1.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(search_server1.FindTopDocuments("the"s).empty(),
                    "Stop words must be excluded from documents"s);
    }

    {
        const set<string> stop_words_set = {"in"s, "about"s, "the"s};
        SearchServer search_server2(stop_words_set);
        search_server2.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(search_server2.FindTopDocuments("the"s).empty(),
                    "Stop words must be excluded from documents"s);
    }

    {
        const vector<string> stop_words_vector = {"in"s, "about"s, "the"s, ""s, "about"s};
        SearchServer search_server3(stop_words_vector);
        search_server3.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(search_server3.FindTopDocuments("the"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestExcludeMinusWordsDocumentsFromResult() {
    SearchServer search_server("  и  в на   "s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный кот выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный кот евгений"s,             DocumentStatus::BANNED, {9});
    const auto found_docs = search_server.FindTopDocuments("пушистый -ухоженный кот"s);
    const vector<int> correct_indexes = {1, 0};
    vector<int> found_indexes;
    for (const auto& [id, sec, thir] : found_docs) {
        found_indexes.push_back(id);
    }
    ASSERT_HINT(equal(correct_indexes.begin(), correct_indexes.end(), found_indexes.begin(), found_indexes.end()), "Minus words exclusion failure"s);
    const auto found_docs_with_minus = search_server.MatchDocument("кот -белый"s, 0);
    ASSERT_EQUAL_HINT(get<vector<string>>(found_docs_with_minus).size(), 0, "Matched minus words do not reset plus words"s);
}

void TestRelevantWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const vector<string> relevant_words = {"cat"s, "city"s};
    {
        SearchServer server("in, the"s);
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);

        const auto found_docs = server.MatchDocument("cat city"s, 42);
        const vector<string>& found_relevant_words = get<vector<string>>(found_docs);
        ASSERT_HINT(equal(relevant_words.begin(), relevant_words.end(), found_relevant_words.begin(), found_relevant_words.end()), "Incorrect relevant words found"s);
    }
}

void TestDocumentRating() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {5, -12, 2, 1};
    {
        SearchServer server("in, the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat city"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, -1, "Incorrect rating calculation"s);
    }
}

void TestPredicateInclusion() {
    SearchServer search_server("  и  в на   "s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    const auto found_docs_id = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus, int) { return document_id % 2 == 0; });
    vector<int> correct_indexes = {0, 2};
    vector<int> found_indexes;
    for (const auto& [id, sec, thir] : found_docs_id) {
        found_indexes.push_back(id);
    }
    ASSERT_HINT(equal(correct_indexes.begin(), correct_indexes.end(), found_indexes.begin(), found_indexes.end()), "Predicate inclusion failure"s);

    const auto found_docs_status = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int, DocumentStatus status, int) { return status == DocumentStatus::BANNED; });
    correct_indexes.clear();
    correct_indexes = {3};
    found_indexes.clear();
    for (const auto& [id, sec, thir] : found_docs_status) {
        found_indexes.push_back(id);
    }
    ASSERT_HINT(equal(correct_indexes.begin(), correct_indexes.end(), found_indexes.begin(), found_indexes.end()), "Predicate inclusion failure"s);
}

bool AreDoublesEqual(double a, double b) {
    return std::fabs(a - b) < EPSILON;
}

void TestStatusAndRelevanceCorrespondence() {
    SearchServer search_server("  и  в на   "s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    const vector<int> correct_indexes = {3, 2};
    const vector<double> correct_relevances = {0.231049, 0.173287};
    vector<int> found_indexes;
    vector<double> found_relevances;
    for (const auto& [id, relevance, thir] : found_docs) {
        found_indexes.push_back(id);
        found_relevances.push_back(relevance);
    }
    ASSERT_HINT(equal(correct_indexes.begin(), correct_indexes.end(), found_indexes.begin(), found_indexes.end()), "Status search failure"s);
    ASSERT_HINT(equal(correct_relevances.begin(), correct_relevances.end(), found_relevances.begin(), found_relevances.end(), AreDoublesEqual), "Incorrect documents' relevance"s);
    const auto found_docs_via_matchdocument = search_server.MatchDocument("скворец евгений"s, 3);
    ASSERT_HINT(get<DocumentStatus>(found_docs_via_matchdocument) == DocumentStatus::BANNED, "Incorrect status return"s);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsDocumentsFromResult);
    RUN_TEST(TestRelevantWords);
    RUN_TEST(TestDocumentRating);
    RUN_TEST(TestPredicateInclusion);
    RUN_TEST(TestStatusAndRelevanceCorrespondence);
    cout << "Tests result: revealed no errors"s << endl;
}

// UNIT TESTS END

int main() {
    TestSearchServer();
    return 0;
}