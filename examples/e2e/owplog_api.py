"""
A robust interface for discovering, loading, and communicating with the OWPLog API.
"""
import typing
import ctypes
import pathlib
import platform
import threading
import logging
import os
import shutil

import ctypes.util

try:
    from mpi4py import MPI
    _RANK: int = MPI.COMM_WORLD.Get_rank()
except ImportError:
    MPI = None
    _RANK: int = -1

OWPLOG_LIBRARY_PATH_ENVIRONMENT_VARIABLE_NAME: typing.Final[str] = "OWPLOG_LIBRARY_PATH"
"""The name of the acceptable environment variable that points to the owplog C shared library directory."""

OWPLOG_BUILD_DIRECTORY_ENVIRONMENT_VARIABLE_NAME: typing.Final[str] = "OWPLOG_BUILD_DIRECTORY"
"""The name of the acceptable environment variable that points to the owplog C build directory."""

MACOS_LIBRARY_NAME = "libowplog_c.dylib"
LINUX_LIBRARY_NAME = "libowplog_c.so"
# Character encoding used for Python <-> C boundary conversion.
TEXT_ENCODING = "utf-8"
LANGUAGE: typing.Final[bytes] = "python".encode(TEXT_ENCODING)

class OwplogLogMeta(ctypes.Structure):
    _fields_ = [
        ("file", ctypes.c_char_p),
        ("line", ctypes.c_int),
        ("function", ctypes.c_char_p),
        ("language", ctypes.c_char_p),
        ("rank", ctypes.c_int),
    ]

def _library_filename() -> str:
    """Return the platform-specific owplog shared-library filename.

    Returns:
        The expected filename for the current OS.

    Raises:
        No explicit exceptions.
    """
    system = platform.system()
    if system == "Darwin":
        return MACOS_LIBRARY_NAME
    return LINUX_LIBRARY_NAME

def _resolve_library_path(
        explicit: typing.Optional[str] = None,
        library_directory: typing.Optional[str] = None,
        build_dir: typing.Optional[str] = None
) -> pathlib.Path:
    """Resolve the owplog shared-library path from explicit or inferred locations.

    Args:
        explicit: Absolute or relative path provided by caller. If set, this path is used exclusively.
        build_dir: Optional build directory to search before default candidate paths.

    Returns:
        Fully resolved path to an existing owplog shared library.

    Preconditions:
        ``explicit`` should point to the owplog C ABI shared library when provided.

    Postconditions:
        Returned path exists on disk.

    Raises:
        FileNotFoundError: If no candidate shared library path exists.
    """
    if explicit:
        path = pathlib.Path(explicit).resolve()
        if path.is_file():
            return path
        raise FileNotFoundError(f"OWPLog shared library not found: {path}")

    filename = _library_filename()
    candidates: list[pathlib.Path] = collect_candidate_library_paths(
        library_filename=filename,
        library_directory=library_directory,
        build_dir=build_dir
    )

    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()

    linked_library_path: typing.Optional[pathlib.Path] = find_library_with_system_loader(library_filename=filename)

    if linked_library_path is not None:
        return linked_library_path.resolve()

    error_message: str = f"""Could not find {filename}
Checked in:
    - {(os.linesep + '    - ').join(str(c) for c in candidates)}
"""
    raise FileNotFoundError(error_message)


