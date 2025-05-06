#include "Prince-Stream.h"
#include "Transaction.h"
#include <iostream>
#include <set>
#include <map>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <iomanip>

uint32_t NODE_ID = 0;
uint32_t minSupp = 3;
uint32_t totalGens = 0;

//const uint32_t windowSize = 1000;

std::set<uint32_t>** TListByID;

int testedJp = 0;
float sumJp = 0;
float actgen = 0;
bool extratext = false;


void Addition(std::set<uint32_t> t_n, int n, GenNode* root, TIDList* TList, std::multimap<uint32_t, ClosedIS*>* ClosureList) {

    TList->add(t_n, n);
    std::set<uint32_t> emptySet;
    std::multimap<uint32_t, ClosedIS*> fGenitors;
    std::vector<ClosedIS*>* newClosures = new std::vector<ClosedIS*>;


    descend(root, emptySet, t_n, &fGenitors, ClosureList, newClosures, TList, root);
    if (extratext) {
        std::cout << "filterCandidates" << std::endl;
    }
    filterCandidates(&fGenitors, root, ClosureList);
    if (extratext) {
        std::cout << "computeJumpers" << std::endl;
    }
    //computeJumpers(root, t_n, newClosures, TList, root, ClosureList);
    if (extratext) {
        std::cout << "endloop " << newClosures->size() << std::endl;
    }
    for (std::vector<ClosedIS*>::iterator jClos = newClosures->begin(); jClos != newClosures->end(); ++jClos) {
        if (extratext) {
            std::cout << " -------- " << (*jClos)->itemset.size() << " ------ " << t_n.size() << std::endl;
        }

        std::set<std::set<uint32_t>*> preds = compute_preds_exp(*jClos);
        //std::set<std::set<uint32_t>*> preds = std::set<std::set<uint32_t>*>();
        //compute_generators_v2(&preds, *jClos);

        uint32_t key = CISSum((*jClos)->itemset);
        if (extratext) {
            std::cout << "|preds|=" << preds.size() << std::endl;
        }
        //for (std::vector<std::set<uint32_t>*>::iterator pred = preds.begin(); pred != preds.end(); ++pred) {
        for (std::set<std::set<uint32_t>*>::iterator pred = preds.begin(); pred != preds.end(); ++pred) {
            // pour chaque predecesseur, on le retrouve via son intent
            ClosedIS* predNode = findCI(**pred, ClosureList);
            if (predNode) {

              /*if (predNode->deleted) {
                std::cout << "...." << std::endl;
              }*/

              // puis on link dans les deux sens
              predNode->succ->insert(std::make_pair(key, *jClos));
              (*jClos)->preds->insert(std::make_pair(CISSum(**pred), predNode));
            }
            else {
              std::cout << "oh pred is null..." << std::endl;
            }
            delete *pred;
        }
    }
    if (extratext) {
        std::cout << "reseting" << std::endl;
    }
    //std::cout << testedJp << " jumpers tested.\n";
    closureReset(ClosureList); // This is needed to set all visited flags back to false and clear the candidate list

    delete newClosures;
}


void Deletion(std::set<uint32_t> t_0, int n, GenNode* root, TIDList* TList, std::multimap<uint32_t, ClosedIS*>* ClosureList) {
    TList->remove(t_0, n);
    std::vector<ClosedIS*>* iJumpers = new std::vector<ClosedIS*>;
    std::multimap<uint32_t, ClosedIS*>* fObsoletes = new std::multimap<uint32_t, ClosedIS*>;

    descendM(root, t_0, ClosureList, iJumpers, fObsoletes);

    for (std::vector<ClosedIS*>::iterator jprIt = iJumpers->begin(); jprIt != iJumpers->end(); ++jprIt) {
        ClosedIS* jumper = *jprIt;
        dropJumper(jumper, ClosureList);
    }

    for (std::multimap<uint32_t, ClosedIS*>::reverse_iterator obsIt = fObsoletes->rbegin(); obsIt != fObsoletes->rend(); ++obsIt) {
        ClosedIS* obsolete = obsIt->second;
        dropObsolete(obsolete, ClosureList, root);
    }

    closureReset(ClosureList);

    delete iJumpers;
    delete fObsoletes;
}



