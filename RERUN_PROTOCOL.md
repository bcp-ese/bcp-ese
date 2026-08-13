# Review rerun protocol

## Frozen configuration

- Release candidate tag: `review-rerun-rc2`. This tag must be created only after committing
  the reviewed benchmark-export fix and passing both the solver and export tests; the earlier
  `review-rerun-rc1` tag does not preserve the required CaDiCaL counters in its CSV output.
- SAT backend: CaDiCaL `rel-1.9.5`, commit
  `146207318796f094dcded87349a64f0c6927309e`.
- CaDiCaL seed: `0` (set by the solver wrapper and recorded in every result row).
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

The first command must print `review-rerun-rc2`; the second must print nothing. Build
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
git worktree add --detach ../bcp-rerun 'refs/tags/review-rerun-rc2^{}'
./run-all-36-review-rerun.sh ../bcp-rerun ../rerun-output/review-rerun-rc2
```

Build `../bcp-rerun/bcp` from the pinned CaDiCaL dependency before starting. If execution
is interrupted, invoke the same command again; completed configurations are validated and
skipped, while an incomplete configuration resumes from the latest saved CSV.

## Statistical unit

For each instance/configuration, report the mean and sample standard deviation of the
three runtimes. For paired Wilcoxon comparisons, use one representative mean runtime per
distinct BCP instance; the three repetitions are not three independent statistical units.

The 20 MS-CAP-formatted files yield only four distinct BCP projections after demands and
self-loops are removed. Follow `dataset/MS_CAP_MANIFEST.md` and never treat all 20 files as
independent BCP instances. Report solved and timeout counts separately and apply the Holm
correction to the prespecified family of confirmatory comparisons.

## Solver-effort counters

Every result row records the aggregate CaDiCaL counters `conflicts`, `decisions`,
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