def collect_candidate_library_paths(
    library_filename: str,
    library_directory: typing.Optional[str] = None,
    build_dir: typing.Optional[str] = None,
) -> list[pathlib.Path]:
    """
    Collect possible paths for the OWPLog library
    :param library_filename:
    :param library_directory:
    :param build_dir:
    :return: All paths that may contain the OWPLog library
    """
    candidate_paths: list[pathlib.Path] = []

    if library_directory is not None:
        candidate_paths.extend(
            get_directory_library_candidates(pathlib.Path(library_directory), library_filename)
        )

    if build_dir is not None:
        candidate_paths.extend(
            get_directory_library_candidates(pathlib.Path(build_dir), library_filename)
        )

    environment_library_directory = os.environ.get(OWPLOG_LIBRARY_PATH_ENVIRONMENT_VARIABLE_NAME, None)

    if environment_library_directory is not None and environment_library_directory != library_directory:
        candidate_paths.extend(
            get_directory_library_candidates(pathlib.Path(environment_library_directory), library_filename)
        )

    environment_build_directory = os.environ.get(OWPLOG_BUILD_DIRECTORY_ENVIRONMENT_VARIABLE_NAME, None)

    if environment_build_directory is not None and environment_build_directory != build_dir:
        candidate_paths.extend(
            get_directory_library_candidates(pathlib.Path(environment_build_directory), library_filename)
        )

    module_directory: pathlib.Path = pathlib.Path(__file__).resolve().parent

    candidate_paths.extend([
        module_directory / library_filename,
        module_directory / "lib" / library_filename,
        module_directory / "bin" / library_filename,
        module_directory.parent / library_filename,
        module_directory.parent / "lib" / library_filename,
        module_directory.parent / "bin" / library_filename,
    ])

    current_working_directory: pathlib.Path = pathlib.Path.cwd()

    candidate_paths.extend(
        get_directory_library_candidates(current_working_directory, library_filename)
    )

    path_lookup_result = shutil.which(library_filename)

    if path_lookup_result is not None:
        candidate_paths.append(pathlib.Path(path_lookup_result))

    candidate_paths: list[pathlib.Path] = deduplicate_paths(candidate_paths)
    return candidate_paths

def deduplicate_paths(candidate_paths: typing.Iterable[pathlib.Path]) -> list[pathlib.Path]:
    deduplicated_paths: typing.List[pathlib.Path] = []
    seen_paths: set[pathlib.Path] = set()

    for candidate_path in candidate_paths:
        resolved_path = candidate_path.expanduser().resolve()

        if resolved_path in seen_paths:
            continue

        seen_paths.add(resolved_path)
        deduplicated_paths.append(resolved_path)

    return deduplicated_paths

def get_directory_library_candidates(
    root_path: pathlib.Path,
    library_filename: str,
) -> list[pathlib.Path]:
    """
    Get a list of candidate library paths for the build directory for OWPLog
    :return:
    """
    return [
        root_path / library_filename,
        root_path / "lib" / library_filename,
        root_path / "bin" / library_filename,
        root_path / "Release" / library_filename,
        root_path / "Debug" / library_filename,
        root_path / "lib" / "Release" / library_filename,
        root_path / "lib" / "Debug" / library_filename,
        root_path / "bin" / "Release" / library_filename,
        root_path / "bin" / "Debug" / library_filename,
    ]


def find_library_with_system_loader(library_filename: str) -> typing.Optional[pathlib.Path]:
    """
    Try to resolve OWPLog through system library discovery

    :param library_filename:
    :return:
    """
    library_stem: str = get_library_stem_from_filename(library_filename=library_filename)
    discovered_library_text: typing.Optional[str] = ctypes.util.find_library(library_stem)

    if discovered_library_text is None:
        return None

    discovered_library_path: pathlib.Path = pathlib.Path(discovered_library_text).expanduser()

    if discovered_library_path.is_file():
        return discovered_library_path.resolve()

    path_lookup_result: typing.Optional[str] = shutil.which(discovered_library_path)

    if path_lookup_result is None:
        return None

    path_lookup_path: pathlib.Path = pathlib.Path(path_lookup_result)

    if path_lookup_path.is_file():
        path_lookup_path = path_lookup_path.resolve()

    return path_lookup_path


def get_library_stem_from_filename(library_filename: str) -> str:
    """Return the library stem (without extension) from a filename."""
    library_path: pathlib.Path = pathlib.Path(library_filename)
    library_name: str = library_path.name

    if library_name.startswith("lib"):
        library_name = library_name[3:]

    for shared_library_extension in [".so", ".dylib"]:
        if library_name.endswith(shared_library_extension):
            return library_name[: -len(shared_library_extension)]

    return library_name


