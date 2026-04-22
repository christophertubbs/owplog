"""Minimal dictConfig-based Python example for owplog handler integration."""

import logging
import logging.config

from owplog_logger import install_owplog_logger_class


def main():
    """Configure Python logging to use owplog handler and emit one message."""
    # Install OwplogLogger subclass before building logger objects via dictConfig.
    install_owplog_logger_class()
    # Declarative logging setup that wires in the owplog handler factory.
    logging.config.dictConfig(
        {
            # Standard dictConfig version marker.
            "version": 1,
            # Keep existing third-party loggers active.
            "disable_existing_loggers": False,
            "formatters": {
                # Formatter applied by the owplog handler before forwarding.
                "standard": {"format": "%(asctime)s %(name)s %(levelname)s %(message)s"}
            },
            "handlers": {
                "owplog": {
                    # Factory path resolving to owplog_logger.owplog_handler_factory.
                    "()": "owplog_logger.owplog_handler_factory",
                    "level": "INFO",
                    # Optional C-API sublogger registration.
                    "sublogger_name": "interop.python",
                    # Formatter name defined above.
                    "formatter": "standard",
                }
            },
            "loggers": {
                "interop.python": {
                    # Attach only owplog handler to this logger.
                    "handlers": ["owplog"],
                    "level": "INFO",
                    # Prevent duplicate propagation to root handlers.
                    "propagate": False,
                }
            },
        }
    )

    # Acquire configured logger and emit one record.
    logger = logging.getLogger("interop.python")
    logger.info("hello from interop helper via dictConfig")


if __name__ == "__main__":
    main()
