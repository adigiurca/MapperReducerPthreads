#include <iostream>
#include <fstream>
#include <pthread.h>
#include <string>
#include <set>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <map>
#include <sstream>

using namespace std;

int min(int a, int b) {
    return a < b ? a : b;
}

//functie pentru transformarea cuvintelor de tip "he's" in "hes"
void remove_special_characters_and_set_lowercase(string &word) {
    transform(word.begin(), word.end(), word.begin(), ::tolower); //scrierea cuvintelor fara majuscule

    word.erase(remove_if(word.begin(), word.end(), [](char c) { //functie pentru stergerea caracterelor din cuvinte
        return !(('a' <= c && c <= 'z') || (c == ' '));         // care nu sunt alfanumerice
    }), word.end());
}

typedef struct Mapper {
    int id;
    int files_per_mapper;
    vector<string> files;
    int start;
    int end;

    set<pair<string, int>> word_set; //set pentru stocarea structurilor de tip {cuvant, file_ID}
    pthread_mutex_t mutex;
} MAPPER;

typedef struct Reducer {
    int id;
    int start;
    int end;

    unordered_map<string, set<int>> word_map; //hash-map pentru stocarea structurilor de top {cuvant, {File_ID1, File_ID2, ...}}
    pthread_mutex_t mutex;
} REDUCER;

//structura atat pentru mapperi cat si pentru reduceri pentru a apela functia pthread_create si pthread_join o singura data
typedef struct {
    MAPPER *mappers;
    REDUCER *reducers;
    int id;
    int number_of_mappers;
    int number_of_reducers;
    pthread_barrier_t *barrier;
} THREAD_TYPE;

void *mapper_reduce_thread(void *arg) {
    THREAD_TYPE *thread = (THREAD_TYPE *) arg;

    if (thread->id < thread->number_of_mappers) {
        MAPPER &mapper = thread->mappers[thread->id];

        int start = mapper.start;
        int end = mapper.end;

        for (int j = 0; j < end - start; j++) {
            ifstream file_read_by_mapper(mapper.files[j]);
            if (!file_read_by_mapper.is_open()) {
                cerr << "File " << mapper.files[j] << " not found" << endl;
                exit(1);
            }

            string buffer;
            while (getline(file_read_by_mapper, buffer)) {
                remove_special_characters_and_set_lowercase(buffer);
                istringstream stream(buffer);
                string word;
                while (stream >> word) {
                    if (!word.empty()) {
                        pthread_mutex_lock(&mapper.mutex);
                        mapper.word_set.insert({word, start + j + 1});
                        pthread_mutex_unlock(&mapper.mutex);
                    }
                }
            }
        }
    }

   // bariera pentru a astepta ca toti mapperi sa isi termine jobul inainte ca reduceri sa inceapa
    pthread_barrier_wait(thread->barrier);

    if (thread->id >= thread->number_of_mappers) {
        int reducer_id = thread->id - thread->number_of_mappers;
        REDUCER &reducer = thread->reducers[reducer_id];

        int start = reducer.start;
        int end = reducer.end;

        //prelucrarea structurilor din set-urile create de mapperi
        for (int i = 0; i < thread->number_of_mappers; ++i) {
            pthread_mutex_lock(&thread->mappers[i].mutex); //mutex pentru citirea din set
            for (const auto &entry : thread->mappers[i].word_set) {
                string word = entry.first;
                int file_id = entry.second;
                char initial = word[0];

                if (initial >= 'a' && initial <= 'z' &&
                    initial - 'a' >= start && initial - 'a' < end) {
                    pthread_mutex_lock(&reducer.mutex); //mutex pentru scrierea in hash-map
                    reducer.word_map[word].insert(file_id);
                    pthread_mutex_unlock(&reducer.mutex);
                }
            }
            pthread_mutex_unlock(&thread->mappers[i].mutex);
        }

        //crearea de fisiere de output (ex. a.txt, b.txt, ...)
        for (char ch = 'a' + start; ch < 'a' + end; ch++) {
            string filename(1, ch);
            filename += ".txt";
            ofstream outfile(filename);

            if (outfile.is_open()) {
                map<string, set<int>> filtered_words;
                for (const auto &pair : reducer.word_map) { //for pentru scrierea tuturor cuvintelor care incep cu o
                    if (pair.first[0] == ch) {              // anumita litera in documentul asociat cu acea litera
                        filtered_words[pair.first] = pair.second;
                    }
                }

                //sortarea cuvintelor
                vector<pair<string, set<int>>> sorted_words(filtered_words.begin(), filtered_words.end());
                sort(sorted_words.begin(), sorted_words.end(),
                     [](const pair<string, set<int>> &a, const pair<string, set<int>> &b) {
                         if (a.second.size() != b.second.size())
                             return a.second.size() > b.second.size(); // descrescator in functie de numarul de fisiere in care se regasesc
                         return a.first < b.first; //crescator daca se regasesc in numar egal de fisiere
                     });

                ostringstream buffer; // buffer temporar pentru a scrie in acesta continutul fisierelor de output
                                      // dupa care acesta este pus integral in fisierul de output.
                                      // este mai eficient in acest mod deoarece scrierea intr-un fisier de mai multe ori
                                      // este mai costisitor


                // for pentru crearea structurii "cuvant:[fileID1, fileID2, ...]"
                for (const auto &pair : sorted_words) {
                    buffer << pair.first << ":[";

                    for (auto it = pair.second.begin(); it != pair.second.end(); ++it) {
                        if (it != pair.second.begin()) {
                            buffer << " ";
                        }
                        buffer << *it;
                    }
                    buffer << "]";
                    buffer << endl;
                }

                outfile << buffer.str();
            }
        }
    }

    return nullptr;
}

