IFDEF RAX

getcontext PROTO STDCALL :QWORD
setcontext PROTO STDCALL :QWORD

; ToDo: replace magic numbers with CONTEXT structure field offset calculations
offset_rip equ 248
offset_rsp equ 152

.code

; On ARM and x64 processors, __stdcall is accepted and ignored by the compiler
; On ARM and x64 architectures, by convention, arguments are passed in registers when possible

swapcontext_ PROC
	push rax ; save to
	push rcx ; save from

	mov rax, rcx
	call getcontext	; get from's context

	; correct rip
	pop rcx	; from
	lea rax, done
	mov [rcx + offset_rip], rax

	pop rax ; to

	; correct rsp
	mov [rcx + offset_rsp], rsp

	mov rcx, rax ; to
	call setcontext
done:
	ret
swapcontext_ ENDP

ENDIF

end