import argparse
import concurrent.futures
import datetime
import hashlib
import multiprocessing
import os
import platform
import shlex
import subprocess
import sys
import time
import traceback
from pathlib import Path

import pandas as pd
import psutil

REPO_ROOT = Path(__file__).resolve().parent
SOLVER_NAME = 'CaDiCaL'


def sha256_file(file_path):
    digest = hashlib.sha256()
    with open(file_path, 'rb') as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def parse_solver_output(output_text):
    data = {}
    for line in output_text.strip().split('\n'):
        if ':' in line:
            key, val = line.split(':', 1)
            val = val.strip()
            try:
                data[key.strip()] = int(val)
            except ValueError:
                try:
                    data[key.strip()] = float(val)
                except ValueError:
                    data[key.strip()] = val
    return data


def process_instance(solving_method: str, file_path: str, upper_bound=None, time_limit: int = None,
                     queue: multiprocessing.Queue = None, use_incremental_solving: bool = False,
                     variable_for_incremental='x', use_symmetry_breaking: bool = False, use_heuristic: bool = False,
                     width: str = '', run_id: int = 1, source_sha: str = '',
                     source_dirty: bool = False, binary_path: str = './bcp', binary_sha256: str = '',
                     compiler: str = '', runner_versions: str = '', concurrency: int = 1):
    """
    Process a single instance of the given path.
    """
    print(f"Solving {file_path}")
    stats = None
    try:
        command = [binary_path, f"{file_path}", f"{solving_method}"]

        if upper_bound is not None:
            command.append("-ub")
            command.append(str(upper_bound))

        if time_limit is not None:
            command.append("-t")
            command.append(str(time_limit))

        if use_incremental_solving:
            command.append(f"-i")
            command.append("-v")
            command.append(variable_for_incremental)

        if use_symmetry_breaking:
            command.append("--use-symmetry-breaking")

        if use_heuristic:
            command.append("--use-pairwise")

        if width:
            command.append("--width")
            command.append(width)

        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=True
        )

        stats = parse_solver_output(result.stdout)
        stats.update({
            'run_id': run_id,
            'source_sha': source_sha,
            'source_dirty': source_dirty,
            'input_sha256': sha256_file(file_path),
            'binary_sha256': binary_sha256,
            'command': shlex.join(command),
            'host': platform.node(),
            'platform': platform.platform(),
            'compiler': compiler,
            'runner_versions': runner_versions,
            'concurrency': concurrency,
            'recorded_at': datetime.datetime.now().astimezone().isoformat(),
            'solver': SOLVER_NAME,
            'method': solving_method,
            'time_limit': time_limit if time_limit is not None else '',
            'incremental': use_incremental_solving,
            'incremental_variable': variable_for_incremental if use_incremental_solving else '',
            'symmetry_breaking': use_symmetry_breaking,
            'pairwise': use_heuristic,
            'width': width,
        })

    except subprocess.CalledProcessError as e:
        print(f"Command failed with return code {e.returncode}")
        print(f"Error message: {e.stderr}")
        exit(1)

    if stats['status'] == -1:
        stats['status'] = 'UNKNOWN'
    elif stats['status'] == 0:
        stats['status'] = 'UNSATISFIABLE'
    elif stats['status'] == 1:
        stats['status'] = 'SATISFIABLE'
    elif stats['status'] == 2:
        stats['status'] = 'OPTIMAL'

    queue.put(stats)


def worker(args):
    (solving_method, file_path, upper_bound, time_limit, use_incremental_solving,
     variable_for_incremental, use_symmetry_breaking, use_heuristic, width,
     run_id, source_sha, source_dirty, binary_path, binary_sha256, compiler, runner_versions, concurrency) = args

    queue = multiprocessing.Queue()
    p = multiprocessing.Process(target=process_instance, kwargs={'solving_method': solving_method,
                                                                 'file_path': file_path,
                                                                 'upper_bound': upper_bound,
                                                                 'time_limit': time_limit,
                                                                 'queue': queue,
                                                                 'use_incremental_solving': use_incremental_solving,
                                                                 'variable_for_incremental': variable_for_incremental,
                                                                 'use_symmetry_breaking': use_symmetry_breaking,
                                                                 'use_heuristic': use_heuristic,
                                                                 'width': width,
                                                                 'run_id': run_id,
                                                                 'source_sha': source_sha,
                                                                 'source_dirty': source_dirty,
                                                                 'binary_path': binary_path,
                                                                 'binary_sha256': binary_sha256,
                                                                 'compiler': compiler,
                                                                 'runner_versions': runner_versions,
                                                                 'concurrency': concurrency})
    p.start()

    peak_memory = 0
    while p.is_alive():
        try:
            proc = psutil.Process(p.pid)
            mem = proc.memory_info().rss
            for child in proc.children(recursive=True):
                try:
                    mem += child.memory_info().rss
                except psutil.NoSuchProcess:
                    continue
            peak_memory = max(peak_memory, mem)
        except psutil.NoSuchProcess:
            break
        time.sleep(0.1)

    p.join()

    if p.exitcode != 0:
        raise RuntimeError(f"Process for {file_path} failed with exit code {p.exitcode}")

    instance_stats = queue.get()
    instance_stats['name'] = Path(file_path).name
    instance_stats['memory_usage'] = round(peak_memory / (1024 ** 2), 5)
    return instance_stats


