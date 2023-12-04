#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <cmath>
#include <sys/stat.h>
#include <pthread.h>

using namespace std;

class LZWParallelEncoder {
private:
    int numThreads;
    pthread_attr_t* threadAttributes;
    pthread_t* threadHandles;

    struct ThreadArguments {
        int id;                                       // Thread id
        string filename;                              // The file name that the thread works on
        long long startPosition;                      // Start pos
        long long blockSize;                          // The size of block
        unordered_map<string, long long>* codeTable;  // Table map of the code and content 
        long long startingCode;                       // Start code
        vector<long long> encodedOutput;              // Encoded result
    };

    ThreadArguments* threadArgs;

    void readInputFile(ThreadArguments* currentArgs, string& prefix, string& current, int& code, unsigned long long& count, ifstream& inputFile) {
        prefix += (char)inputFile.get();
        char ch;
        while (!inputFile.fail()) {
            ch = inputFile.get();
            if (!inputFile.eof()) {
                current += ch;
            }
            if (currentArgs->codeTable->find(prefix + current) != currentArgs->codeTable->end()) {
                prefix = prefix + current;
            } else {
                currentArgs->encodedOutput.push_back(currentArgs->codeTable->at(prefix));
                currentArgs->codeTable->insert(make_pair(prefix + current, code));
                code++;
                prefix = current;
            }
            current = "";
            count++;
            if (currentArgs->blockSize > 0 && count >= (unsigned long long)currentArgs->blockSize) {
                break;
            }
        }
        currentArgs->encodedOutput.push_back(currentArgs->codeTable->at(prefix));
    }

    void spawnNextThreads(ThreadArguments* currentArgs, int& code) {
        if (currentArgs->id * 2 <= numThreads) {
            int nextId = currentArgs->id * 2 - 1;
            threadArgs[nextId].startingCode = code;
            threadArgs[nextId].codeTable = currentArgs->codeTable;
        }

        if (currentArgs->id * 2 + 1 <= numThreads) {
            int nextId = currentArgs->id * 2;
            threadArgs[nextId].startingCode = code;
            threadArgs[nextId].codeTable = new unordered_map<string, long long>(*(currentArgs->codeTable));
            pthread_create(&threadHandles[nextId], threadAttributes, &LZWParallelEncoder::encode, (void*)&threadArgs[nextId]);
        }
    }

    void joinSpawnedThreads(ThreadArguments* currentArgs) {
        if (currentArgs->id * 2 <= numThreads) {
            encode((void*)&threadArgs[currentArgs->id * 2 - 1]);
        }

        if (currentArgs->id * 2 + 1 <= numThreads) {
            pthread_join(threadHandles[currentArgs->id * 2], NULL);
            delete threadArgs[currentArgs->id * 2].codeTable;
        }
    }

    static void* encode(void* args) {
        LZWParallelEncoder* encoder = static_cast<LZWParallelEncoder*>(args);
        ThreadArguments* currentArgs = encoder->threadArgs;
        ifstream inputFile(currentArgs->filename);
        inputFile.seekg(currentArgs->startPosition);

        string prefix = "", current = "";
        int code = currentArgs->startingCode;
        unsigned long long count = 1;

        encoder->readInputFile(currentArgs, prefix, current, code, count, inputFile);

        encoder->spawnNextThreads(currentArgs, code);

        encoder->joinSpawnedThreads(currentArgs);

        return NULL;
    }

    long long getFileSize(string fileName) {
        struct stat statBuf;
        int rc = stat(fileName.c_str(), &statBuf);
        return rc == 0 ? statBuf.st_size : -1;
    }

public:
    LZWParallelEncoder(int threads) : numThreads(threads) {
        threadHandles = new pthread_t[numThreads];
        threadArgs = new ThreadArguments[numThreads];
        threadAttributes = new pthread_attr_t;
        pthread_attr_init(threadAttributes);
        pthread_attr_setdetachstate(threadAttributes, PTHREAD_CREATE_JOINABLE);
    }

    ~LZWParallelEncoder() {
        delete [] threadHandles;
        delete [] threadArgs;
        delete threadAttributes;
    }

    void execute(string inputFileName, string outputFileName) {
        long long inputFileSize = getFileSize(inputFileName);
        if (inputFileSize == -1) {
            cerr << "Something is wrong with the input file!" << endl;
            return;
        }

        long long blockSize = inputFileSize / numThreads;
        for (int i = 0; i < numThreads; i++) {
            threadArgs[i].id = i + 1;
            threadArgs[i].filename = inputFileName;
            threadArgs[i].startPosition = i * blockSize;
            threadArgs[i].blockSize = i == numThreads - 1 ? -1 : blockSize;
        }

        threadArgs[0].codeTable = new unordered_map<string, long long>;
        for (int i = 0; i <= 255; i++) {
            string ch = "";
            ch += char(i);
            threadArgs[0].codeTable->insert(make_pair(ch, i));
        }

        threadArgs[0].startingCode = 256;

        pthread_create(&threadHandles[0], threadAttributes, &LZWParallelEncoder::encode, (void*)this);
        pthread_join(threadHandles[0], NULL);

        // Write results to the output file
        ofstream outputFile(outputFileName);
        long long maxCode = 0;
        unsigned long long totalSize = 0;
        outputFile << -2 << " " << numThreads << endl;
        for (int i = 0; i < numThreads; i++) {
            totalSize += threadArgs[i].encodedOutput.size();
            outputFile << -1 << " " << threadArgs[i].encodedOutput.size() << endl;
            for (size_t j = 0; j < threadArgs[i].encodedOutput.size(); j++) {
                outputFile << threadArgs[i].encodedOutput[j] << endl;
                maxCode = threadArgs[i].encodedOutput[j] > maxCode ? threadArgs[i].encodedOutput[j] : maxCode;
            }
        }

        // Display information
        int numBits = (int)ceil(log2(maxCode));
        unsigned long long compressedFileSize = totalSize * numBits / 8;
        cout << "Maximum code: " << maxCode << endl;
        cout << "Bits usage of code: " << numBits << endl;
        cout << "# of output codes: " << totalSize << endl;
        cout << "Best compressed size: " << compressedFileSize << " bytes" << endl;
        cout << "Compression Rate = " << (double)inputFileSize / compressedFileSize << endl;

        delete threadArgs[0].codeTable;
    }
};

int main(int argc, char** argv) {
    int numThreads = atoi(argv[1]);
    if (numThreads < 1 || numThreads > 16) {
        cerr << "Please enter valid thread numbers! (1 - 16)" << endl;
        return 1;
    }

    string inputFileName = argv[2];
    string outputFileName = argv[3];

    LZWParallelEncoder encoder(numThreads);
    encoder.execute(inputFileName, outputFileName);

    return 0;
}
