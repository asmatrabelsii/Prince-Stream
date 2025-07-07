#include "Prince-Stream.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <functional>

using namespace std;

// Helper: Generate subsets up to MAX_ITEMSET_SIZE
vector<set<uint32_t>> generateSubsets(const set<uint32_t>& itemset, uint32_t maxSize) {
    vector<set<uint32_t>> subsets;
    vector<uint32_t> items(itemset.begin(), itemset.end());
    uint32_t n = min(static_cast<uint32_t>(items.size()), maxSize);
    try {
        for (uint32_t i = 1; i < (1u << n); ++i) {
            set<uint32_t> subset;
            for (uint32_t j = 0; j < n; ++j) {
                if (i & (1u << j)) subset.insert(items[j]);
            }
            subsets.push_back(subset);
        }
    } catch (const std::bad_alloc& e) {
        throw std::runtime_error("Memory allocation failed in generateSubsets");
    }
    return subsets;
}

// Helper: Generate proper subsets up to MAX_ITEMSET_SIZE
vector<set<uint32_t>> generateProperSubsets(const set<uint32_t>& itemset, uint32_t maxSize) {
    vector<set<uint32_t>> subsets;
    vector<uint32_t> items(itemset.begin(), itemset.end());
    uint32_t n = min(static_cast<uint32_t>(items.size()), maxSize);
    try {
        for (uint32_t i = 1; i < (1u << n) - 1; ++i) {
            set<uint32_t> subset;
            for (uint32_t j = 0; j < n; ++j) {
                if (i & (1u << j)) subset.insert(items[j]);
            }
            subsets.push_back(subset);
        }
    } catch (const std::bad_alloc& e) {
        throw std::runtime_error("Memory allocation failed in generateProperSubsets");
    }
    return subsets;
}

// Helper: Check if itemset is minimal
bool isMinimalGenerator(const set<uint32_t>& itemset, uint32_t sup, TIDList* TList) {
    auto subsets = generateProperSubsets(itemset, MGStream::MAX_ITEMSET_SIZE);
    for (const auto& subset : subsets) {
        if (TList->supp_from_tidlist(subset) == sup) {
            return false;
        }
    }
    return true;
}

void MGStream::addTransaction(const set<uint32_t>& t, uint32_t tid) {
    try {
        TList->add(t, tid);
        vector<set<uint32_t>> candidates = generateSubsets(t, MAX_ITEMSET_SIZE);

        // Update existing generators
        for (auto gen : FMG) {
            if (includes(t.begin(), t.end(), gen->itemset.begin(), gen->itemset.end())) {
                gen->support++;
            }
        }
        for (auto gen : GBd) {
            if (includes(t.begin(), t.end(), gen->itemset.begin(), gen->itemset.end())) {
                gen->support++;
            }
        }

        // Process the empty set
        set<uint32_t> emptySet;
        uint32_t currentWindowSize = min(tid + 1, windowSize);
        bool emptyExists = false;
        for (auto gen : FMG) {
            if (gen->itemset == emptySet) {
                emptyExists = true;
                gen->support = TList->maxTid;;
                break;
            }
        }
        for (auto gen : GBd) {
            if (gen->itemset == emptySet) {
                emptyExists = true;
                gen->support = TList->maxTid;;
                break;
            }
        }
        if (!emptyExists && currentWindowSize >= minSup - 1 && isMinimalGenerator(emptySet, currentWindowSize, TList)) {
            MinGen* newGen = genPool.acquire(emptySet, currentWindowSize);
            if (currentWindowSize >= minSup) {
                FMG.insert(newGen);
            } else {
                GBd.insert(newGen);
            }
        }

        // Process other candidates
        for (const auto& cand : candidates) {
            uint32_t sup = TList->supp_from_tidlist(cand);
            bool exists = false;
            for (auto gen : FMG) {
                if (gen->itemset == cand) {
                    exists = true;
                    break;
                }
            }
            for (auto gen : GBd) {
                if (gen->itemset == cand) {
                    exists = true;
                    break;
                }
            }
            if (!exists && sup >= minSup - 1 && isMinimalGenerator(cand, sup, TList)) {
                MinGen* newGen = genPool.acquire(cand, sup);
                if (sup >= minSup) {
                    FMG.insert(newGen);
                } else {
                    GBd.insert(newGen);
                }
            }
        }

        // Move generators between FMG and GBd
        set<MinGen*> toRemoveFMG, toRemoveGBd;
        for (auto gen : FMG) {
            if (gen->support < minSup) {
                toRemoveFMG.insert(gen);
                if (gen->support == minSup - 1 && isMinimalGenerator(gen->itemset, gen->support, TList)) {
                    GBd.insert(gen);
                } else {
                    genPool.release(gen);
                }
            }
        }
        for (auto gen : GBd) {
            if (gen->support >= minSup && isMinimalGenerator(gen->itemset, gen->support, TList)) {
                toRemoveGBd.insert(gen);
                FMG.insert(gen);
            } else if (gen->support < minSup - 1) {
                toRemoveGBd.insert(gen);
                genPool.release(gen);
            }
        }
        for (auto gen : toRemoveFMG) FMG.erase(gen);
        for (auto gen : toRemoveGBd) GBd.erase(gen);
    } catch (const std::runtime_error& e) {
        throw std::runtime_error("Error in addTransaction: " + string(e.what()));
    }
}

