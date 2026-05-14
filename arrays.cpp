#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>


long long potenciraj(int baza, int eksponent) {
    long long rezultat = 1;
    for (int i = 0; i < eksponent; ++i) {
        rezultat *= baza;
    }
    return rezultat;
}

void fun1(const long long n) {
    std::vector<int> nums(n);
    std::iota(nums.begin(), nums.end(), 0);
    std::vector<int> numsP(n);

    std::iota(numsP.begin(), numsP.end(), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(numsP.begin(), numsP.end(), gen);



    std::vector<int> arr(n);

    std::uniform_int_distribution<> dist(1, 100);
    std::generate(arr.begin(), arr.end(), [&]() { return dist(gen); });

    volatile long long sum = 0, sumP = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int br : nums) {
        sum += arr[br];
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    std::cout << "Time( " << n << ", normal ): "  << time.count() << "ns" <<  std::endl;


    start = std::chrono::high_resolution_clock::now();

    for (int br : numsP) {
        sumP += arr[br];
    }

    end = std::chrono::high_resolution_clock::now();

    auto timeP = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    std::cout << "Time( " << n << ", permutation ): "  << timeP.count() << "ns" <<  std::endl;

}

int main() {

    for (int i = 5; i < 25; ++i) {
        fun1(potenciraj(2, i));
    }

    return 0;

}