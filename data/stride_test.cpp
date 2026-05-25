#include <iostream>
#include <chrono>
#include <vector>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;

    long long n = std::atoll(argv[1]);
    long long stride = std::atoll(argv[2]);

    std::vector<int> arr(n, 1);
    volatile long long sum = 0;

    long long accesses = n / stride;
    if (accesses == 0) return 0;

    auto start = std::chrono::high_resolution_clock::now();


    for (long long i = 0; i < n; i += stride) {
        sum += arr[i];
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    std::cout << time.count() << " " << accesses << std::endl;

    return 0;
}
