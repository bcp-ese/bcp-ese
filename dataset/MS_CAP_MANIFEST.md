# MS-CAP input manifest and BCP projections

This directory contains 20 MS-CAP-formatted benchmark input files used to derive BCP instances:

- 16 c21 files: `c21_1_d1.col`--`c21_8_d2.col`;
- 2 c25 files: `c25_1_d3.col` and `c25_1_d4.col`;
- 2 c55 files: `c55_1_d1.col` and `c55_2_d2.col`.

The BCP reader uses the non-loop `e` records. It ignores the multicoloring-demand records (`n`) and self-loops, which encode within-vertex separation in the source MS-CAP format. No vertex-to-clique transformation is performed.

Consequently, the 20 input files yield only four distinct BCP projections. Files with the same projection must not be treated as statistically independent BCP instances.

| BCP projection | Input files | Files | Vertices | Non-loop edges | Known optimum |
|---|---|---:|---:|---:|---:|
| c21, direct distance 1 | odd c21 indices, both demand vectors | 8 | 21 | 102 | 7 |
| c21, direct distance 2 | even c21 indices, both demand vectors | 8 | 21 | 102 | 9 |
| c25 | both c25 demand vectors | 2 | 25 | 134 | 8 |
| c55 | both c55 demand vectors | 2 | 55 | 362 | 7 |

The raw `p band` edge counts are 123 for c21, 159 for c25, and 417 for c55 because they include one self-loop per vertex. C21 was corrected by removing the spurious direct topology relation `14--19`. The c25 topology is the symmetric `C^(3)` compatibility matrix from the published CAP benchmark appendix.

## Projection checksums

These SHA-256 values hash canonical lines of the form `min(u,v) max(u,v) weight` after removing self-loops and `n` records. The triples are sorted in ascending integer-tuple order, joined with `\n`, and the final line has no trailing newline.

| Projection | SHA-256 |
|---|---|
| c21, direct distance 1 | `8359cbd8a77f980f2d11d07e134acf66f1b99a682475da1874102ef91bf7b42b` |
| c21, direct distance 2 | `7e69e8de9389ec5fdc65f1db74975364d111e2007ebb919eb740f7669e516caa` |
| c25 | `384e3ece24f55986c15a76855150c9bdb45af0d8c50fb03e317c124971a5e4bb` |
| c55 | `e4c3d70ab7c08cc6b3c494640d479a4df6f630907ae5612c01f9f26853c9f62d` |

## Full input-file checksums

| File | SHA-256 |
|---|---|
| `c21_1_d1.col` | `cd7d8de43d330f8e1ede366963a1ee6facce729feb6a4012d22caf78b734d8f8` |
| `c21_1_d2.col` | `2550e966c00526dcd883283442927e619fe87d6a13309f8fa3a18fcc97a14a02` |
| `c21_2_d1.col` | `156ef9674cf0c38f34b0a3f750e960ab33ca4bfaf2faad1a01391ab33a8af183` |
| `c21_2_d2.col` | `e1e7707a560a472085bffc60e7ea6f1f3e31a52d7776a0b612f75df36e5f8b0b` |
| `c21_3_d1.col` | `51cce8dd87f7d6814bb122446275d55db96c04c89a543462772cb3aa00d7baa2` |
| `c21_3_d2.col` | `82c2ed420db09898b5c682f6da28c883860da5590776807f81617d07c8849153` |
| `c21_4_d1.col` | `bf96ae45cb349014939c09c14f4f2177788c7dd8f9f119bea08139faa9b32866` |
| `c21_4_d2.col` | `39ff8927223d4f26b5e0527f8825dc7df41895d32de828a81ca854bd8bc90ad2` |
| `c21_5_d1.col` | `59d08066599c2120b31f048cfea21f1ef480d55174f9c01365745541174b8c4c` |
| `c21_5_d2.col` | `09317853a0af14f3873fb5f1ef5f8c1375f0ef4cdddfbc98e60dee47af6b3277` |
| `c21_6_d1.col` | `71ffa7f5da161f4fefced9f37161268afe7f681cf9bb97bdb3e5d0666abe63cd` |
| `c21_6_d2.col` | `bbfee5a4668c63e566ad9779bd48c7b9c6b8799fee3b0836a01505afc44a54f1` |
| `c21_7_d1.col` | `05992ed7f85872fed05c27be3f53509d77f0cbae8692b9067588bb1bc8fff237` |
| `c21_7_d2.col` | `32ae88fc2ec0cb3e15d0ef168656bc509aa551ffc445ff7b9ee0df1fe34258ec` |
| `c21_8_d1.col` | `28b84a8360eb39fd3f90800829aeee1b5809e2fd41d708dad879f7c638f52af8` |
| `c21_8_d2.col` | `3db07d6cf02bfcebdc048bcbc06864e4e8ee292ea4000843468b8a9f11f8d990` |
| `c25_1_d3.col` | `0473e51c7479131af559624342825751d0987c64257ffa5ca66a9da059cef81f` |
| `c25_1_d4.col` | `aa341d75e08d0a1fea904a74f84cdd1bed10a7e1ab33af7cfcc966a0a28e5588` |
| `c55_1_d1.col` | `618fb3bbcf0092690653631b62b4c7c9ec889a5b5681543a81e4f48ccf69019d` |
| `c55_2_d2.col` | `ce1eb64f2c9cd784068cee5c0748ccb9b44f5e49d2580ba1b09c0099fbd2120f` |

## Sources

- Dias et al., *Integer and constraint programming approaches for providing optimality to the bandwidth multicoloring problem*: <https://www.numdam.org/articles/10.1051/ro/2020065/>.
- CAP benchmark appendix containing the c25 `C^(3)` matrix: <https://personal.ntu.edu.sg/elpwang/PDF_web/04_csa_cap_chapter.pdf>.
