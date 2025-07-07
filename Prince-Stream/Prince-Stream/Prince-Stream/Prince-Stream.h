#include <set>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <vector> // Added for std::vector<bool>
#include "Transaction.h"
#include <cstdint>

using namespace std;

// Represents a minimal generator with its support
struct MinGen {
    set<uint32_t> itemset; // The minimal generator (e.g., {A}, {C}, {BD})
    uint32_t support;      // Support count
};

// Memory pool for MinGen to reduce fragmentation
class MinGenPool {
private:
    vector<MinGen*> pool;
public:
    MinGen* acquire(const set<uint32_t>& itemset, uint32_t support) {
        if (!pool.empty()) {
            MinGen* gen = pool.back();
            pool.pop_back();
            gen->itemset = itemset;
            gen->support = support;
            return gen;
        }
        return new MinGen{itemset, support};
    }
    void release(MinGen* gen) {
        gen->itemset.clear();
        gen->support = 0;
        pool.push_back(gen);
    }
    ~MinGenPool() {
        for (auto gen : pool) delete gen;
    }
};

// Represents a closed itemset
struct ClosedIS {
    set<uint32_t> itemset; // The closed itemset
    uint32_t support;      // Support count
    set<MinGen*> gens;     // Set of minimal generators for this closure
};

// Represents a node in the MG-Lattice
struct MGLatticeNode {
    set<uint32_t> generator; // The minimal generator
    uint32_t support;        // Support of the generator
    ClosedIS* closure;       // Associated closed itemset
    vector<MGLatticeNode*> children; // Child nodes in the lattice
    MGLatticeNode* parent;   // Parent node
};

// TIDList for tracking transaction IDs per item using bitset
class TIDList {
public:
    map<uint32_t, vector<bool>*> TransactionList; // Item -> bitset of TIDs
    map<uint32_t, uint32_t> singletonSupport;     // Item -> support count
    uint32_t maxTid;                             // Track maximum TID for bitset size

    TIDList() : maxTid(0) {}

    void add(const set<uint32_t>& t, uint32_t n) {
        try {
            for (auto item : t) {
                if (!TransactionList[item]) {
                    TransactionList[item] = new vector<bool>(maxTid + 1, false);
                }
                if (n >= TransactionList[item]->size()) {
                    TransactionList[item]->resize(n + 1, false);
                }
                (*TransactionList[item])[n] = true;
                singletonSupport[item]++;
            }
            maxTid = max(maxTid, n + 1);
        } catch (const std::bad_alloc& e) {
            throw std::runtime_error("Memory allocation failed in TIDList::add");
        }
    }

    void remove(const set<uint32_t>& t, uint32_t n) {
        for (auto item : t) {
            if (TransactionList[item] && n < TransactionList[item]->size()) {
                (*TransactionList[item])[n] = false;
                singletonSupport[item]--;
                if (singletonSupport[item] == 0) {
                    delete TransactionList[item];
                    TransactionList.erase(item);
                    singletonSupport.erase(item);
                }
            }
        }
    }

    uint32_t supp_from_tidlist(const set<uint32_t>& itemset) {
        if (itemset.empty()) return maxTid;
        auto it = itemset.begin();
        vector<bool> result = *TransactionList[*it];
        for (++it; it != itemset.end(); ++it) {
            vector<bool> temp(result.size(), false);
            for (size_t i = 0; i < result.size() && i < TransactionList[*it]->size(); ++i) {
                temp[i] = result[i] && (*TransactionList[*it])[i];
            }
            result = temp;
        }
        return count(result.begin(), result.end(), true);
    }

    set<uint32_t> closure(const set<uint32_t>& itemset, uint32_t minSup) {
        set<uint32_t> result = itemset;
        uint32_t sup = supp_from_tidlist(itemset);
        for (auto& pair : TransactionList) {
            if (itemset.find(pair.first) == itemset.end()) {
                set<uint32_t> temp = itemset;
                temp.insert(pair.first);
                if (supp_from_tidlist(temp) == sup) {
                    result.insert(pair.first);
                }
            }
        }
        return result;
    }

    ~TIDList() {
        for (auto& pair : TransactionList) delete pair.second;
    }
};

// Main class for MG-Stream
class MGStream {
public:
    uint32_t minSup;
    uint32_t windowSize;
    float minConf;
    vector<set<uint32_t>*> TListByID; // Sliding window transactions
    TIDList* TList;                   // TID list for all transactions
    set<MinGen*> FMG;                 // Frequent minimal generators
    set<MinGen*> GBd;                 // Infrequent generators (sup = minSup - 1)
    vector<ClosedIS*> FCI;            // Frequent closed itemsets
    MGLatticeNode* latticeRoot;       // Root of MG-Lattice
    MinGenPool genPool;               // Memory pool for MinGen
    static const uint32_t MAX_ITEMSET_SIZE = 10; // Limit for subset generation

    MGStream(uint32_t ms, uint32_t ws, float mc) : minSup(ms), windowSize(ws), minConf(mc), TList(new TIDList), latticeRoot(nullptr) {
        try {
            TListByID.resize(windowSize, nullptr);
        } catch (const std::bad_alloc& e) {
            throw std::runtime_error("Memory allocation failed in MGStream constructor");
        }
    }

    ~MGStream() {
        for (auto gen : FMG) genPool.release(gen);
        for (auto gen : GBd) genPool.release(gen);
        for (auto ci : FCI) delete ci;
        delete TList;
        for (auto t : TListByID) delete t;
        clearLattice();
    }

    void addTransaction(const set<uint32_t>& t, uint32_t tid);
    void deleteTransaction(const set<uint32_t>& t, uint32_t tid);
    void buildMGLattice();
    void extractRules(const string& sgerFile, const string& sgarFile);
    void printLattice(const string& latticeFile);
    void clearLattice();
};