# Review rerun protocol

## Frozen configuration

- Release candidate: `review-rerun-rc3`. This annotated tag must be created from the reviewed
  baseline adapter and logging changes before any official run. The earlier RC1/RC2 tags do
  not contain the complete revision protocol and must not be used for reported results.
- SAT backend: CaDiCaL `rel-1.9.5`, commit
  `146207318796f094dcded87349a64f0c6927309e`.
- CaDiCaL seed: `0` (set by the solver wrapper and recorded in every result row). The fixed
  seed controls this experimental factor across configurations; the three fresh processes
  measure elapsed-time repeatability, not seed-to-seed variation.
- Time limit: `3600` seconds per instance.
- Repetitions: exactly `3` fresh process executions per instance/configuration.
- Concurrency: `1` for reported timing runs.
- Incremental implementation: permanent bound-tightening unit clauses, without SAT
  assumptions.

Before a reported run, verify that the source is exactly the tagged release and clean:

```sh
git describe --tags --exact-match
git status --porcelain --untracked-files=all
```

The first command must print `review-rerun-rc3`; the second must print nothing. Build
CaDiCaL and BCP by following `external/cadical/README.md`, then run the test suite before
starting the timing experiments.

Also run the benchmark-export regression test:

```sh
python3 -m unittest test/test_benchmark.py
```

## Benchmark command template

The benchmark driver defaults to three repetitions, but `--repetitions 3` is kept explicit
in reported commands:

```sh
python3 benchmark.py METHOD \
  --time_limit 3600 \
  --repetitions 3 \
  --num_concurrent_processes 1 \
  --binary ./bcp \
  [configuration flags]
```

Relevant configuration flags are:

- `--use_incremental_solving --variable_for_incremental x|y|both`;
- `--use_symmetry_breaking`;
- `--use_pairwise` for 2G, 2L, X, and Xa only;
- `--width fixed|vary` for X and Xa.

The valid incremental modes are `y` for 1G/1L; `x`, `y`, or `both` for 2G/2L; and `x`
for X/Xa.

## Complete manuscript matrix

The executable runner `run-all-36-review-rerun.sh` covers exactly the 36 configurations
listed in the manuscript configuration matrix:

- 1G and 1L: non-incremental or incremental `y`, each with symmetry off/on (4 each);
- 2G and 2L: non-incremental, incremental `x`, or incremental `y`, each with symmetry
  off/on (6 each);
- X and cached Xa: fixed or varying width, non-incremental or incremental `x`, each with
  symmetry off/on (8 each).

Although the implementation supports additional experimental modes, incremental `both`,
pairwise constraints, and `Xa(no-cache)` are not part of the manuscript's 36-configuration
matrix. With the 53 current `.col` files and three repetitions, the runner performs
`36 x 53 x 3 = 5724` solver invocations. It validates the release tag and SHA before
starting, runs with concurrency one, and can resume from its most recent partial result.

Invoke the runner from a checkout containing the script, while passing a separate clean,
detached worktree at the frozen release tag as its first argument:

```sh
git worktree add --detach ../bcp-rerun 'refs/tags/review-rerun-rc3^{}'
./run-all-36-review-rerun.sh ../bcp-rerun ../rerun-output/review-rerun-rc3
```

Build `../bcp-rerun/bcp` from the pinned CaDiCaL dependency before starting. If execution
is interrupted, invoke the same command again; completed configurations are validated and
skipped, while an incomplete configuration resumes from the latest saved CSV.

## Statistical unit

For each input-file/configuration pair, report the mean and sample standard deviation of the
three runtimes. If a manuscript table reports a total across instances, compute one total for
each `run_id`, then report the mean and sample standard deviation of the three totals. Do not
sum three repetitions into a single benchmark score.

