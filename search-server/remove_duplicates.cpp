#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> remove_id;
    std::set<std::set<std::string>> set_of_setkeys;
    for (const int document_id : search_server)
    {
        std::set<std::string> res;
        auto word_frequencies=search_server.GetWordFrequencies(document_id);
        for(std::map<std::string, double>::const_iterator it = word_frequencies.begin(); it != word_frequencies.end(); ++it) {
            res.insert(it->first);
        }
        if(std::find(set_of_setkeys.begin(), set_of_setkeys.end(), res)!=set_of_setkeys.end())
        {
            remove_id.insert(document_id);
        }
        else
        {  
            set_of_setkeys.insert(res);
        }
    }
    std::vector<int> remove_idf(remove_id.begin(), remove_id.end());
    std::sort(remove_idf.begin(), remove_idf.end());
    for(const int doc: remove_idf)
    {
        std::cout<<"Found duplicate document id "<<doc<<std::endl;
        search_server.RemoveDocument(doc);
    }
}