// Helper functions
void printAllGens(GenNode* node, std::ostream& _output) {
  //totalGens = 0;
  if (node->succ) {
    for (auto child : *node->succ) {
      printAllGens(child.second, _output);
    }
  }
  _output << node->clos->support << " ";
  std::set<uint32_t> itemset = node->items();
  for (auto item : itemset) {
    _output << item << " ";
  }
  //totalGens++;
  _output << "\n";
}

void releaseAllGens(GenNode* node) {
  //totalGens = 0;
  if (node->succ) {
    for (auto child : *node->succ) {
      releaseAllGens(child.second);
    }
  }
  node->succ->clear();
  delete node->succ;
  delete node;
}


void printAllClosuresWithGens(std::multimap<uint32_t, ClosedIS*> ClosureList) {
    totalGens = 0;
    for (auto x : ClosureList) {
        ClosedIS currCI = *x.second;
        std::cout << "Closed itemset { ";
        for (auto item : currCI.itemset) {
          std::cout << item << " ";
        }
        std::cout << "} (" << currCI.support << ") has generators : ";
        for (auto gen : currCI.gens) {
            totalGens++;
            std::cout << "{ ";
            for (auto item : gen->items()) {
              std::cout << item << " ";
            }
            std::cout << "} ";
        }
        std::cout << "\n";

    }
}


void printAllClosuresWithGensTM(std::multimap<uint32_t, ClosedIS*> ClosureList, std::ostream& f) {
  //totalGens = 0;
  for (auto x : ClosureList) {
    ClosedIS currCI = *x.second;
    f << "s=" << currCI.support << " fermeture : ";
    for (auto item : currCI.itemset) {
      f << item << " ";
    }
    f << "generateurs : ";
    uint32_t cursor = 0;
    for (auto gen : currCI.gens) {
      if (cursor != 0) {
        f << "  ";
      }
      for (auto item : gen->items()) {
        f << item << " ";
      }
      cursor += 1;
    }
    f << "\n";
  }
}

void printClosureOrderTM(std::multimap<uint32_t, ClosedIS*> ClosureList, std::ostream& f) {
  for (std::multimap<uint32_t, ClosedIS*>::iterator clos = ClosureList.begin(); clos != ClosureList.end(); ++clos) {
    ClosedIS currCI = *clos->second;
    uint32_t cursor = 0;
    f << "#" << currCI.id << "[";
    for (auto item : currCI.itemset) {
      if (cursor != 0) {
        f << ", ";
      }
      f << item;
      cursor += 1;
    }
    f << "]|s=" << currCI.support << " => {";
    cursor = 0;
    for (std::multimap<uint32_t, ClosedIS*>::iterator child = currCI.preds->begin(); child != currCI.preds->end(); ++child) {
      ClosedIS currChild = *child->second;
      if (cursor != 0) {
        f << ", ";
      }
      f << "#" << currChild.id;
      cursor += 1;
    }
    f << "}\n";
  }
}

void printClosureOrder(std::multimap<uint32_t, ClosedIS*> ClosureList) {
    for (std::multimap<uint32_t, ClosedIS*>::iterator clos = ClosureList.begin(); clos != ClosureList.end(); ++clos) {
        ClosedIS currCI = *clos->second;
        std::cout << "Closed itemset { ";
        for (auto item : currCI.itemset) {
            std::cout << item << " ";
        }
        std::cout << "} (" << currCI.support << ") has children : ";
        for (std::multimap<uint32_t, ClosedIS*>::iterator child = currCI.succ->begin(); child != currCI.succ->end(); ++child) {
            ClosedIS currChild = *child->second;
            std::cout << "{";
            for (auto item : currChild.itemset) {
                std::cout << item << " ";
            }
            std::cout << "}, ";
        }
        std::cout << "\n";
        std::cout << " has predecessors : ";
        for (std::multimap<uint32_t, ClosedIS*>::iterator child = currCI.preds->begin(); child != currCI.preds->end(); ++child) {
          ClosedIS currChild = *child->second;
          std::cout << "{";
          for (auto item : currChild.itemset) {
            std::cout << item << " ";
          }
          std::cout << "}, ";
        }
        std::cout << "\n";
    }
}

