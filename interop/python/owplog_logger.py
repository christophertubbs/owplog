"""Python logging integration for the constrained owplog C API.

This module provides:
1. A thin thread-safe ``ctypes`` bridge to the C ABI.
2. ``logging.Handler`` and ``logging.Logger`` integrations.
3. Utilities for installing/restoring ``OwplogLogger`` as the active logger class.
"""

from __future__ import annotations

import ctypes
import logging
import platform
import threading
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator
from typing import Optional

# Platform-specific default library names used during auto-discovery.
WINDOWS_LIBRARY_NAME = "owplog_c.dll"
MACOS_LIBRARY_NAME = "libowplog_c.dylib"
LINUX_LIBRARY_NAME = "libowplog_c.so"
# Character encoding used for Python <-> C boundary conversion.
TEXT_ENCODING = "utf-8"
# Default handler level used by OwplogLogger.enable_owplog.
DEFAULT_PYTHON_LEVEL = logging.INFO

# Numeric level constants mirrored from ``include/owplog/c_api.h``.
OWPLOG_TRACE = 0
OWPLOG_DEBUG = 10
OWPLOG_INFO = 20
OWPLOG_WARNING = 30
OWPLOG_ERROR = 40
OWPLOG_CRITICAL = 50
OWPLOG_OFF = 60


def _library_filename() -> str:
    """Return the platform-specific owplog shared-library filename.

    Returns:
        The expected filename for the current OS.

    Raises:
        No explicit exceptions.
    """
    system = platform.system()
    if system == "Windows":
        return WINDOWS_LIBRARY_NAME
    if system == "Darwin":
        return MACOS_LIBRARY_NAME
    return LINUX_LIBRARY_NAME


def _resolve_library_path(explicit: Optional[str] = None, build_dir: Optional[str] = None) -> Path:
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
        path = Path(explicit).resolve()
        if not path.exists():
            raise FileNotFoundError(f"owplog shared library not found: {path}")
        return path

    filename = _library_filename()
    candidates = []
    if build_dir:
        root = Path(build_dir)
        candidates.extend([root / filename, root / "Release" / filename, root / "Debug" / filename])

    cwd = Path.cwd()
    candidates.extend([
        cwd / "build" / filename,
        cwd / "build" / "Release" / filename,
        cwd / "build" / "Debug" / filename,
        cwd / filename,
    ])
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError(f"Could not find {filename}; checked {', '.join(str(c) for c in candidates)}")


def _to_owplog_level(py_level: int) -> int:
    """Map a Python logging level integer to owplog C ABI numeric level.

    Args:
        py_level: Python ``logging`` numeric level.

    Returns:
        Closest compatible owplog numeric level.

    Raises:
        No explicit exceptions.
    """
    if py_level <= logging.DEBUG:
        return OWPLOG_DEBUG if py_level == logging.DEBUG else OWPLOG_TRACE
    if py_level <= logging.INFO:
        return OWPLOG_INFO
    if py_level <= logging.WARNING:
        return OWPLOG_WARNING
    if py_level <= logging.ERROR:
        return OWPLOG_ERROR
    if py_level <= logging.CRITICAL:
        return OWPLOG_CRITICAL
    return OWPLOG_OFF


