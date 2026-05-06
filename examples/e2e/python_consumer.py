#!/usr/bin/env python3
"""
An example of some Python code that might use the OWPLog logging solution

This is meant to be arbitrarily complex to show how a long-running python process will use the OWPLog logging
solution alongside another standard logging handler
"""
import typing
import collections.abc as generic
import argparse
import hashlib
import collections
import math
import dataclasses
import random
import statistics
import sys
import time
import logging
import logging.handlers
import logging.config
import pathlib

from owplog_logger import OwplogHandler

DEFAULT_MATRIX_SIZE: int = 50
DEFAULT_ITERATION_COUNT: int = 20

DEFAULT_RANDOM_SEED: int = 8675309

DEFAULT_GRAPH_NODE_COUNT: int = 30

LOGGING_CONFIG: dict[str, typing.Any] = {
    "version": 1,
    "disable_existing_loggers": False,
    "root": {
        "level": "DEBUG",
        "handlers": ["file", "owplog"]
    },
    "handlers": {
        "file": {
            "level": "DEBUG",
            "formatter": "default",
            "class": "logging.handlers.RotatingFileHandler",
            "filename": "python_consumer.log",
            "maxBytes": 10485760,
            "backupCount": 2
        },
        "owplog": {
            "level": "DEBUG",
            "formatter": "bare",
            "class": OwplogHandler,
            "explicit_path": "/DMOD/christopher.tubbs/owplog/cmake-build-debug/libowplog_c.so"
        }
    },
    "formatters": {
        "default": {
            "format": "%(asctime)s %(levelname)s %(name)s %(message)s",
            "datefmt": "%Y-%m-%d %H:%M:%S"
        },
        "bare": {
            "format": "%(message)s",
        }
    }
}

if not logging.root.hasHandlers():
    logging.config.dictConfig(LOGGING_CONFIG)

LOGGER: logging.Logger = logging.getLogger(pathlib.Path(__file__).stem)
LOGGER.info(f"Running '{__file__}' from {pathlib.Path.cwd()}")

@dataclasses.dataclass(frozen=True)
class WorkConfiguration:
    matrix_size: int = DEFAULT_MATRIX_SIZE
    iteration_count: int = DEFAULT_ITERATION_COUNT
    random_seed: int = DEFAULT_RANDOM_SEED
    graph_node_count: int = DEFAULT_GRAPH_NODE_COUNT

@dataclasses.dataclass
class WorkResult:
    checksum: str
    duration_seconds: float
    numeric_score: float


def parse_arguments(*args) -> WorkConfiguration:
    argument_parser: argparse.ArgumentParser = argparse.ArgumentParser(
        description="Run a needlessly complicated CPU-bound Python workload"
    )
    argument_parser.add_argument(
        "--matrix-size",
        type=int,
        default=DEFAULT_MATRIX_SIZE,
        help="The size of the square matrix to operate on"
    )
    argument_parser.add_argument(
        "--iteration-count",
        type=int,
        default=DEFAULT_ITERATION_COUNT,
        help="The number of iterations to perform"
    )
    argument_parser.add_argument(
        "--random-seed",
        type=int,
        default=DEFAULT_RANDOM_SEED,
        help="The random seed to use for the workload"
    )
    argument_parser.add_argument(
        "--graph-node-count",
        type=int,
        default=DEFAULT_GRAPH_NODE_COUNT,
        help="The number of nodes in the graph to operate on"
    )

    try:
        parsed_arguments: argparse.Namespace = argument_parser.parse_args()
    except Exception as e:
        LOGGER.error(f"Failed to parse arguments: {args}")
        raise

    if parsed_arguments.matrix_size < 1:
        raise ValueError("Matrix size must be greater than zero")

    if parsed_arguments.iteration_count < 1:
        raise ValueError("Iteration count must be greater than zero")

    if parsed_arguments.graph_node_count < 1:
        raise ValueError("Graph node count must be greater than zero")

    return WorkConfiguration(
        matrix_size=parsed_arguments.matrix_size,
        iteration_count=parsed_arguments.iteration_count,
        random_seed=parsed_arguments.random_seed,
        graph_node_count=parsed_arguments.graph_node_count
    )

