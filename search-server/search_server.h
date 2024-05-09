#pragma once
#include "document.h"
#include "log_duration.h"
#include "string_processing.h"
#include <map>
#include <cmath>
#include <iterator>
#include <algorithm>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            using namespace std::string_literals;
            throw std::invalid_argument("Some of stop words are invalid"s);
        }
    }
    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))
    {
    }
    
    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
                     const std::vector<int>& ratings);
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                      DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
    
    int GetDocumentCount() const;
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query,
                                                        int document_id) const;
    
    std::vector<int>::const_iterator begin() const;
    std::vector<int>::const_iterator end() const;
    std::vector<int>::const_iterator GetDocIdByIndex (int x) const;//не могу перенести в приватную область класса, так как эта функция вызывается из MatchDocuments в файле search_server.cpp строка 126 const int document_id = *search_server.GetDocIdByIndex(index);
    
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(int document_id);
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    //при попытке переделать программу под тип set возникает следующая ошибка в файле search_server.cpp, строка 198 document_ids_.push_back(document_id) меняю на document_ids_.insert(document_id) и получаю вот это
    std::vector<int> document_ids_;//usr/include/c++/10/bits/stl_algo.h: In instantiation of ‘_ForwardIterator std::__remove_if(_ForwardIterator, _ForwardIterator, _Predicate) [with _ForwardIterator = std::_Rb_tree_const_iterator<int>; _Predicate = __gnu_cxx::__ops::_Iter_equals_val<const int>]’: /usr/include/c++/10/bits/stl_algo.h:880:30: required from ‘_FIter std::remove(_FIter, _FIter, const _Tp&) [with _FIter = std::_Rb_tree_const_iterator<int>; _Tp = int]’ /tmp/tmpesj1iaxm/search_server.cpp:185:92: required from here /usr/include/c++/10/bits/stl_algo.h:844:16: error: assignment of read-only location ‘__result.std::_Rb_tree_const_iterator<int>::operator*()’ 844 | *__result =
    
    //ну разумеется после переделывания всех
    //std::vector<int>::const_iterator begin() const;
    //std::vector<int>::const_iterator end() const;
    //std::vector<int>::const_iterator GetDocIdByIndex (int x) const;
    //под тип set
    
    bool IsStopWord(const std::string& word) const;
    static bool IsValidWord(const std::string& word);
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);
    
    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(const std::string& text) const;
    
    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };
    
    Query ParseQuery(const std::string& text) const;
    
    double ComputeWordInverseDocumentFreq(const std::string& word) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const;
};

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query,
                                                         DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);
    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
             return lhs.relevance > rhs.relevance
                 || (std::abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
                                           DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document,
                 DocumentStatus status, const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string& query);