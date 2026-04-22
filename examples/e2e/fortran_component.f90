subroutine fortran_component_emit_logs() bind(C, name="fortran_component_emit_logs")
  use, intrinsic :: iso_c_binding
  use owplog_logger_mod
  implicit none

  integer(c_int) :: h

  call owplog_log(OWPLOG_LOG_INFO, "e2e.fortran", "message from Fortran component")
  h = owplog_add_sublogger("e2e.fortran.sublogger")
  if (h > 0_c_int) then
    call owplog_log_sublogger(OWPLOG_LOG_WARNING, h, "warning via Fortran sublogger handle")
  end if
  call owplog_log_ex(OWPLOG_LOG_INFO, "e2e.fortran", "info with Fortran metadata", &
    file_name="fortran_component.f90", line_no=12_c_int, function_name="fortran_component_emit_logs", &
    language_name="fortran", rank_value=-1_c_int)
end subroutine fortran_component_emit_logs
