!> @file owplog_logger_mod.f90
!> @brief Fortran ISO_C_BINDING helpers for owplog message submission C API.
module owplog_logger_mod
  use, intrinsic :: iso_c_binding
  implicit none

  !> TRACE log level constant.
  integer(c_int), parameter :: OWPLOG_LOG_TRACE = 0_c_int
  !> DEBUG log level constant.
  integer(c_int), parameter :: OWPLOG_LOG_DEBUG = 10_c_int
  !> INFO log level constant.
  integer(c_int), parameter :: OWPLOG_LOG_INFO = 20_c_int
  !> WARNING log level constant.
  integer(c_int), parameter :: OWPLOG_LOG_WARNING = 30_c_int
  !> ERROR log level constant.
  integer(c_int), parameter :: OWPLOG_LOG_ERROR = 40_c_int
  !> CRITICAL log level constant.
  integer(c_int), parameter :: OWPLOG_LOG_CRITICAL = 50_c_int
  !> OFF log level constant.
  integer(c_int), parameter :: OWPLOG_LOG_OFF = 60_c_int

  !> C-compatible metadata structure for extended logging calls.
  !> Pointer fields must either be `c_null_ptr` or point to null-terminated C strings.
  type, bind(C) :: owplog_log_meta_t
    type(c_ptr) :: file
    integer(c_int) :: line
    type(c_ptr) :: function
    type(c_ptr) :: language
    integer(c_int) :: rank
  end type owplog_log_meta_t

  !> Raw C ABI bindings.
  interface
    !> Register a named sublogger and return its handle.
    !> @param sublogger_name Null-terminated C string.
    !> @return Positive handle on success; 0 on failure.
    function owplog_logger_add_sublogger(sublogger_name) bind(C, name="owplog_logger_add_sublogger") result(handle)
      import :: c_char, c_int
      character(kind=c_char), dimension(*) :: sublogger_name
      integer(c_int) :: handle
    end function

    !> Emit one record using explicit logger name.
    !> @param level Numeric log level.
    !> @param logger_name Null-terminated C logger name.
    !> @param message Null-terminated C message text.
    subroutine owplog_logger_log(level, logger_name, message) bind(C, name="owplog_logger_log")
      import :: c_int, c_char
      integer(c_int), value :: level
      character(kind=c_char), dimension(*) :: logger_name
      character(kind=c_char), dimension(*) :: message
    end subroutine

    !> Emit one record using sublogger handle.
    !> @param level Numeric log level.
    !> @param sublogger_handle Handle returned by `owplog_logger_add_sublogger`.
    !> @param message Null-terminated C message text.
    subroutine owplog_logger_log_sublogger(level, sublogger_handle, message) bind(C, name="owplog_logger_log_sublogger")
      import :: c_int, c_char
      integer(c_int), value :: level
      integer(c_int), value :: sublogger_handle
      character(kind=c_char), dimension(*) :: message
    end subroutine

    !> Emit one record using explicit logger name and metadata.
    !> @param level Numeric log level.
    !> @param logger_name Null-terminated C logger name.
    !> @param message Null-terminated C message text.
    !> @param meta Metadata values; pointer fields may be null.
    subroutine owplog_logger_log_ex(level, logger_name, message, meta) bind(C, name="owplog_logger_log_ex")
      import :: c_int, c_char, owplog_log_meta_t
      integer(c_int), value :: level
      character(kind=c_char), dimension(*) :: logger_name
      character(kind=c_char), dimension(*) :: message
      type(owplog_log_meta_t), intent(in) :: meta
    end subroutine
  end interface

