#include <iostream>
#include <chrono>
#include <vector>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    long long n = std::atoll(argv[1]);

    std::vector<int> arr(n, 1);
    volatile long long sum = 0; 

    auto start = std::chrono::high_resolution_clock::now();
    for (long long i = 0; i < n; ++i) {
        sum += arr[i];
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << time.count() << std::endl;

    return 0;
}