def benchmark(solving_method: str, time_limit: int = None, continue_from: str = None,
              num_concurrent_processes: int = 1, save_interval_seconds: int = 60,
              use_predefine_upper_bound: bool = False, use_incremental_solving: bool = False,
              variable_for_incremental: str = 'x', use_symmetry_breaking: bool = False, use_heuristic: bool = False,
              width: str = '', repetitions: int = 3,
              allow_dirty: bool = False, binary_path: str = './bcp'):
    """
    Benchmark a dataset using concurrent.futures.ProcessPoolExecutor.
    Periodically exports partial results to avoid data loss from unexpected interruptions.
    """
    source_sha = subprocess.run(
        ['git', 'rev-parse', 'HEAD'], cwd=REPO_ROOT, capture_output=True, text=True, check=True
    ).stdout.strip()
    source_dirty = bool(subprocess.run(
        ['git', 'status', '--porcelain', '--untracked-files=all', '--', '.', ':(exclude)result/**'],
        cwd=REPO_ROOT, capture_output=True, text=True, check=True
    ).stdout.strip())
    if source_dirty and not allow_dirty:
        raise RuntimeError('Refusing to benchmark a dirty source tree; commit/archive the release candidate first.')

    binary_path = Path(binary_path)
    if not binary_path.is_absolute():
        binary_path = (Path.cwd() / binary_path).resolve()
    if not (binary_path.is_file() and os.access(binary_path, os.X_OK)):
        raise FileNotFoundError(f'Benchmark executable not found or not executable: {binary_path}')
    binary_path = str(binary_path)
    binary_sha256 = sha256_file(binary_path)

    compiler_executable = os.environ.get('CXX', 'c++')
    try:
        compiler = subprocess.run(
            [*shlex.split(compiler_executable), '--version'], capture_output=True, text=True, check=True
        ).stdout.splitlines()[0]
    except (OSError, subprocess.CalledProcessError, IndexError):
        compiler = f'unavailable ({compiler_executable})'
    runner_versions = (
        f'Python {platform.python_version()}; pandas {pd.__version__}; psutil {psutil.__version__}'
    )

    # Load existing progress if continuing
    if continue_from is None:
        dataset_stats = pd.DataFrame(columns=[
            'name', 'V', 'E', 'upper_bound', 'variables', 'clauses',
            'status', 'span', 'encoding_time', 'total_solving_time', 'time_used', 'memory_usage',
            'timed_out', 'optimality_proven', 'run_id', 'source_sha', 'source_dirty', 'input_sha256',
            'binary_sha256', 'command', 'host', 'platform', 'compiler', 'runner_versions',
            'concurrency', 'recorded_at',
            'solver', 'solver_seed', 'method', 'time_limit', 'incremental', 'incremental_variable',
            'symmetry_breaking', 'pairwise', 'width'
        ])
    else:
        dataset_stats = pd.read_csv(continue_from)
        required_resume_columns = {
            'name', 'run_id', 'source_sha', 'input_sha256', 'binary_sha256', 'solver', 'solver_seed', 'method',
            'time_limit', 'incremental', 'incremental_variable', 'symmetry_breaking', 'pairwise', 'width',
            'host', 'platform', 'compiler', 'runner_versions', 'concurrency'
        }
        missing_columns = required_resume_columns - set(dataset_stats.columns)
        if missing_columns:
            raise ValueError(f'Cannot safely resume: missing provenance columns {sorted(missing_columns)}')
        if not dataset_stats.empty:
            if set(dataset_stats['source_sha'].astype(str)) != {source_sha}:
                raise ValueError('Cannot resume results produced from a different source commit.')
            if set(dataset_stats['binary_sha256'].astype(str)) != {binary_sha256}:
                raise ValueError('Cannot resume results produced by a different executable.')
            expected_config = {
                'solver': SOLVER_NAME,
                'solver_seed': '0',
                'method': solving_method,
                'time_limit': '' if time_limit is None else str(time_limit),
                'incremental': str(use_incremental_solving),
                'incremental_variable': variable_for_incremental if use_incremental_solving else '',
                'symmetry_breaking': str(use_symmetry_breaking),
                'pairwise': str(use_heuristic),
                'width': width,
                'host': platform.node(),
                'platform': platform.platform(),
                'compiler': compiler,
                'runner_versions': runner_versions,
                'concurrency': str(num_concurrent_processes),
            }
            for column, expected in expected_config.items():
                actual = set(dataset_stats[column].fillna('').astype(str))
                if actual != {expected}:
                    raise ValueError(
                        f'Cannot resume because {column} differs: expected {expected!r}, found {sorted(actual)!r}'
                    )
            for row in dataset_stats[['name', 'input_sha256']].drop_duplicates().itertuples(index=False):
                current_input = REPO_ROOT / 'dataset' / row.name
                if not current_input.is_file() or sha256_file(current_input) != row.input_sha256:
                    raise ValueError(f'Cannot resume because the input changed or is missing: {row.name}')

    tasks = []
    completed = set(zip(dataset_stats['name'], dataset_stats['run_id']))

    if use_predefine_upper_bound:
        bound_path = REPO_ROOT / 'bound' / f'bound_{solving_method}.csv'
        if not bound_path.is_file():
            raise FileNotFoundError(f'Predefined upper-bound file not found: {bound_path}')
        bound = pd.read_csv(bound_path)
        for run_id in range(1, repetitions + 1):
            for row in bound.itertuples():
                if (row.name, run_id) in completed:
                    continue
                tasks.append((
                    solving_method, str(REPO_ROOT / 'dataset' / row.name), row.upper_bound, time_limit,
                    use_incremental_solving, variable_for_incremental, use_symmetry_breaking,
                    use_heuristic, width, run_id, source_sha, source_dirty, binary_path,
                    binary_sha256, compiler, runner_versions, num_concurrent_processes
                ))
    else:
        folder_path = REPO_ROOT / 'dataset'
        files = sorted(file.name for file in folder_path.iterdir()
                       if file.is_file() and file.suffix.lower() == '.col')

        for run_id in range(1, repetitions + 1):
            for file in files:
                if (file, run_id) in completed:
                    continue
                tasks.append((
                    solving_method, f'{folder_path}/{file}', None, time_limit,
                    use_incremental_solving, variable_for_incremental, use_symmetry_breaking,
                    use_heuristic, width, run_id, source_sha, source_dirty, binary_path,
                    binary_sha256, compiler, runner_versions, num_concurrent_processes
                ))

    executor = concurrent.futures.ProcessPoolExecutor(max_workers=num_concurrent_processes)
    futures = []

    last_save_time = time.time()  # track time of last save

    try:
        for task in tasks:
            futures.append(executor.submit(worker, task))

        for future in concurrent.futures.as_completed(futures):
            try:
                result = future.result()
                result.pop('file_path', None)
                dataset_stats.loc[len(dataset_stats)] = result

                # Periodically save partial results
                current_time = time.time()
                if current_time - last_save_time >= save_interval_seconds:
                    export_result(solving_method, dataset_stats, use_incremental_solving, variable_for_incremental,
                                  use_symmetry_breaking, use_heuristic, width, suffix="partial")
                    last_save_time = current_time

            except Exception:
                traceback.print_exc()
                for f in futures:
                    f.cancel()
                executor.shutdown(wait=False, cancel_futures=True)
                export_result(solving_method, dataset_stats, use_incremental_solving, variable_for_incremental,
                              use_symmetry_breaking, use_heuristic, width, suffix="crash")
                sys.exit(1)

    except KeyboardInterrupt:
        for f in futures:
            f.cancel()
        executor.shutdown(wait=False, cancel_futures=True)
        export_result(solving_method, dataset_stats, use_incremental_solving, variable_for_incremental,
                      use_symmetry_breaking, use_heuristic, width, suffix="interrupted")
        sys.exit(1)

    # Final export at the end
    export_result(solving_method, dataset_stats, use_incremental_solving, variable_for_incremental,
                  use_symmetry_breaking, use_heuristic, width)


