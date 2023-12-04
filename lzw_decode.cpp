#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <sys/stat.h>
#include <pthread.h>

using namespace std;

const int ERROR_CODE_FILE_READ = -1;
const long long CODE_NEW_BLOCK = -1;
const long long CODE_FILE_BEGIN = -2;

struct ThreadArgs {
    int threadId;               // Thread id
    long long startCode;        //start code
    long long endCode;          //end code
    unordered_map<long long, string>* dictionary;      //table map of the code and content
    long long nextCodeIndex;               //the index of the next code
    vector<string> decodedStrings;         // decoded result
};

ThreadArgs* thread_args;
int num_thread;
pthread_attr_t* thread_attr;
vector<long long> codeVector;
pthread_t* threads;

void* decode(void* args); 
long long getFileSize(const string& fileName);
void initializeDictionary(unordered_map<long long, string>& dictionary);
void manageThreads(int threadId, int& numThreads, pthread_t* threads, pthread_attr_t* threadAttr, ThreadArgs* threadArgs);
void cleanResources(ThreadArgs* thread_args, pthread_t* threads, pthread_attr_t* thread_attr);
void execute(const std::string& input_file, const std::string& output_file);

//the function to get the size of the input file
long long getFileSize(const string& fileName) {
    struct stat statBuf;
    int result = stat(fileName.c_str(), &statBuf);
    if (result == 0) {
        return statBuf.st_size;
    } else {
        return ERROR_CODE_FILE_READ;
    }
}

//the function to initialize the dictionary map
void initializeDictionary(unordered_map<long long, string>& dictionary) {
    for (int i = 0; i <= 255; ++i) {
        dictionary[i] = string(1, char(i));
    }
}


void manageThreads(int threadId, int& numThreads, pthread_t* threads, pthread_attr_t* threadAttr, ThreadArgs* threadArgs) {
    int nextId1 = threadId * 2 - 1;
    int nextId2 = threadId * 2;

    if (nextId1 < numThreads) {
        threadArgs[nextId1].startCode = threadArgs[threadId - 1].nextCodeIndex;
        threadArgs[nextId1].dictionary = threadArgs[threadId - 1].dictionary;

        // Recursive call for left child
        manageThreads(nextId1 + 1, numThreads, threads, threadAttr, threadArgs);
    }

    if (nextId2 < numThreads) {
        threadArgs[nextId2].startCode = threadArgs[threadId - 1].nextCodeIndex;
        threadArgs[nextId2].dictionary = new unordered_map<long long, string>(*threadArgs[threadId - 1].dictionary);

        // Create new thread for right child
        if (pthread_create(&threads[nextId2], threadAttr, decode, &threadArgs[nextId2]) != 0) {
            cerr << "Error creating thread " << nextId2 << endl;
        }

        // Wait for the right child thread to complete
        if (pthread_join(threads[nextId2], NULL) != 0) {
            cerr << "Error joining thread " << nextId2 << endl;
        }

        delete threadArgs[nextId2].dictionary;
    }
}

void* decode(void* args) 
{
    auto& currentArgs = *static_cast<ThreadArgs*>(args);
    long long previousCode = currentArgs.startCode;
    string previousString = currentArgs.dictionary->at(previousCode);
    string firstChar = previousString.substr(0, 1);
    currentArgs.decodedStrings.push_back(previousString);

    for (long long i = currentArgs.startCode + 1; i <= currentArgs.endCode; ++i) {
        long long currentCode = i;
        string currentString;

        if (currentArgs.dictionary->find(currentCode) == currentArgs.dictionary->end()) {
            currentString = previousString + firstChar;
        } else {
            currentString = currentArgs.dictionary->at(currentCode);
        }

        currentArgs.decodedStrings.push_back(currentString);
        firstChar = currentString.substr(0, 1);
        auto insertValue = std::make_pair(currentArgs.nextCodeIndex, previousString + firstChar);
        currentArgs.dictionary->insert(insertValue);
        currentArgs.nextCodeIndex++;
        previousString = currentString;
    }

    //thread management code
    manageThreads(currentArgs.threadId, num_thread, threads, thread_attr, thread_args);
    return NULL;
}



void cleanResources(ThreadArgs* thread_args, pthread_t* threads, pthread_attr_t* thread_attr) {
    delete thread_args[0].dictionary;
    delete[] threads;
    delete[] thread_args;
    delete thread_attr;
}

void execute(const std::string& input_file, const std::string& output_file) {
    std::ifstream ifile(input_file);
    long long buf;
    ifile >> buf;
    if (ifile.fail() || buf != -2) {
        cerr << "Error: input file is corrupted" << endl;
        ifile.close();
        return; 
    }

    ifile >> num_thread;

    threads = new pthread_t[num_thread];
    thread_args = new ThreadArgs[num_thread];

    int curr_block = 0;
    int code_cnt = 0;
    int block_size = -1;

    while (ifile >> buf) {
        if (buf == -1) {
            thread_args[curr_block].startCode = code_cnt;
            ifile >> block_size;
            for (int i = 0; i < block_size; i++) {
                ifile >> buf;
                codeVector.push_back(buf);
                code_cnt++;
            }
            thread_args[curr_block].endCode = code_cnt - 1;
            curr_block++;
        } else {
            cerr << "Warning: corruption" << endl;
            return;
        }
    }

    struct timespec start, stop;
    double time = 0;

    if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
        perror("clock gettime");
    }

    for (int i = 0; i < num_thread; i++) {
        thread_args[i].threadId = i + 1;
    }

    thread_args[0].dictionary = new std::unordered_map<long long, std::string>;
    for (int i = 0; i <= 255; i++) {
        std::string ch;
        ch += char(i);
        thread_args[0].dictionary->insert(std::make_pair(i, ch));
    }
    thread_args[0].startCode = 256;

    thread_attr = new pthread_attr_t;
    pthread_attr_init(thread_attr);
    pthread_attr_setdetachstate(thread_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&threads[0], thread_attr, decode, (void*)&thread_args[0]);
    pthread_join(threads[0], NULL);

    if (clock_gettime(CLOCK_REALTIME, &stop) == -1) {
        perror("clock gettime");
    }
    time = (stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / 1e9;
    std::cout << "Decoding time = " << time << " sec " << std::endl;

    std::ofstream ofile(output_file);
    for (int i = 0; i < num_thread; i++) {
        for (size_t j = 0; j < thread_args[i].decodedStrings.size(); j++) {
            ofile << thread_args[i].decodedStrings[j];
        }
    }

    // clean the resources
    cleanResources(thread_args, threads, thread_attr);
}



int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "usage: ./lzw_parallel_decode encoded_file extracted_file" << endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    long long input_file_size = getFileSize(input_file);
    if (input_file_size < 0) {
        cerr << "Error: cannot read input file or input file is corrupted" << endl;
        return 1;
    }

    execute(input_file, output_file);  //execute the program to do the compress

    return 0;
}