void MGStream::deleteTransaction(const set<uint32_t>& t, uint32_t tid) {
    try {
        TList->remove(t, tid);
        vector<set<uint32_t>> candidates = generateSubsets(t, MAX_ITEMSET_SIZE);

        // Update supports
        for (auto gen : FMG) {
            if (gen->itemset.empty() || includes(t.begin(), t.end(), gen->itemset.begin(), gen->itemset.end())) {
                gen->support--;
            }
        }
        for (auto gen : GBd) {
            if (gen->itemset.empty() || includes(t.begin(), t.end(), gen->itemset.begin(), gen->itemset.end())) {
                gen->support--;
            }
        }

        // Move generators between FMG and GBd
        set<MinGen*> toRemoveFMG, toRemoveGBd;
        for (auto gen : FMG) {
            if (gen->support < minSup) {
                toRemoveFMG.insert(gen);
                if (gen->support == minSup - 1 && isMinimalGenerator(gen->itemset, gen->support, TList)) {
                    GBd.insert(gen);
                } else {
                    genPool.release(gen);
                }
            }
        }
        for (auto gen : GBd) {
            if (gen->support >= minSup && isMinimalGenerator(gen->itemset, gen->support, TList)) {
                toRemoveGBd.insert(gen);
                FMG.insert(gen);
            } else if (gen->support < minSup - 1) {
                toRemoveGBd.insert(gen);
                genPool.release(gen);
            }
        }
        for (auto gen : toRemoveFMG) FMG.erase(gen);
        for (auto gen : toRemoveGBd) GBd.erase(gen);
    } catch (const std::runtime_error& e) {
        throw std::runtime_error("Error in deleteTransaction: " + string(e.what()));
    }
}

