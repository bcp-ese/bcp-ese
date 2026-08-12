# SAT ENCODINGS FOR BANDWIDTH COLORING: A SYSTEMATIC DESIGN STUDY
This repository contains the artifacts for the paper "SAT Encodings for Bandwidth Coloring: A Systematic Design Study".

**Authors:** Duc Trung Kim Nguyen, Tuyen Van Kieu, Khanh Van To

**Affiliation:** Faculty of Information Technology, VNU University of Engineering and Technology, Hanoi, Vietnam

## Short description
The Bandwidth Coloring Problem (BCP) is a generalization of graph coloring in which adjacent vertices must be assigned colors with a minimum separation, with applications in frequency assignment for wireless networks. This repository contains the implementations of six encoding methods organized into three categories: one-variable encodings, two-variable encodings, and block encodings along with several configurable features, including incremental solving modes, symmetry breaking based on graph structure, auxiliary variable caching, and block width strategies.

## Repository structure
- Repository root with build and metadata: build.sh, CMakeLists.txt, README.md

- `dataset`: Graph instance files (COL format) used for experiments

- `src`: Source code files for the encoding methods and configurable features.

- `test`: Test files for validating the implementations.

- `results`: Experiment results.


## How to reproduce experiments
1. Clone the repository:
   ```bash
   git clone https://github.com/bcp-ese/bcp-ese.git
   cd bcp-ese
   ```

2. Add dependencies:
- Install a C++ compiler (e.g., g++) and CMake.
- Compile CaDiCaL SAT solver then place dynamic library files in the `external/cadical` directory and header files in the `external/cadical/include` directory.

3. Build the project:
   ```bash
   ./build.sh
   ```

4. Run binary with desired parameters. For more details, run
    ```bash
    ./bcp -h
    ```

## Results
The `results` folder contains experiment outputs generated during evaluation.
## Citation
If you use this work, please cite:

D. T. K. Nguyen, T. V. Kieu, and K. V. To, "SAT Encodings for Bandwidth Coloring: A Systematic Design Study" (manuscript).
