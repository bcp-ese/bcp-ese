# Review rerun protocol

## Frozen configuration

- Release candidate tag: `review-rerun-rc1`.
- SAT backend: CaDiCaL `rel-1.9.5`, commit
  `146207318796f094dcded87349a64f0c6927309e`.
- CaDiCaL seed: `0` (set by the solver wrapper and recorded in every result row).
- Time limit: `3600` seconds per instance.
- Repetitions: exactly `3` independent processes per instance/configuration.
- Concurrency: `1` for reported timing runs.
- Incremental implementation: permanent bound-tightening unit clauses, without SAT
  assumptions.

Before a reported run, verify that the source is exactly the tagged release and clean:

```sh
git describe --tags --exact-match
git status --porcelain --untracked-files=all
```

The first command must print `review-rerun-rc1`; the second must print nothing. Build
CaDiCaL and BCP by following `external/cadical/README.md`, then run the test suite before
starting the timing experiments.

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

## Statistical unit

For each instance/configuration, report the mean and sample standard deviation of the
three runtimes. For paired Wilcoxon comparisons, use one representative mean runtime per
distinct BCP instance; the three repetitions are not three independent statistical units.

The 20 MS-CAP-formatted files yield only four distinct BCP projections after demands and
self-loops are removed. Follow `dataset/MS_CAP_MANIFEST.md` and never treat all 20 files as
independent BCP instances. Report solved and timeout counts separately and apply the Holm
correction to the prespecified family of confirmatory comparisons.
