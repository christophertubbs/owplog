!> @brief Minimal Fortran example using owplog_logger_mod helpers.
program fortran_interop_example
  use, intrinsic :: iso_c_binding
  use owplog_logger_mod
  implicit none
  !> Sublogger handle for this program.
  integer(c_int) :: sublogger

  ! Register sublogger and verify handle.
  sublogger = owplog_add_sublogger("interop.fortran")
  if (sublogger == 0_c_int) stop 1

  ! Emit one message through the sublogger handle.
  call owplog_log_sublogger(OWPLOG_LOG_INFO, sublogger, "hello from Fortran helper")
  call owplog_log_ex(OWPLOG_LOG_INFO, "interop.fortran", "hello with metadata", &
    file_name="fortran_example.f90", line_no=12_c_int, function_name="fortran_interop_example", &
    language_name="fortran", rank_value=-1_c_int)
end program fortran_interop_example