void MGStream::buildMGLattice() {
    try {
        clearLattice();
        latticeRoot = new MGLatticeNode{{}, TList->maxTid, nullptr, {}, nullptr};

        // Sort FMG by decreasing support and increasing size
        vector<MinGen*> sortedFMG(FMG.begin(), FMG.end());
        sort(sortedFMG.begin(), sortedFMG.end(), [](MinGen* a, MinGen* b) {
            if (a->support != b->support) return a->support > b->support;
            return a->itemset.size() < b->itemset.size();
        });

        // Map to store equivalence class representatives
        map<set<uint32_t>, MGLatticeNode*> nodeMap;
        nodeMap[{}] = latticeRoot;

        // Helper function to find representative of an equivalence class
        auto findRepresentative = [&](const set<uint32_t>& itemset) -> MGLatticeNode* {
            for (auto& pair : nodeMap) {
                uint32_t unionSup = TList->supp_from_tidlist(itemset);
                if (pair.second->support == unionSup && TList->closure(pair.first, minSup) == TList->closure(itemset, minSup)) {
                    return pair.second;
                }
            }
            return nullptr;
        };

        // Helper function to manage equivalence class
        auto manageEquivClass = [&](MGLatticeNode* node, MGLatticeNode* rep) {
            for (auto& pair : nodeMap) {
                auto& succs = pair.second->children;
                for (auto it = succs.begin(); it != succs.end(); ) {
                    if (*it == node) {
                        it = succs.erase(it);
                        if (find(succs.begin(), succs.end(), rep) == succs.end()) {
                            succs.push_back(rep);
                        }
                    } else {
                        ++it;
                    }
                }
            }
            nodeMap[node->generator] = rep;
        };

        for (auto gen : sortedFMG) {
            // Generate (k-1)-subsets
            auto subsets = generateProperSubsets(gen->itemset, MGStream::MAX_ITEMSET_SIZE);
            MGLatticeNode* newNode = new MGLatticeNode{gen->itemset, gen->support, nullptr, {}, nullptr};
            nodeMap[gen->itemset] = newNode;

            for (const auto& subset : subsets) {
                auto rep = findRepresentative(subset);
                if (!rep) continue;

                auto& succs = rep->children;
                bool added = false;

                for (auto it = succs.begin(); it != succs.end(); ) {
                    set<uint32_t> unionSet;
                    set_union(gen->itemset.begin(), gen->itemset.end(),
                              (*it)->generator.begin(), (*it)->generator.end(),
                              inserter(unionSet, unionSet.begin()));
                    uint32_t unionSup = TList->supp_from_tidlist(unionSet);

                    if (gen->support == (*it)->support && gen->support == unionSup) {
                        // Same equivalence class
                        manageEquivClass(newNode, *it);
                        delete newNode;
                        newNode = *it;
                        added = true;
                        break;
                    } else if (gen->support < (*it)->support && gen->support == unionSup) {
                        // newNode is a successor of *it's equivalence class
                        if (find(succs.begin(), succs.end(), newNode) == succs.end()) {
                            succs.push_back(newNode);
                            newNode->parent = rep;
                            added = true;
                        }
                        ++it;
                    } else {
                        ++it;
                    }
                }

                if (!added && find(succs.begin(), succs.end(), newNode) == succs.end()) {
                    bool incomparable = true;
                    for (auto succ : succs) {
                        set<uint32_t> unionSet;
                        set_union(gen->itemset.begin(), gen->itemset.end(),
                                  succ->generator.begin(), succ->generator.end(),
                                  inserter(unionSet, unionSet.begin()));
                        if (TList->supp_from_tidlist(unionSet) != gen->support ||
                            succ->support <= gen->support) {
                            incomparable = false;
                            break;
                        }
                    }
                    if (incomparable) {
                        succs.push_back(newNode);
                        newNode->parent = rep;
                        added = true;
                    }
                }
            }

            if (!newNode->parent && !gen->itemset.empty()) {
                latticeRoot->children.push_back(newNode);
                newNode->parent = latticeRoot;
            }

            // Compute closure and update FCI
            set<uint32_t> closure = TList->closure(gen->itemset, minSup);
            ClosedIS* ci = nullptr;
            for (auto existingCI : FCI) {
                if (existingCI->itemset == closure) {
                    ci = existingCI;
                    break;
                }
            }
            if (!ci) {
                ci = new ClosedIS{closure, gen->support, {}};
                FCI.push_back(ci);
            }
            ci->gens.insert(gen);
            nodeMap[gen->itemset]->closure = ci;
        }
    } catch (const std::bad_alloc& e) {
        throw std::runtime_error("Memory allocation failed in buildMGLattice");
    }
}