int main(int argc, char **argv) {

    int mapper_number = atoi(argv[1]);
    int reducer_number = atoi(argv[2]);
    string files_name = argv[3];

    int number_of_threads = mapper_number + reducer_number;

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, nullptr, number_of_threads);

    vector<THREAD_TYPE> threads(number_of_threads);
    vector<MAPPER> mappers(mapper_number);
    vector<REDUCER> reducers(reducer_number);

    for (int i = 0; i < mapper_number; i++) {
        pthread_mutex_init(&mappers[i].mutex, nullptr);
    }
    for (int i = 0; i < reducer_number; i++) {
        pthread_mutex_init(&reducers[i].mutex, nullptr);
    }

    ifstream mapper_file(files_name);
    if (!mapper_file.is_open()) {
        cerr << "Error opening file" << endl;
        return 1;
    }

    //citirea numarului fisierelor de input
    string buffer;
    getline(mapper_file, buffer);
    int number_of_files = stoi(buffer);

    vector<pthread_t> mappers_and_reducers_threads(number_of_threads);

    // initializare mapperi
    for (int i = 0; i < mapper_number; i++) {
        mappers[i].id = i;

        //impartirea fisierelor de input relativ egal la thread-urile mapper
        mappers[i].start = mappers[i].id * (double) number_of_files / mapper_number;
        mappers[i].end = min((mappers[i].id + 1) * (double) number_of_files / mapper_number, number_of_files);

        mappers[i].files_per_mapper = mappers[i].end - mappers[i].start;

        for (int j = 0; j < mappers[i].files_per_mapper; j++) {
            getline(mapper_file, buffer);
            if (!buffer.empty() && buffer.back() == '\n') {
                buffer.pop_back(); //elimina caracterul "\n"
            }
            mappers[i].files.push_back(buffer); //adauga numele fisierului in vectorul de fisiere ale mapper-ului
        }
    }

    // initializare reduceri
    int letters_per_reducer = 26 / reducer_number;
    for (int i = 0; i < reducer_number; ++i) {
        reducers[i].id = i;

        //impartirea thread-urilor reduceri in functie de numarul fisierelor de output (26 (numarul de litere din alfabet))
        reducers[i].start = i * letters_per_reducer;
        reducers[i].end = (i == reducer_number - 1) ? 26 : (i + 1) * letters_per_reducer;
    }

    mapper_file.close();

    // crearea thread-urilor prin functia pthread_create
    for (int i = 0; i < number_of_threads; i++) {
        threads[i].number_of_mappers = mapper_number;
        threads[i].number_of_reducers = reducer_number;

        threads[i].id = i;
        //.data() retureaza un pointer catre vectorii de mapperi si reduceri creati anterior
        threads[i].mappers = mappers.data();
        threads[i].reducers = reducers.data();

        threads[i].barrier = &barrier;

        pthread_create(&mappers_and_reducers_threads[i], nullptr, mapper_reduce_thread, (void *) &threads[i]);
    }

    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(mappers_and_reducers_threads[i], nullptr);
    }

    pthread_barrier_destroy(&barrier);

    for (int i = 0; i < mapper_number; i++) {
        pthread_mutex_destroy(&mappers[i].mutex);
    }
    for (int i = 0; i < reducer_number; i++) {
        pthread_mutex_destroy(&reducers[i].mutex);
    }

    return 0;
}