class _OwplogCAPI:
    """Thread-safe singleton wrapper around low-level owplog C ABI functions.

    Preconditions:
        The owplog C shared library must be present and loadable.

    Postconditions:
        Once constructed, function signatures are configured and ready for calls.
    """

    _instance: Optional["_OwplogCAPI"] = None
    _instance_lock = threading.Lock()

    def __init__(self, library_path: Optional[str] = None, build_dir: Optional[str] = None) -> None:
        """Initialize the ctypes bridge and configure exported function signatures.

        Args:
            library_path: Optional explicit shared-library path.
            build_dir: Optional build directory to prioritize during lookup.

        Raises:
            FileNotFoundError: If library path cannot be resolved.
            OSError: If the shared library cannot be loaded.
        """
        self._lock = threading.Lock()
        self._lib_path = _resolve_library_path(library_path, build_dir)
        self._lib = ctypes.CDLL(str(self._lib_path))
        self._configure_signatures()

    def _configure_signatures(self) -> None:
        """Configure ctypes argtypes/restype for every used C ABI function.

        Raises:
            AttributeError: If expected symbols are missing from the shared library.
        """
        self._lib.owplog_logger_add_sublogger.argtypes = [ctypes.c_char_p]
        self._lib.owplog_logger_add_sublogger.restype = ctypes.c_int
        self._lib.owplog_logger_log.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p]
        self._lib.owplog_logger_log_sublogger.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p]
        self._lib.owplog_logger_log_ex.argtypes = [
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_void_p,
        ]

    @classmethod
    def shared(cls, library_path: Optional[str] = None, build_dir: Optional[str] = None) -> "_OwplogCAPI":
        """Return process-wide shared C API wrapper instance.

        Args:
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
                cls._instance = cls(library_path=library_path, build_dir=build_dir)
            return cls._instance

    def add_sublogger(self, sublogger_name: str) -> int:
        """Register a sublogger name and return its integer handle.

        Args:
            sublogger_name: Name to register.

        Returns:
            Positive handle on success, ``0`` on failure.

        Preconditions:
            ``sublogger_name`` should be non-empty.
        """
        with self._lock:
            return int(self._lib.owplog_logger_add_sublogger(sublogger_name.encode(TEXT_ENCODING)))

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
                _to_owplog_level(level),
                logger_name.encode(TEXT_ENCODING),
                message.encode(TEXT_ENCODING),
            )

    def log_ex(
        self,
        level: int,
        logger_name: str,
        message: str,
        file_name: Optional[str],
        line_no: int,
        function_name: Optional[str],
        language_name: Optional[str],
        rank_value: int = -1,
    ) -> None:
        """Submit one message with explicit source/language/rank metadata.

        Args:
            level: Python logging level value.
            logger_name: Logical logger name.
            message: Final message payload.
            file_name: Optional source filename.
            line_no: Source line number (`0` if unknown).
            function_name: Optional function name.
            language_name: Optional language/runtime label.
            rank_value: Optional HPC rank (`-1` if unknown).

        Postconditions:
            Metadata is passed by pointer to ``owplog_logger_log_ex`` for this call.
        """
        class OwplogLogMeta(ctypes.Structure):
            _fields_ = [
                ("file", ctypes.c_char_p),
                ("line", ctypes.c_int),
                ("function", ctypes.c_char_p),
                ("language", ctypes.c_char_p),
                ("rank", ctypes.c_int),
            ]

        meta = OwplogLogMeta(
            file_name.encode(TEXT_ENCODING) if file_name else None,
            int(line_no),
            function_name.encode(TEXT_ENCODING) if function_name else None,
            language_name.encode(TEXT_ENCODING) if language_name else None,
            int(rank_value),
        )
        with self._lock:
            self._lib.owplog_logger_log_ex(
                _to_owplog_level(level),
                logger_name.encode(TEXT_ENCODING),
                message.encode(TEXT_ENCODING),
                ctypes.byref(meta),
            )

    def log_sublogger(self, level: int, sublogger_handle: int, message: str) -> None:
        """Submit one message using a previously registered sublogger handle.

        Args:
            level: Python logging level value.
            sublogger_handle: Handle returned by ``add_sublogger``.
            message: Final message payload.
        """
        with self._lock:
            self._lib.owplog_logger_log_sublogger(
                _to_owplog_level(level),
                int(sublogger_handle),
                message.encode(TEXT_ENCODING),
            )


class OwplogHandler(logging.Handler):
    """Python ``logging.Handler`` that forwards records into owplog C ABI.

    Preconditions:
        owplog C shared library must be discoverable, unless already initialized.
    """

    def __init__(
        self,
        level: int = logging.NOTSET,
        library_path: Optional[str] = None,
        build_dir: Optional[str] = None,
        sublogger_name: Optional[str] = None,
    ) -> None:
        """Create a forwarding handler bound to the shared C API client.

        Args:
            level: Handler threshold level.
            library_path: Optional explicit shared-library path.
            build_dir: Optional build directory to search for the shared library.
            sublogger_name: Optional sublogger name to register once and reuse.

        Raises:
            FileNotFoundError: If no loadable owplog library is found during first init.
            OSError: If shared library load fails during first init.
        """
        super().__init__(level=level)
        self._client = _OwplogCAPI.shared(library_path=library_path, build_dir=build_dir)
        self._sublogger_handle = 0
        if sublogger_name:
            self._sublogger_handle = self._client.add_sublogger(sublogger_name)

    def emit(self, record: logging.LogRecord) -> None:
        """Format and forward a Python log record into owplog.

        Args:
            record: Record emitted by Python ``logging``.

        Postconditions:
            Either a sublogger-handle log call or metadata-rich ``log_ex`` call is made.

        Error behavior:
            Any exception is delegated to ``handleError(record)``.
        """
        try:
            message = self.format(record)
            if self._sublogger_handle > 0:
                self._client.log_sublogger(record.levelno, self._sublogger_handle, message)
            else:
                self._client.log_ex(
                    record.levelno,
                    record.name,
                    message,
                    getattr(record, "pathname", None),
                    int(getattr(record, "lineno", 0)),
                    getattr(record, "funcName", None),
                    "python",
                    int(getattr(record, "rank", -1)),
                )
        except Exception:
            self.handleError(record)


class OwplogLogger(logging.Logger):
    """``logging.Logger`` subclass with convenience owplog handler management."""

    def enable_owplog(
        self,
        *,
        handler_level: int = DEFAULT_PYTHON_LEVEL,
        library_path: Optional[str] = None,
        build_dir: Optional[str] = None,
        sublogger_name: Optional[str] = None,
        formatter: Optional[logging.Formatter] = None,
    ) -> OwplogHandler:
        """Attach and return an ``OwplogHandler`` unless one already exists.

        Args:
            handler_level: Threshold level for the created handler.
            library_path: Optional explicit shared-library path.
            build_dir: Optional build directory to search for shared library.
            sublogger_name: Optional C ABI sublogger registration name.
            formatter: Optional Python formatter applied to the handler.

        Returns:
            Existing or newly created ``OwplogHandler``.

        Postconditions:
            Exactly one ``OwplogHandler`` remains attached for this logger.
        """
        for handler in self.handlers:
            if isinstance(handler, OwplogHandler):
                return handler
        handler = OwplogHandler(
            level=handler_level,
            library_path=library_path,
            build_dir=build_dir,
            sublogger_name=sublogger_name,
        )
        if formatter is not None:
            handler.setFormatter(formatter)
        self.addHandler(handler)
        return handler

    def disable_owplog(self) -> None:
        """Detach and close all ``OwplogHandler`` instances from this logger.

        Postconditions:
            Logger no longer contains owplog forwarding handlers.
        """
        for handler in list(self.handlers):
            if isinstance(handler, OwplogHandler):
                self.removeHandler(handler)
                handler.close()


def owplog_handler_factory(**kwargs) -> OwplogHandler:
    """`dictConfig` factory for creating ``OwplogHandler`` instances.

    Args:
        **kwargs: Keyword args passed directly to ``OwplogHandler``.

    Returns:
        Configured ``OwplogHandler`` instance.
    """
    return OwplogHandler(**kwargs)


_previous_logger_class: Optional[type[logging.Logger]] = None


def install_owplog_logger_class() -> None:
    """Install ``OwplogLogger`` as active logger class for ``logging.getLogger``.

    Postconditions:
        New logger instances created after this call use ``OwplogLogger``.
    """
    global _previous_logger_class
    if _previous_logger_class is None:
        _previous_logger_class = logging.getLoggerClass()
    logging.setLoggerClass(OwplogLogger)


def restore_logger_class() -> None:
    """Restore the logger class active before ``install_owplog_logger_class``.

    Postconditions:
        ``logging`` logger class is restored when a previous class was saved.
    """
    global _previous_logger_class
    if _previous_logger_class is not None:
        logging.setLoggerClass(_previous_logger_class)
        _previous_logger_class = None


@contextmanager
def owplog_logger_class_installed() -> Iterator[None]:
    """Context manager that temporarily installs ``OwplogLogger``.

    Yields:
        ``None``.

    Postconditions:
        Previous logger class is restored on context exit.
    """
    install_owplog_logger_class()
    try:
        yield
    finally:
        restore_logger_class()