void MGStream::extractRules(const string& sgerFile, const string& sgarFile) {
    try {
        ofstream sgerOut(sgerFile);
        if (!sgerOut.is_open()) {
            throw std::runtime_error("Could not open SGER file for writing: " + sgerFile);
        }
        ofstream sgarOut(sgarFile);
        if (!sgarOut.is_open()) {
            sgerOut.close();
            throw std::runtime_error("Could not open SGAR file for writing: " + sgarFile);
        }

        auto isDirectCover = [&](const set<uint32_t>& g_closure, const set<uint32_t>& f) -> bool {
            if (!includes(f.begin(), f.end(), g_closure.begin(), g_closure.end()) || g_closure == f) {
                return false;
            }
            for (auto h : FCI) {
                if (h->itemset == f || h->itemset == g_closure) continue;
                if (includes(h->itemset.begin(), h->itemset.end(), g_closure.begin(), g_closure.end()) &&
                    includes(f.begin(), f.end(), h->itemset.begin(), h->itemset.end())) {
                    return false;
                }
            }
            return true;
        };

        for (auto gen : FMG) {
            set<uint32_t> g_closure = TList->closure(gen->itemset, minSup);
            for (auto ci : FCI) {
                if (ci->support == 0 || gen->support == 0) continue; // Skip invalid entries

                set<uint32_t> consequent;
                set_difference(ci->itemset.begin(), ci->itemset.end(),
                               gen->itemset.begin(), gen->itemset.end(),
                               inserter(consequent, consequent.begin()));
                if (consequent.empty()) continue;

                float conf = static_cast<float>(ci->support) / gen->support;
                if (conf < 0 || conf > 1) continue; // Skip invalid confidence values

                string rule = "{";
                for (auto it = gen->itemset.begin(); it != gen->itemset.end(); ++it) {
                    rule += to_string(*it) + (next(it) != gen->itemset.end() ? "," : "");
                }
                rule += "} => {";
                for (auto it = consequent.begin(); it != consequent.end(); ++it) {
                    rule += to_string(*it) + (next(it) != consequent.end() ? "," : "");
                }
                rule += "} [conf=" + to_string(conf) + ", supp=" + to_string(ci->support) + "]";

                if (g_closure == ci->itemset) {
                    sgerOut << rule << endl;
                } else if (isDirectCover(g_closure, ci->itemset) && conf >= minConf) {
                    sgarOut << rule << endl;
                }
            }
        }

        sgerOut.close();
        sgarOut.close();
    } catch (const std::bad_alloc& e) {
        throw std::runtime_error("Memory allocation failed in extractRules");
    } catch (const std::runtime_error& e) {
        throw std::runtime_error("Error in extractRules: " + string(e.what()));
    }
}

void MGStream::printLattice(const string& latticeFile) {
    try {
        ofstream outFile(latticeFile);
        if (!outFile.is_open()) {
            throw std::runtime_error("Could not open lattice file for writing: " + latticeFile);
        }

        function<void(MGLatticeNode*, int, ofstream&)> printNode = [&](MGLatticeNode* node, int level, ofstream& out) {
            // Print indentation
            for (int i = 0; i < level; ++i) out << "  ";

            // Print node details
            out << "Node: {";
            for (auto it = node->generator.begin(); it != node->generator.end(); ++it) {
                out << *it << (next(it) != node->generator.end() ? "," : "");
            }
            out << "}, Support: " << node->support;
            if (node->closure) {
                out << ", Closure: {";
                for (auto it = node->closure->itemset.begin(); it != node->closure->itemset.end(); ++it) {
                    out << *it << (next(it) != node->closure->itemset.end() ? "," : "");
                }
                out << "}";
            }
            out << endl;

            // Print children
            for (auto child : node->children) {
                printNode(child, level + 1, out);
            }
        };

        outFile << "Minimal Generator Lattice:\n";
        printNode(latticeRoot, 0, outFile);
        outFile.close();
    } catch (const std::runtime_error& e) {
        throw std::runtime_error("Error in printLattice: " + string(e.what()));
    }
}

void MGStream::clearLattice() {
    if (!latticeRoot) return;
    function<void(MGLatticeNode*)> deleteNode = [&](MGLatticeNode* node) {
        for (auto child : node->children) {
            deleteNode(child);
        }
        delete node;
    };
    deleteNode(latticeRoot);
    latticeRoot = nullptr;
}