# C++ Graph & Cache Performance Benchmark

<<<<<<< HEAD
This repository consists of two separate components:
1. GraphProject: A generic undirected graph implementation tested using the Google Test framework.
2. Cache Benchmark: Python (Jupyter) scripts that automatically generate C++ source code, compile it, and test memory access speeds (Cache hit/miss).

## Prerequisites

To successfully run the entire project, you need to have the following installed:
* C++ Compiler (supporting the C++23 standard)
* CMake (v4.1 or newer)
* Python 3.x
* Python Packages: jupyter, numpy, matplotlib

You can install the required Python packages via pip by running: pip install jupyter numpy matplotlib

## Running the Project

1. Compiling and Testing the Graph (C++)

Open a terminal in the root directory of the project and execute the following CMake commands to build it:
mkdir build
cd build
cmake ..
cmake --build .

After a successful build, you can run the Google Tests:
On Linux/macOS: ./GraphProject
On Windows: GraphProject.exe

2. Cache Benchmark (Jupyter Notebook)

The cache performance measurement is fully managed through a Jupyter Notebook. The notebook automatically generates the required C++ source files (normal.cpp, random.cpp, stride_test.cpp), invokes g++ to compile them with -O3 optimization, and visualizes the benchmark results.

To run the benchmark, open your terminal and type: jupyter notebook

Then, open arraydata.ipynb in your browser and run all cells (Run All). The script will generate two distinct plots:
* A performance comparison between sequential (normal) and random access to array elements.
* A cache line size analysis measuring the execution time per element for various stride sizes.
=======
https://www.vldb.org/pvldb/vol17/p4827-li.pdf
>>>>>>> 265dcc3e01d960ee5732908a5c86cc0bccdf8f6e
