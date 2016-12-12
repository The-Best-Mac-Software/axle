[GLOBAL read_cr0]
read_cr0:
	mov eax, cr0
	retn

[GLOBAL write_cr0]
write_cr0:
	push ebp
	mov ebp, esp
	mov eax, [ebp+8]
	mov cr0, eax
	pop ebp
	retn

[GLOBAL read_cr3]
read_cr3:
	mov eax, cr3
	retn

[GLOBAL write_cr3]
write_cr3:
	push ebp
	mov ebp, esp
	mov eax, [ebp+8]
	mov cr3, eax
	pop ebp
	retn

[GLOBAL flush_cache]
flush_cache:
	mov eax, cr3
	mov cr3, eax
	retn

; .text section is around 0x10000
; enable paging in physical kernel mem
; and higher-half virtual
[EXTERN paging_enable];
[GLOBAL enter_higher_half]
enter_higher_half:
	lea ebx, [higher_half] ; load addr of label 
	jmp ebx ; set eip val to higher-half

	higher_half: ; eip is in higher half!
	call paging_enable
	retn
