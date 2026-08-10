#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <random>
#include <algorithm>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;

    // N is the number of integers in our array
    long long n = std::atoll(argv[1]);

    // Fixed number of memory jumps so the test takes enough time
    long long accesses = 50000000; 

    std::vector<int> arr(n);
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    // Shuffle to create random jumps
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    // Link the elements to form a single massive cyclic chain
    for (long long i = 0; i < n - 1; ++i) {
        arr[indices[i]] = indices[i + 1];
    }
    arr[indices[n - 1]] = indices[0]; // Close the loop

    // Warmup: load what we can into cache
    int curr = 0;
    for(long long i = 0; i < n; ++i) {
        curr = arr[curr];
    }

    auto start = std::chrono::high_resolution_clock::now();

    // THE CORE EXPERIMENT: purely chasing memory addresses
    for (long long i = 0; i < accesses; ++i) {
        curr = arr[curr];
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Defeat compiler optimization by using 'curr' at the very end
    volatile int dummy = curr; 

    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Output total time and number of accesses
    std::cout << time.count() << " " << accesses << std::endl;

    return 0;
}