For paired comparisons, first average the three repetitions of each instance/configuration.
Use the 33 GEOM instances and exactly one prespecified representative of each MS-CAP-derived
BCP projection: `c21_1_d1.col`, `c21_2_d1.col`, `c25_1_d3.col`, and `c55_1_d1.col`. This gives
37 paired statistical units when all pairs complete. The other MS-CAP source files remain in
the file-level reproducibility results but do not enter comparative benchmark totals or the
inferential analysis.

The 20 MS-CAP-formatted files yield only four distinct BCP projections after demands and
self-loops are removed. Follow `dataset/MS_CAP_MANIFEST.md` and never treat all 20 files as
independent BCP instances.

The confirmatory family contains exactly two two-sided paired Wilcoxon signed-rank tests:

1. Xa (fixed width, incremental `x`, symmetry on) versus POPH-S-B;
2. Xa (fixed width, incremental `x`, symmetry on) versus POP-S-B.

Apply Holm's correction across these two p-values at `alpha=0.05`. Feature comparisons for
incremental solving, symmetry breaking, block width, and the `x`/`y` entry point are
descriptive unless a separate family is specified before the run. A paired test includes only
instances for which both configurations complete all three repetitions; always report the
paired sample size, Holm-adjusted p-value, median paired runtime ratio, and the
completion/timeout counts. Report GEOM120b as mean and sample
standard deviation over its three repetitions without a separate significance test.

## POP-S-B and POPH-S-B baselines

The comparison uses the authors' public implementation at the pinned commit
`8f19dbff4135e6cff9e4b147ebe8462603d5fe03` of
<https://github.com/s6dafabe/popsatgcpbcp>. The encoding classes `POP_SAT_BCP` and
`POPHyb_SAT_BCP` are imported unchanged. A BCP-only adapter supplies the controlled parts of
the experiment: the same greedy upper bound as the proposed implementation, explicit
descending linear search, CaDiCaL 1.9.5 with seed 0, a 3600-second joint time budget, and
three sequential fresh-process repetitions.

Prepare the pinned source and Python environment once:

```sh
./prepare-pop-baseline-source.sh
```

After creating the clean RC3 worktree and building its CaDiCaL 1.9.5 executable, run both
baselines with:

```sh
./run-pop-baselines-review-rerun.sh \
  ../bcp-rerun \
  ./external/popsatgcpbcp/source \
  ../cadical-1.9.5/build/cadical \
  ../rerun-output/review-rerun-rc3/pop-baselines
```

This runner performs `2 x 53 x 3 = 318` instance runs with concurrency one. It rejects a
modified upstream checkout, a non-RC3 worktree, a different CaDiCaL version, incomplete or
mixed-protocol resume files, and final CSVs that fail the row/metadata checks.

For every proposed configuration and both baselines, the reported runtime is
`encoding_time + total_solving_time`. For the baseline adapter, `encoding_time` is the sum of
the elapsed construction and DIMACS-writing time for all candidate bounds; for the proposed
implementation it is the sum of its in-memory encoding calls. `total_solving_time` is the
sum over all CaDiCaL calls. Input parsing and greedy upper-bound computation are excluded.
The 3600-second budget applies to the encoding-plus-solving sum, not to SAT time alone.

## Solver-effort counters

Every proposed-configuration result row records the aggregate CaDiCaL counters `conflicts`, `decisions`,
`propagations`, `learned`, `learned_lits`, `restarts`, and `reduced`. Counters that CaDiCaL
omits when their value is zero are exported explicitly as zero. The benchmark driver also
preserves any additional counters printed by the solver, and the matrix runner rejects a
final CSV if any required counter is missing or nonnumeric.

For the exploratory comparison of `x`- and `y`-based tightening in 2G/2L, retain symmetry
off and symmetry on as separate strata. For each instance/configuration, first summarize the
three repetitions, then compare paired time, conflict, decision, and propagation ratios.
The aggregate counters characterize search effort; they do not identify the variable
composition of a learned clause or whether a particular clause learned at one bound is used
at a later bound.