def export_result(solving_method: str, stat, use_incremental_solving, variable_for_incremental, use_symmetry_breaking,
                  use_heuristic, width, suffix=None):
    """
    Export both detailed results and summary reports.
    `suffix` is appended to the filename to indicate partial or crash saves.
    """
    result_dir = REPO_ROOT / 'result'
    result_dir.mkdir(exist_ok=True)
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    suffix = f"_{suffix}" if suffix else ""

    file_name = f'{solving_method}'

    if width:
        file_name += f'-{width}-width'

    if use_incremental_solving:
        file_name += f'-I-{variable_for_incremental}'
    else:
        file_name += '-N'

    if use_symmetry_breaking:
        file_name += '-S'
    else:
        file_name += '-0'

    if use_heuristic:
        file_name += '-P'
    else:
        file_name += '-0'

    file_name += f'_{SOLVER_NAME}'

    result_path = result_dir / f'{file_name}{suffix}_{timestamp}.csv'

    stat.to_csv(result_path, index=False)
    print(f'Results written to {result_path}')


def main():
    parser = argparse.ArgumentParser(description='Benchmarking script for BCP solver.')
    parser.add_argument('solving_method', type=str,
                        choices=['1L', '1G', '2L', '2G', 'Xa(no-cache)', 'Xa(cache)', "X"],
                        help="Method for solving the BCP problem.")
    parser.add_argument('--time_limit', type=int, help='Time limit for solving one instance.',
                        default=None)
    parser.add_argument('--continue_from', type=str, help='Result file name to continue from.',
                        default=None)
    parser.add_argument('--num_concurrent_processes', type=int, default=1,
                        help='Number of concurrent processes to use for benchmarking.')
    parser.add_argument('--save_interval_seconds', type=int, default=300,
                        help='Interval (in seconds) between periodic partial result exports. Default is 300 seconds.')
    parser.add_argument('--use_predefined_upper_bound', action='store_true',
                        help='Use predefined upper bounds from the bound files.')
    parser.add_argument('--use_incremental_solving', action='store_true', default=False,
                        help='Use incremental solving strategy (if applicable).')
    parser.add_argument('--variable_for_incremental', type=str, default='x',
                        choices=['x', 'y', 'both'],
                        help='Variable to use for incremental solving (if applicable).')
    parser.add_argument('--use_symmetry_breaking', action='store_true', default=False,
                        help='Use symmetry breaking in the solving process')
    parser.add_argument('--use_pairwise', action='store_true', default=False,
                        help='Enable pairwise while encoding, if applicable')
    parser.add_argument('--width', type=str, default="", choices=['fixed', '', 'vary'],
                        help='Width parameter (if applicable).')
    parser.add_argument('--repetitions', type=int, default=3,
                        help='Number of independent timing repetitions per instance (default: 3).')
    parser.add_argument('--allow_dirty', action='store_true',
                        help='Allow exploratory runs from a dirty working tree (not for reported results).')
    parser.add_argument('--binary', type=str, default='./bcp',
                        help='Path to the built bcp executable (default: ./bcp).')

    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error('--repetitions must be at least 1')
    if args.num_concurrent_processes < 1:
        parser.error('--num_concurrent_processes must be at least 1')
    if args.save_interval_seconds <= 0:
        parser.error('--save_interval_seconds must be positive')
    if args.time_limit is not None and args.time_limit < 0:
        parser.error('--time_limit must be non-negative')
    staircase_methods = {'Xa(no-cache)', 'Xa(cache)', 'X'}
    if args.solving_method in staircase_methods and not args.width:
        parser.error('The selected staircase encoding requires --width fixed or --width vary')
    if args.solving_method not in staircase_methods and args.width:
        parser.error('--width is only supported by staircase encodings')
    if args.use_pairwise and args.solving_method in {'1G', '1L'}:
        parser.error('--use_pairwise is not supported by the one-variable encodings')
    if args.use_incremental_solving:
        allowed_variables = {
            '1G': {'y'},
            '1L': {'y'},
            '2G': {'x', 'y', 'both'},
            '2L': {'x', 'y', 'both'},
            'Xa(no-cache)': {'x'},
            'Xa(cache)': {'x'},
            'X': {'x'},
        }
        if args.variable_for_incremental not in allowed_variables[args.solving_method]:
            parser.error(
                f"--variable_for_incremental={args.variable_for_incremental} is not supported by "
                f"{args.solving_method}"
            )

    benchmark(args.solving_method, args.time_limit, args.continue_from,
              args.num_concurrent_processes, args.save_interval_seconds, args.use_predefined_upper_bound,
              args.use_incremental_solving, args.variable_for_incremental, args.use_symmetry_breaking,
              args.use_pairwise, args.width, args.repetitions, args.allow_dirty, args.binary)


if __name__ == "__main__":
    main()
