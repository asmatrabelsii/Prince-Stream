#include "Prince-Stream.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " input_file min_supp window_size [min_conf] [sger_file] [sgar_file]" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    uint32_t minSup = std::atoi(argv[2]);
    uint32_t windowSize = std::atoi(argv[3]);
    float minConf = (argc > 4) ? std::atof(argv[4]) : 0.3;
    std::string sgerFile = (argc > 5) ? argv[5] : "../sger.txt";
    std::string sgarFile = (argc > 6) ? argv[6] : "../sgar.txt";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        MGStream stream(minSup, windowSize, minConf);
        std::ifstream in(inputFile);
        if (!in.is_open()) {
            std::cerr << "Error: Could not open input file " << inputFile << std::endl;
            return 1;
        }

        std::string line;
        uint32_t tid = 0;
        const uint32_t batchSize = 100; // Process in batches

        while (std::getline(in, line)) {
            Transaction<uint32_t> t;
            t.load(line, " ");
            std::set<uint32_t> items(t.data.begin(), t.data.end());

            stream.addTransaction(items, tid);

            if (tid >= windowSize) {
                uint32_t oldTid = tid - windowSize;
                if (stream.TListByID[oldTid % windowSize]) {
                    stream.deleteTransaction(*stream.TListByID[oldTid % windowSize], oldTid);
                    delete stream.TListByID[oldTid % windowSize];
                    stream.TListByID[oldTid % windowSize] = nullptr;
                }
            }

            stream.TListByID[tid % windowSize] = new std::set<uint32_t>(items);
            tid++;

            if (tid % batchSize == 0) {
                auto current = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> duration = current - start;
                std::cout << "Processed " << tid << " transactions, Execution time: " << duration.count() << " seconds" << std::endl;
            }
        }

        stream.buildMGLattice();
        stream.extractRules(sgerFile, sgarFile);
        stream.printLattice("../lattice.txt");

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << "Total Execution time: " << duration.count() << " seconds" << std::endl;

        in.close();

        stream.clearLattice();
    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}