void releaseClosures(std::multimap<uint32_t, ClosedIS*> ClosureList) {
  for (multimap<uint32_t, ClosedIS*>::iterator itr = ClosureList.begin(); itr != ClosureList.end(); ++itr) {
    ClosedIS* currCI = itr->second;
    currCI->candidates.clear();
    currCI->gens.clear();
    currCI->itemset.clear();
    currCI->preds->clear();
    delete currCI->preds;
    currCI->succ->clear();
    delete currCI->succ;
    //std::cout << "deleted #" << currCI->id << std::endl;
    if (currCI->id != 1) {
      delete currCI;
    }
  }
}

void sanityCheck(GenNode* n) {
    if (n->clos == nullptr) {
        std::cout << "Sanity check failed for \"generator\" ";
        for (auto item : n->items()) {
            std::cout << item << " ";
        }
        std::cout << "\n";
    }
    for (std::map<uint32_t, GenNode*>::iterator child = n->succ->begin(); child != n->succ->end(); ++child) {
        sanityCheck(child->second);
    }
}

//only use this for (very) small datasets
void sanityCheck_full(ClosedIS* clos, TIDList* TList) {
    std::set<uint32_t> cIS = clos->itemset;
    std::set<uint32_t> closTL;

    if(!cIS.empty())closTL = TList->getISTL(clos->itemset);

    for (std::set<GenNode*>::iterator genIt = clos->gens.begin(); genIt != clos->gens.end(); ++genIt) {
        std::set<uint32_t> items = (*genIt)->items();
        if (!items.empty()){
            std::set<uint32_t> genTL = TList->getISTL(items);
            if (closTL != genTL) {
                std::cout << "pseudo-full SC failed !\n";
            }
        }
    }
}



// template override due to what seems to be a VS bug ?
// comment this function, and compare readings of trx with debug/release configs
// if differences between D and R, then nasty bug still present...
template<>
void Transaction<uint32_t>::load(char* _s, const char* _delims, const short _withcrc) {
  uint32_t v;
  uint32_t oldV = 1 << 31;
  clean();
  char* pch = _s;
  while (pch != 0) {
    std::from_chars(pch, pch + strlen(pch), v);
    //std::cout << "item is " << v << std::endl;
    //Stupid hack to avoid blank spaces at end of lines
    if (v != oldV) {
      __data.push_back(v);
    }
    pch = strtok(0, _delims);
    oldV = v;
  }
};

