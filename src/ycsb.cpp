#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include "data_page.h"
#include "pm_ehash.h"

using namespace std;

const string DIR_PREFIX = "./workload/";
const string LOAD_SUFFIX = "-load.txt";
const string RUN_SUFFIX = "-run.txt";
const vector<string> FILENAME_LIST = {
    /*"1w-rw-50-50",  "10w-rw-0-100", "10w-rw-100-0", */"10w-rw-25-75",
    "10w-rw-75-25", "10w-rw-50-50", "220w-rw-50-50"};

void load_bench(PmEHash* PmE, string filename)
{
    filename = DIR_PREFIX + filename + LOAD_SUFFIX;
    printf("filename: %s\n", filename.c_str());
    freopen(filename.c_str(), "r", stdin);
    char op[10];
    uint64_t num;
    int count[1] = {0};
    auto start = chrono::steady_clock::now();
    while (scanf("%s%llu", op, &num) != EOF)
    {
        kv newdata;
        newdata.key = num;
        newdata.value = num;
        int result = PmE->insert(newdata);
        count[0]++;
    }
    auto end = chrono::steady_clock::now();
    chrono::duration<double> elapsed_seconds = end - start;
    printf("load time: %lf\n", elapsed_seconds.count());
    printf("insert count: %d\n", count[0]);
    fclose(stdin);
}

void run_bench(PmEHash* PmE, string filename)
{
    filename = DIR_PREFIX + filename + RUN_SUFFIX;
    printf("filename: %s\n", filename.c_str());
    freopen(filename.c_str(), "r", stdin);
    char op[10];
    uint64_t num;
    int count[3] = {0, 0, 0};
    auto start = chrono::steady_clock::now();
    while (scanf("%s%llu", op, &num) != EOF)
    {
        kv newdata;
        newdata.key = num;
        newdata.value = num;
        if (op[0] == 'U')
        {
            PmE->update(newdata);
            count[0]++;
        }
        else if (op[0] == 'I')
        {
            PmE->insert(newdata);
            count[1]++;
        }
        else if (op[0] == 'R')
        {
            PmE->search(newdata.key, newdata.value);
            count[2]++;
        }
    }
    auto end = chrono::steady_clock::now();
    chrono::duration<double> elapsed_seconds = end - start;
    printf("run time: %lf\n", elapsed_seconds.count());
    printf("insert count: %d\n", count[1]);
    printf("update count: %d\n", count[0]);
    printf("search count: %d\n", count[2]);
    fclose(stdin);
}

int main()
{
    for (auto &filename : FILENAME_LIST)
    {
        PmEHash* PmE = new PmEHash;
        PmE->selfDestory();
        load_bench(PmE, filename);
        run_bench(PmE, filename);
        PmE->selfDestory();
    }
    return 0;
}