def generate_matrix(matrix_size: int, random_number_generator: random.Random) -> list[list[float]]:
    """
    Generates a matrix of random integers
    :param matrix_size: The size of the matrix
    :param random_number_generator: The random number generator to use
    :return: The generated matrix
    """
    matrix: list[list[float]] = []

    for row_index in range(matrix_size):
        matrix_row: list[float] = []

        for column_index in range(matrix_size):
            wave_component: float = math.sin(row_index * 0.13) * math.cos(column_index * 0.17)
            noise_component: float = random_number_generator.uniform(-1.0, 1.0)
            matrix_row.append(wave_component + noise_component)

        matrix.append(matrix_row)
    return matrix


def transform_matrix(matrix: list[list[float]], iteration_index: int) -> list[list[float]]:
    """
    Transform a matrix using neighboring cells and trignometric sludge

    :param matrix:
    :param iteration_index:
    :return:
    """
    matrix_size: int = len(matrix)
    transformed_matrix: list[list[float]] = []

    iteration_scale: float = 1.0 + (iteration_index % 17) * 0.001

    for row_index in range(matrix_size):
        transformed_matrix_row: list[float] = []

        previous_row_index: int = (row_index - 1) % matrix_size
        next_row_index: int = (row_index + 1) % matrix_size

        for column_index in range(matrix_size):
            previous_column_index: int = (column_index - 1) % matrix_size
            next_column_index: int = (column_index + 1) % matrix_size

            center_value: float = matrix[row_index][column_index]
            north_value: float = matrix[previous_row_index][column_index]
            south_value: float = matrix[next_row_index][column_index]
            west_value: float = matrix[row_index][previous_column_index]
            east_value: float = matrix[row_index][next_column_index]

            neighbor_average: float = (
                (north_value + south_value + west_value + east_value) / 4.0
            )
            transformed_value: float = math.tanh((center_value * 0.62 + neighbor_average * 0.38) * iteration_scale)
            transformed_value += math.sin(center_value + iteration_index * 0.01) * 0.003

            transformed_matrix_row.append(transformed_value)

        transformed_matrix.append(transformed_matrix_row)
    return transformed_matrix

def generate_graph(node_count: int, random_number_generator: random.Random) -> dict[int, list[int]]:
    """
    Generate a deterministic directed graph
    :param node_count:
    :param random_number_generator:
    :return:
    """
    graph: dict[int, list[int]] = {}
    for node_index in range(node_count):
        edge_count: int = random_number_generator.randint(1, 5)
        adjacent_nodes: set[int] = set()

        while len(adjacent_nodes) < edge_count:
            adjacent_nodes.add(random_number_generator.randrange(node_count))

        graph[node_index] = sorted(adjacent_nodes)
    return graph

def calculate_graph_score(
    graph: dict[int, list[int]],
    starting_node: int,
    maximum_depth: int,
) -> int:
    """
    Traverse the graph breadth-first and compute a deterministic score
    :param graph:
    :param starting_node:
    :param maximum_depth:
    :return:
    """
    visited_nodes: set[int] = set()
    pending_nodes: collections.deque[tuple[int, int]] = collections.deque()

    pending_nodes.append((starting_node, 0))
    traversal_score: int = 0

    while pending_nodes:
        current_node, current_depth = pending_nodes.popleft()

        if current_node in visited_nodes:
            continue

        if current_depth > maximum_depth:
            continue

        visited_nodes.add(current_node)
        traversal_score += (current_node + 1) * (current_depth + 3)

        for adjacent_node in graph[current_node]:
            pending_nodes.append((adjacent_node, current_depth + 1))

    return traversal_score