int main(int argc, char** argv) {
    auto start = std::chrono::high_resolution_clock::now();
    uint32_t window_size = 0;
    uint32_t exitAt = 0;
    float minconf = 0.5;
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <min_supp> [exit_at] [out_cis_gen] [window_size] [out_order] [min_conf] [out_lattice] [out_exact_rules] [out_informative_rules]" << std::endl;
        return 1;
    }
    minSupp = strtoul(argv[2], 0, 10);
    char* output_lattice = nullptr;
    char* output_cis_gen = nullptr;
    char* output_order = nullptr;
    char* output_exact_rules = nullptr;
    char* output_informative_rules = nullptr;

    if (argc >= 4) {
        exitAt = strtoul(argv[3], 0, 10);
    }
    if (argc >= 5) {
        output_cis_gen = argv[4];
    }
    if (argc >= 6) {
        window_size = strtoul(argv[5], 0, 10);
        TListByID = new std::set<uint32_t>*[window_size];
        for (uint32_t k = 0; k < window_size; k++) {
            TListByID[k] = new std::set<uint32_t>();
        }
    }
    if (argc >= 7) {
        output_order = argv[6];
    }
    if (argc >= 8) {
        minconf = strtof(argv[7], 0);
        if (minconf < 0.0f || minconf > 1.0f) {
            std::cerr << "Minimum confidence must be between 0.0 and 1.0" << std::endl;
            return 1;
        }
    }
    if (argc >= 9) {
        output_lattice = argv[8];
    }
    if (argc >= 10) {
        output_exact_rules = argv[9];
    }
    if (argc >= 11) {
        output_informative_rules = argv[10];
    }

    // Set default rule output files if not provided
    std::string exact_rules_file = output_exact_rules ? output_exact_rules : "../outExactRules.txt";
    std::string informative_rules_file = output_informative_rules ? output_informative_rules : "../outInformativeRules.txt";

    std::ifstream input(argv[1]);
    if (!input.is_open()) {
        std::cerr << "Failed to open input file: " << argv[1] << std::endl;
        return 1;
    }
    char s[10000];
    uint32_t i = 0;

    TIDList* TList = new TIDList();
    std::set<uint32_t> closSet;
    std::multimap<uint32_t, ClosedIS*> ClosureList;

    input.getline(s, 10000);
    char* pch = strtok(s, " ");
    Transaction<uint32_t> new_transaction = Transaction<uint32_t>(pch, " ", 0);
    i++;
    std::vector<uint32_t> closSetvec = *new_transaction.data();
    TListByID[i % window_size]->insert(closSetvec.begin(), closSetvec.end());

    closSet.insert(closSetvec.begin(), closSetvec.end());
    TList->add(closSet, i);

    while (i < minSupp) {
        input.getline(s, 10000);
        char* pch = strtok(s, " ");
        Transaction<uint32_t> new_transaction = Transaction<uint32_t>(pch, " ", 0);
        i++;
        std::vector<uint32_t> closSetvec = *new_transaction.data();
        TListByID[i % window_size]->insert(closSetvec.begin(), closSetvec.end());

        std::set<uint32_t> closSetPart(closSetvec.begin(), closSetvec.end());
        TList->add(closSetPart, i);

        std::set<uint32_t>::iterator it1 = closSet.begin();
        std::set<uint32_t>::iterator it2 = closSetPart.begin();

        while ((it1 != closSet.end()) && (it2 != closSetPart.end())) {
            if (*it1 < *it2) {
                closSet.erase(it1++);
            }
            else if (*it2 < *it1) {
                ++it2;
            }
            else {
                ++it1;
                ++it2;
            }
        }
        closSet.erase(it1, closSet.end());
    }

    ClosedIS EmptyClos(closSet, minSupp, &ClosureList);
    GenNode* root = new GenNode(1 << 31, nullptr, &EmptyClos);
    while (input.getline(s, 10000)) {
        i++;
        if (i == 121) {
            i++; i--;
        }
        char* pch = strtok(s, " ");
        Transaction<uint32_t> new_transaction = Transaction<uint32_t>(pch, " ", 0);
        std::vector<uint32_t> t_nVec = *new_transaction.data();
        std::set<uint32_t> t_n(t_nVec.begin(), t_nVec.end());

        Addition(t_n, i, root, TList, &ClosureList);

        if (i > window_size) {
            Deletion(*TListByID[i % window_size], i - window_size, root, TList, &ClosureList);
        }

        TListByID[i % window_size]->clear();
        TListByID[i % window_size]->insert(t_n.begin(), t_n.end());

        if (i % 100 == 0) {
            std::cout << i << " transactions processed" << std::endl;
        }
        if (i % 500 == 0) {
            auto stop = std::chrono::high_resolution_clock::now();
            std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " milliseconds elapsed between start and current transaction" << std::endl;
        }
        if (i == exitAt) {
            break;
        }
    }

    // Build and output the generator lattice
    GenLatticeNode* latticeRoot = buildGenLattice(&ClosureList, TList);
    if (output_lattice) {
        std::ofstream f;
        f.open(output_lattice);
        printGenLattice(latticeRoot, f);
        f.close();
    } else {
        std::cout << "Generator Lattice:\n";
        printGenLattice(latticeRoot, std::cout);
    }

    // Write rules using the lattice to user-specified files
    extractERFromLattice(latticeRoot, exact_rules_file);
    extractIRFromLattice(latticeRoot, minconf, informative_rules_file);

    std::cout << "Displaying all found generators as of transaction " << i << " :\n";

    // Cleanup
    for (std::map<uint32_t, std::set<uint32_t>*>::iterator oo = TList->TransactionList.begin(); oo != TList->TransactionList.end(); ++oo) {
        delete oo->second;
    }
    TList->TransactionList.clear();
    delete TList;

    for (uint32_t k = 0; k < window_size; k++) {
        delete TListByID[k];
    }

    if (output_cis_gen) {
        std::ofstream f;
        f.open(output_cis_gen);
        printAllClosuresWithGensTM(ClosureList, f);
        f.close();
    }
    std::cout << "Total number of generators: " << totalGens << "\n";

    if (output_order) {
        std::ofstream f2;
        f2.open(output_order);
        printClosureOrderTM(ClosureList, f2);
        f2.close();
    }

    releaseClosures(ClosureList);
    releaseAllGens(root);
    releaseGenLattice(latticeRoot);

    return 0;
}