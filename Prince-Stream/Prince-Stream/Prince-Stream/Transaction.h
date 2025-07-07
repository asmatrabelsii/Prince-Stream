#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <vector>
#include <string>
#include <sstream>

template<typename T>
class Transaction {
public:
    std::vector<T> data; // Store items (replacing 'items')

    void load(const std::string& s, const char* delims, short withcrc = 0) {
        std::istringstream iss(s);
        std::string item;
        data.clear();
        while (std::getline(iss, item, delims[0])) {
            if (!item.empty()) {
                data.push_back(std::stoi(item));
            }
        }
    }
};

#endif