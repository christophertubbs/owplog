"""Python logging integration for the constrained owplog C API.
"""
import logging
import typing

from owplog_api import OWPLogAPI

# Default handler level used by OwplogLogger.enable_owplog.
DEFAULT_PYTHON_LEVEL = logging.INFO


class OwplogHandler(logging.Handler):
    """Python ``logging.Handler`` that forwards records into owplog C ABI.

    Preconditions:
        owplog C shared library must be discoverable, unless already initialized.
    """

    def __init__(
        self,
        level: int = logging.NOTSET,
        explicit_path: typing.Optional[str] = None,
        library_path: typing.Optional[str] = None,
        build_dir: typing.Optional[str] = None,
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
        self._client = OWPLogAPI.shared(
            explicit_library_path=explicit_path,
            library_path=library_path,
            build_dir=build_dir
        )

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
            self._client.log_ex(
                record.levelno,
                record.name,
                message,
                getattr(record, "pathname", None),
                int(getattr(record, "lineno", 0)),
                getattr(record, "funcName", None)
            )
        except Exception:
            self.handleError(record)