def calculate_matrix_statistics(matrix: list[list[float]]) -> tuple[float, float, float]:
    """
    Calculate summary statistics for a matrix

    :param matrix:
    :return:
    """
    flattened_values: list[float] = [value for row in matrix for value in row]

    mean_value: float = statistics.fmean(flattened_values)
    standard_deviation: float = statistics.pstdev(flattened_values)
    median_value: float = statistics.median(flattened_values)

    return mean_value, standard_deviation, median_value


def update_checksum(
    checksum_builder,
    matrix_statistics: tuple[float, float, float],
    graph_score: int,
    iteration_index: int,
) -> None:
    """
    Update the checksum with the current workflow state

    :param checksum_builder:
    :param matrix_statistics:
    :param graph_score:
    :param iteration_index:
    :return:
    """
    mean_value, standard_deviation, median_value = matrix_statistics
    checksum_fragment: str = (
        f"{iteration_index}:{mean_value:.2f}:{standard_deviation:.2f}:{median_value:.2f}:{graph_score}"
        f"{mean_value:.12f}:"
        f"{standard_deviation:.12f}:"
        f"{median_value:.12f}:"
        f"{graph_score}"
    )
    checksum_builder.update(checksum_fragment.encode("utf-8"))


def run_workflow(configuration: WorkConfiguration) -> WorkResult:
    """
    Run the artificial workload

    :param configuration:
    :return:
    """
    start_time: float = time.perf_counter()

    random_number_generator: random.Random = random.Random(configuration.random_seed)
    matrix: list[list[float]] = generate_matrix(configuration.matrix_size, random_number_generator)
    graph: dict[int, list[int]] = generate_graph(configuration.graph_node_count, random_number_generator)

    checksum_builder = hashlib.sha256()
    numeric_score: float = 0.0

    for iteration_index in range(configuration.iteration_count):
        matrix = transform_matrix(
            matrix=matrix,
            iteration_index=iteration_index,
        )
        starting_node: int = iteration_index % configuration.graph_node_count
        maximum_depth: int = 3 + (iteration_index % 5)

        graph_score: int = calculate_graph_score(
            graph=graph,
            starting_node=starting_node,
            maximum_depth=maximum_depth,
        )
        matrix_statistics: tuple[float, float, float] = calculate_matrix_statistics(matrix)

        update_checksum(
            checksum_builder=checksum_builder,
            matrix_statistics=matrix_statistics,
            graph_score=graph_score,
            iteration_index=iteration_index,
        )

        mean_value, standard_deviation, median_value = matrix_statistics
        numeric_score += (
            abs(mean_value) + standard_deviation + median_value + graph_score * 0.000001
        )

        if iteration_index % 10 == 0:
            LOGGER.info(f"Iteration {iteration_index} complete")
            LOGGER.info(f"Score: {numeric_score:.2f}")
            LOGGER.info(f"Graph Score: {graph_score}")

    duration_seconds: float = time.perf_counter() - start_time

    return WorkResult(
        checksum=checksum_builder.hexdigest(),
        duration_seconds=duration_seconds,
        numeric_score=numeric_score,
    )


def main(*args) -> int:
    """
    The entry point of the script
    :return: The process exit code
    """
    work_configuration: WorkConfiguration = parse_arguments(*args)

    try:
        result: WorkResult = run_workflow(work_configuration)

        LOGGER.info("Python example workload complete")
        LOGGER.info(f"Checksum: {result.checksum}")
        LOGGER.info(f"Duration: {result.duration_seconds:.2f} seconds")
        LOGGER.info(f"Numeric score: {result.numeric_score:.2f}")
    except BaseException as exc:
        LOGGER.error(f"Unexpected error in main: {exc}", exc_info=True)
        return 1
    else:
        return 0

if __name__ == "__main__":
    main(sys.argv[1:])