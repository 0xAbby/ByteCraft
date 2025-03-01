_main:
  mov rS, 0
  mov r1, 1          ; SC_WRITE
  mov r2, 1          ; fd=stdout
  mov r3, msg        ; buf
  mov r4, 13         ; len
  syscall

  mov r1, 0          ; SC_EXIT
  mov r2, 0
  syscall

_data:
  DB msg[13]         ; zeroed buffer (e.g., fill via mov [addr], imm if desired)