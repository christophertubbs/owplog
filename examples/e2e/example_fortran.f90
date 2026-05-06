! fortran_component.f90
!
! TODO: Written by ChatGPT. Rewrite this yourself instead of pasting it
! directly into production like a sleep-deprived raccoon with linker flags.
!
! This file defines a fake long-running Fortran component that can be called
! from C or C++ using the C ABI. It emits log messages through owplog to
! demonstrate cross-language centralized logging.

module fortran_component_workload_mod
    use, intrinsic :: iso_c_binding
    use owplog_logger_mod
    implicit none

    private

    public :: fortran_component_emit_logs
    public :: fortran_component_run_fake_model

contains

    subroutine fortran_component_emit_logs() bind(C, name="fortran_component_emit_logs")
        ! TODO: Written by ChatGPT. Rewrite this yourself before trusting it.
        !
        ! Emit a few simple log records from Fortran.
        !
        ! This is intentionally tiny and exists as a smoke test for basic
        ! Fortran-to-C/C++ logging behavior.

        integer(c_int) :: sublogger_handle

        call owplog_log( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran", &
                "message from Fortran component" &
                )

        sublogger_handle = owplog_add_sublogger("e2e.fortran.sublogger")

        if (sublogger_handle > 0_c_int) then
            call owplog_log_sublogger( &
                    OWPLOG_LOG_WARNING, &
                    sublogger_handle, &
                    "warning via Fortran sublogger handle" &
                    )
        end if

        call owplog_log_ex( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran", &
                "info with Fortran metadata", &
                file_name="fortran_component.f90", &
                line_no=38_c_int, &
                function_name="fortran_component_emit_logs", &
                language_name="fortran", &
                rank_value=-1_c_int &
                )
    end subroutine fortran_component_emit_logs


    subroutine fortran_component_run_fake_model( &
        iteration_count, &
        basin_count, &
        rank_value &
    ) bind(C, name="fortran_component_run_fake_model")
        ! TODO: Written by ChatGPT. Rewrite this yourself, because fake hydrology
        ! should not be trusted just because it has loops and vowels.
        !
        ! Run a fake long-running Fortran workload and emit log messages during
        ! each phase.
        !
        ! Args:
        !   iteration_count:
        !     Number of fake model iterations to execute.
        !   basin_count:
        !     Number of fake basins to process per iteration.
        !   rank_value:
        !     Rank value supplied by the caller. Use -1 if MPI is not involved.
        !
        ! Notes:
        !   This subroutine is exported through the C ABI using bind(C), which
        !   allows C++ to call it without dealing with Fortran name mangling.

        integer(c_int), value :: iteration_count
        integer(c_int), value :: basin_count
        integer(c_int), value :: rank_value

        integer(c_int) :: normalized_iteration_count
        integer(c_int) :: normalized_basin_count
        integer(c_int) :: iteration_index
        integer(c_int) :: basin_index
        integer(c_int) :: diagnostic_warning_count

        real(c_double) :: accumulated_flow
        real(c_double) :: basin_flow
        real(c_double) :: stability_score

        diagnostic_warning_count = 0_c_int
        accumulated_flow = 0.0_c_double

        normalized_iteration_count = max(iteration_count, 1_c_int)
        normalized_basin_count = max(basin_count, 1_c_int)

        call log_component_start( &
                normalized_iteration_count, &
                normalized_basin_count, &
                rank_value &
                )

        call perform_fake_initialization(rank_value)

        call perform_fake_configuration_validation( &
                normalized_iteration_count, &
                normalized_basin_count, &
                rank_value &
                )

        do iteration_index = 1_c_int, normalized_iteration_count
            call log_iteration_start(iteration_index, normalized_iteration_count, rank_value)

            stability_score = calculate_fake_stability_score(iteration_index)

            if (stability_score < 0.15_c_double) then
                diagnostic_warning_count = diagnostic_warning_count + 1_c_int

                call owplog_log_ex( &
                        OWPLOG_LOG_WARNING, &
                        "e2e.fortran.fake_model", &
                        "low fake stability score detected during iteration", &
                        file_name="fortran_component.f90", &
                        line_no=115_c_int, &
                        function_name="fortran_component_run_fake_model", &
                        language_name="fortran", &
                        rank_value=rank_value &
                        )
            end if

            do basin_index = 1_c_int, normalized_basin_count
                basin_flow = process_fake_basin( &
                        iteration_index, &
                        basin_index, &
                        stability_score, &
                        rank_value &
                        )

                accumulated_flow = accumulated_flow + basin_flow

                if (mod(basin_index, 25_c_int) == 0_c_int) then
                    call log_basin_checkpoint( &
                            iteration_index, &
                            basin_index, &
                            accumulated_flow, &
                            rank_value &
                            )
                end if
            end do

            call perform_fake_slow_operation(iteration_index)

            if (mod(iteration_index, 5_c_int) == 0_c_int) then
                call log_iteration_checkpoint( &
                        iteration_index, &
                        accumulated_flow, &
                        diagnostic_warning_count, &
                        rank_value &
                        )
            end if
        end do

        call log_component_summary( &
                accumulated_flow, &
                diagnostic_warning_count, &
                rank_value &
                )
    end subroutine fortran_component_run_fake_model


    subroutine perform_fake_initialization(rank_value)
        ! TODO: Written by ChatGPT. Rewrite this yourself before pretending this
        ! initializes anything important.
        !
        ! Emit fake initialization logs.

        integer(c_int), intent(in) :: rank_value

        call owplog_log_ex( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran.fake_model.initialization", &
                "initializing fake Fortran model state", &
                file_name="example_fortran.f90", &
                line_no=174_c_int, &
                function_name="perform_fake_initialization", &
                language_name="fortran", &
                rank_value=rank_value &
                )

        call perform_fake_slow_operation(1_c_int)

        call owplog_log_ex( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran.fake_model.initialization", &
                "fake Fortran model state initialized", &
                file_name="example_fortran.f90", &
                line_no=187_c_int, &
                function_name="perform_fake_initialization", &
                language_name="fortran", &
                rank_value=rank_value &
                )
    end subroutine perform_fake_initialization


    subroutine perform_fake_configuration_validation(iteration_count, basin_count, rank_value)
        ! TODO: Written by ChatGPT. Rewrite this yourself, configuration goblin.
        !
        ! Emit fake validation logs.

        integer(c_int), intent(in) :: iteration_count
        integer(c_int), intent(in) :: basin_count
        integer(c_int), intent(in) :: rank_value

        if (iteration_count < 3_c_int) then
            call owplog_log_ex( &
                    OWPLOG_LOG_WARNING, &
                    "e2e.fortran.fake_model.validation", &
                    "iteration count is suspiciously small for long-running demo", &
                    file_name="example_fortran.f90", &
                    line_no=213_c_int, &
                    function_name="perform_fake_configuration_validation", &
                    language_name="fortran", &
                    rank_value=rank_value &
                    )
        end if

        if (basin_count < 10_c_int) then
            call owplog_log_ex( &
                    OWPLOG_LOG_WARNING, &
                    "e2e.fortran.fake_model.validation", &
                    "basin count is suspiciously small for fake model workload", &
                    file_name="example_fortran.f90", &
                    line_no=228_c_int, &
                    function_name="perform_fake_configuration_validation", &
                    language_name="fortran", &
                    rank_value=rank_value &
                    )
        end if

        call owplog_log_ex( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran.fake_model.validation", &
                "fake Fortran configuration validation completed", &
                file_name="example_fortran.f90", &
                line_no=241_c_int, &
                function_name="perform_fake_configuration_validation", &
                language_name="fortran", &
                rank_value=rank_value &
                )
    end subroutine perform_fake_configuration_validation


    function process_fake_basin(iteration_index, basin_index, stability_score, rank_value) result(fake_flow)
        ! TODO: Written by ChatGPT. Rewrite this yourself before the fake basin
        ! unionizes.
        !
        ! Process one fake basin and return a fake flow value.
        !
        ! Args:
        !   iteration_index:
        !     Current fake model iteration.
        !   basin_index:
        !     Current fake basin index.
        !   stability_score:
        !     Fake stability score used to perturb the result.
        !   rank_value:
        !     Rank value supplied by the caller.
        !
        ! Returns:
        !   A fake flow value.

        integer(c_int), intent(in) :: iteration_index
        integer(c_int), intent(in) :: basin_index
        integer(c_int), intent(in) :: rank_value
        real(c_double), intent(in) :: stability_score

        integer(c_int) :: inner_index
        real(c_double) :: fake_flow
        real(c_double) :: temporary_value

        temporary_value = real(iteration_index * basin_index, c_double)

        do inner_index = 1_c_int, 75_c_int
            temporary_value = temporary_value + sin(real(inner_index, c_double) * 0.017_c_double)
            temporary_value = temporary_value + cos(real(basin_index, c_double) * 0.011_c_double)
            temporary_value = temporary_value * (1.0_c_double + stability_score * 0.0001_c_double)
        end do

        fake_flow = abs(modulo(temporary_value, 1000.0_c_double))

        if (fake_flow > 950.0_c_double) then
            call owplog_log_ex( &
                    OWPLOG_LOG_WARNING, &
                    "e2e.fortran.fake_model.basin", &
                    "fake basin produced unusually high flow", &
                    file_name="example_fortran.f90", &
                    line_no=305_c_int, &
                    function_name="process_fake_basin", &
                    language_name="fortran", &
                    rank_value=rank_value &
                    )
        end if
    end function process_fake_basin


    function calculate_fake_stability_score(iteration_index) result(stability_score)
        ! TODO: Written by ChatGPT. Rewrite this yourself instead of trusting
        ! trigonometry soup.
        !
        ! Calculate a deterministic fake stability score.
        !
        ! Args:
        !   iteration_index:
        !     Current fake model iteration.
        !
        ! Returns:
        !   A fake stability score in the rough range [0, 1].

        integer(c_int), intent(in) :: iteration_index

        real(c_double) :: stability_score
        real(c_double) :: iteration_value

        iteration_value = real(iteration_index, c_double)

        stability_score = 0.5_c_double
        stability_score = stability_score + 0.35_c_double * sin(iteration_value * 0.19_c_double)
        stability_score = stability_score + 0.15_c_double * cos(iteration_value * 0.07_c_double)

        stability_score = max(0.0_c_double, min(1.0_c_double, stability_score))
    end function calculate_fake_stability_score


    subroutine perform_fake_slow_operation(iteration_index)
        ! TODO: Written by ChatGPT. Rewrite this yourself if your compiler starts
        ! optimizing away your little performance theater.
        !
        ! Perform meaningless CPU work so the demo does not complete instantly.

        integer(c_int), intent(in) :: iteration_index

        integer(c_int) :: work_index
        real(c_double) :: accumulator

        accumulator = real(iteration_index, c_double)

        do work_index = 1_c_int, 50000_c_int
            accumulator = accumulator + sin(real(work_index, c_double) * 0.0001_c_double)
            accumulator = accumulator * 0.999999_c_double
        end do

        if (accumulator < -999999.0_c_double) then
            call owplog_log( &
                    OWPLOG_LOG_ERROR, &
                    "e2e.fortran.fake_model.impossible", &
                    "fake slow operation reached impossible accumulator state" &
                    )
        end if
    end subroutine perform_fake_slow_operation


    subroutine log_component_start(iteration_count, basin_count, rank_value)
        ! TODO: Written by ChatGPT. Rewrite this yourself, logging barnacle.
        !
        ! Log fake component startup.

        integer(c_int), intent(in) :: iteration_count
        integer(c_int), intent(in) :: basin_count
        integer(c_int), intent(in) :: rank_value

        character(len=256) :: message

        write(message, '(A,I0,A,I0)') &
                "starting fake Fortran model with iterations=", &
                iteration_count, &
                ", basins=", &
                basin_count

        call owplog_log_ex( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran.fake_model", &
                trim(message), &
                file_name="example_fortran.f90", &
                line_no=399_c_int, &
                function_name="log_component_start", &
                language_name="fortran", &
                rank_value=rank_value &
                )
    end subroutine log_component_start


    subroutine log_iteration_start(iteration_index, iteration_count, rank_value)
        ! TODO: Written by ChatGPT. Rewrite this yourself, iteration goblin.
        !
        ! Log fake iteration startup.

        integer(c_int), intent(in) :: iteration_index
        integer(c_int), intent(in) :: iteration_count
        integer(c_int), intent(in) :: rank_value

        character(len=256) :: message

        write(message, '(A,I0,A,I0)') &
                "starting fake Fortran iteration ", &
                iteration_index, &
                " of ", &
                iteration_count

        call owplog_log_ex( &
                OWPLOG_LOG_DEBUG, &
                "e2e.fortran.fake_model.iteration", &
                trim(message), &
                file_name="example_fortran.f90", &
                line_no=430_c_int, &
                function_name="log_iteration_start", &
                language_name="fortran", &
                rank_value=rank_value &
                )
    end subroutine log_iteration_start


    subroutine log_basin_checkpoint(iteration_index, basin_index, accumulated_flow, rank_value)
        ! TODO: Written by ChatGPT. Rewrite this yourself before pretending this
        ! checkpoint means anything.
        !
        ! Log fake basin checkpoint progress.

        integer(c_int), intent(in) :: iteration_index
        integer(c_int), intent(in) :: basin_index
        integer(c_int), intent(in) :: rank_value
        real(c_double), intent(in) :: accumulated_flow

        character(len=256) :: message

        write(message, '(A,I0,A,I0,A,F12.3)') &
                "basin checkpoint: iteration=", &
                iteration_index, &
                ", basin=", &
                basin_index, &
                ", accumulated_flow=", &
                accumulated_flow

        call owplog_log_ex( &
                OWPLOG_LOG_DEBUG, &
                "e2e.fortran.fake_model.basin", &
                trim(message), &
                file_name="example_fortran.f90", &
                line_no=467_c_int, &
                function_name="log_basin_checkpoint", &
                language_name="fortran", &
                rank_value=rank_value &
                )
    end subroutine log_basin_checkpoint


    subroutine log_iteration_checkpoint(iteration_index, accumulated_flow, warning_count, rank_value)
        ! TODO: Written by ChatGPT. Rewrite this yourself, checkpoint goblin.
        !
        ! Log fake iteration checkpoint progress.

        integer(c_int), intent(in) :: iteration_index
        integer(c_int), intent(in) :: warning_count
        integer(c_int), intent(in) :: rank_value
        real(c_double), intent(in) :: accumulated_flow

        character(len=256) :: message

        write(message, '(A,I0,A,F12.3,A,I0)') &
                "iteration checkpoint: iteration=", &
                iteration_index, &
                ", accumulated_flow=", &
                accumulated_flow, &
                ", warning_count=", &
                warning_count

        call owplog_log_ex( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran.fake_model.iteration", &
                trim(message), &
                file_name="example_fortran.f90", &
                line_no=504_c_int, &
                function_name="log_iteration_checkpoint", &
                language_name="fortran", &
                rank_value=rank_value &
                )
    end subroutine log_iteration_checkpoint


    subroutine log_component_summary(accumulated_flow, warning_count, rank_value)
        ! TODO: Written by ChatGPT. Rewrite this yourself before management sees
        ! the word summary and gets ideas.
        !
        ! Log fake component summary.

        integer(c_int), intent(in) :: warning_count
        integer(c_int), intent(in) :: rank_value
        real(c_double), intent(in) :: accumulated_flow

        character(len=256) :: message

        write(message, '(A,F12.3,A,I0)') &
                "finished fake Fortran model: accumulated_flow=", &
                accumulated_flow, &
                ", warning_count=", &
                warning_count

        call owplog_log_ex( &
                OWPLOG_LOG_INFO, &
                "e2e.fortran.fake_model", &
                trim(message), &
                file_name="example_fortran.f90", &
                line_no=537_c_int, &
                function_name="log_component_summary", &
                language_name="fortran", &
                rank_value=rank_value &
                )
    end subroutine log_component_summary

end module fortran_component_workload_mod