class OWPLogAPI:
    """Thread-safe singleton wrapper around low-level owplog C ABI functions.

    Preconditions:
        The owplog C shared library must be present and loadable.

    Postconditions:
        Once constructed, function signatures are configured and ready for calls.
    """

    _instance: typing.Optional["OWPLogAPI"] = None
    _instance_lock = threading.Lock()

    def __init__(
        self,
        explicit_library_path: typing.Optional[str] = None,
        library_path: typing.Optional[str] = None,
        build_dir: typing.Optional[str] = None
    ) -> None:
        """Initialize the ctypes bridge and configure exported function signatures.

        Args:
            library_path: Optional explicit shared-library path.
            build_dir: Optional build directory to prioritize during lookup.

        Raises:
            FileNotFoundError: If library path cannot be resolved.
            OSError: If the shared library cannot be loaded.
        """
        self._lock = threading.Lock()
        self._lib_path = _resolve_library_path(
            explicit=explicit_library_path,
            library_directory=library_path,
            build_dir=build_dir
        )
        self._lib = ctypes.CDLL(str(self._lib_path))
        self._configure_signatures()

    def _configure_signatures(self) -> None:
        """Configure ctypes argtypes/restype for every used C ABI function.

        Raises:
            AttributeError: If expected symbols are missing from the shared library.
        """
        self._lib.owplog_logger_log.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p]
        self._lib.owplog_logger_log_ex.argtypes = [
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_void_p,
        ]

    @classmethod
    def shared(
        cls,
        explicit_library_path: typing.Optional[str],
        library_path: typing.Optional[str] = None,
        build_dir: typing.Optional[str] = None
    ) -> "OWPLogAPI":
        """Return process-wide shared C API wrapper instance.

        Args:
            explicit_library_path: Optional direct path to the owplog C shared library.
            library_path: Optional library path used only for first-time initialization.
            build_dir: Optional build directory used only for first-time initialization.

        Returns:
            Shared singleton instance.

        Raises:
            FileNotFoundError: If first-time initialization cannot locate the library.
            OSError: If first-time initialization cannot load the library.
        """
        with cls._instance_lock:
            if cls._instance is None:
                cls._instance = cls(
                    explicit_library_path=explicit_library_path,
                    library_path=library_path,
                    build_dir=build_dir
                )
            return cls._instance

    def log(self, level: int, logger_name: str, message: str) -> None:
        """Submit one message with an explicit logger name.

        Args:
            level: Python logging level value.
            logger_name: Logical logger name.
            message: Final message payload.

        Postconditions:
            Message is forwarded to upstream logger or fallback output via C ABI.
        """
        with self._lock:
            self._lib.owplog_logger_log(
                level if isinstance(level, int) else logging.getLevelName(level),
                logger_name.encode(TEXT_ENCODING),
                message.encode(TEXT_ENCODING),
            )

    def log_ex(
            self,
            level: int,
            logger_name: str,
            message: str,
            file_name: typing.Optional[str],
            line_no: int,
            function_name: typing.Optional[str]
    ) -> None:
        """Submit one message with explicit source/language/rank metadata.

        Args:
            level: Python logging level value.
            logger_name: Logical logger name.
            message: Final message payload.
            file_name: Optional source filename.
            line_no: Source line number (`0` if unknown).
            function_name: Optional function name.

        Postconditions:
            Metadata is passed by pointer to ``owplog_logger_log_ex`` for this call.
        """
        meta = OwplogLogMeta(
            file_name.encode(TEXT_ENCODING) if file_name else None,
            int(line_no),
            function_name.encode(TEXT_ENCODING) if function_name else None,
            LANGUAGE,
            _RANK,
        )
        with self._lock:
            self._lib.owplog_logger_log_ex(
                level if isinstance(level, int) else logging.getLevelName(level),
                logger_name.encode(TEXT_ENCODING),
                message.encode(TEXT_ENCODING),
                ctypes.byref(meta),
            )