contains

  !> Convert Fortran character string to C null-terminated char array.
  !> @param s Input Fortran string.
  !> @return Allocatable C char array indexed from 0 with trailing `c_null_char`.
  !> @pre `s` may contain trailing blanks; they are trimmed.
  !> @post Caller receives newly allocated array.
  !> @warning Allocation failure raises a Fortran runtime error.
  function owplog_to_c_string(s) result(cs)
    character(len=*), intent(in) :: s
    character(kind=c_char), allocatable :: cs(:)
    integer :: n, i

    n = len_trim(s)
    allocate(cs(0:n))
    do i = 1, n
      cs(i - 1) = s(i:i)
    end do
    cs(n) = c_null_char
  end function owplog_to_c_string

  !> Register a sublogger and return its handle.
  !> @param sublogger_name Sublogger name in Fortran string form.
  !> @return Positive handle on success; 0 on failure.
  !> @pre `sublogger_name` should not be blank.
  !> @post Handle may be reused in `owplog_log_sublogger`.
  function owplog_add_sublogger(sublogger_name) result(handle)
    character(len=*), intent(in) :: sublogger_name
    integer(c_int) :: handle
    character(kind=c_char), allocatable :: c_name(:)

    c_name = owplog_to_c_string(sublogger_name)
    handle = owplog_logger_add_sublogger(c_name)
  end function owplog_add_sublogger

  !> Submit one log message with explicit logger name.
  !> @param level Numeric level constant (for example `OWPLOG_LOG_INFO`).
  !> @param logger_name Logical logger name.
  !> @param message Message payload.
  !> @pre `logger_name` and `message` should be valid Fortran character strings.
  !> @post Record is forwarded to upstream owplog logger or fallback stdio path.
  subroutine owplog_log(level, logger_name, message)
    integer(c_int), intent(in) :: level
    character(len=*), intent(in) :: logger_name
    character(len=*), intent(in) :: message
    character(kind=c_char), allocatable :: c_logger(:), c_msg(:)

    c_logger = owplog_to_c_string(logger_name)
    c_msg = owplog_to_c_string(message)
    call owplog_logger_log(level, c_logger, c_msg)
  end subroutine owplog_log

  !> Submit one log message with sublogger handle.
  !> @param level Numeric level constant.
  !> @param sublogger_handle Handle from `owplog_add_sublogger`.
  !> @param message Message payload.
  !> @pre None.
  !> @post Invalid handles are mapped to default logger name by C ABI implementation.
  subroutine owplog_log_sublogger(level, sublogger_handle, message)
    integer(c_int), intent(in) :: level
    integer(c_int), intent(in) :: sublogger_handle
    character(len=*), intent(in) :: message
    character(kind=c_char), allocatable :: c_msg(:)

    c_msg = owplog_to_c_string(message)
    call owplog_logger_log_sublogger(level, sublogger_handle, c_msg)
  end subroutine owplog_log_sublogger

  !> Submit one log message with explicit metadata fields.
  !> @param level Numeric level constant.
  !> @param logger_name Logical logger name.
  !> @param message Message payload.
  !> @param file_name Optional source filename.
  !> @param line_no Optional 1-based source line number.
  !> @param function_name Optional source function/procedure name.
  !> @param language_name Optional language/runtime label.
  !> @param rank_value Optional HPC rank (`-1` for unknown).
  !> @pre Optional character arguments should be valid Fortran strings when present.
  !> @post Metadata defaults are applied for absent optional arguments.
  !> @warning Allocation failure while preparing C strings raises a Fortran runtime error.
  subroutine owplog_log_ex(level, logger_name, message, file_name, line_no, function_name, language_name, rank_value)
    integer(c_int), intent(in) :: level
    character(len=*), intent(in) :: logger_name
    character(len=*), intent(in) :: message
    character(len=*), intent(in), optional :: file_name
    integer(c_int), intent(in), optional :: line_no
    character(len=*), intent(in), optional :: function_name
    character(len=*), intent(in), optional :: language_name
    integer(c_int), intent(in), optional :: rank_value
    character(kind=c_char), allocatable, target :: c_logger(:), c_msg(:), c_file(:), c_func(:), c_lang(:)
    type(owplog_log_meta_t) :: meta

    c_logger = owplog_to_c_string(logger_name)
    c_msg = owplog_to_c_string(message)

    meta%file = c_null_ptr
    meta%line = 0_c_int
    meta%function = c_null_ptr
    meta%language = c_null_ptr
    meta%rank = -1_c_int

    if (present(file_name)) then
      c_file = owplog_to_c_string(file_name)
      meta%file = c_loc(c_file(0))
    end if
    if (present(line_no)) then
      meta%line = line_no
    end if
    if (present(function_name)) then
      c_func = owplog_to_c_string(function_name)
      meta%function = c_loc(c_func(0))
    end if
    if (present(language_name)) then
      c_lang = owplog_to_c_string(language_name)
      meta%language = c_loc(c_lang(0))
    end if
    if (present(rank_value)) then
      meta%rank = rank_value
    end if

    call owplog_logger_log_ex(level, c_logger, c_msg, meta)
  end subroutine owplog_log_ex

end module owplog_logger_mod
