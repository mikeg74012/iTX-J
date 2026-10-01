	NAME switch_to_task_functions
	
	SECTION .text:CODE
		
	; extern void task_switch_begin(uint32_t * new_stack, union task_func_u func);
		
  	EXPORT  hardware_task_switch_begin
  	EXPORT  hardware_task_switch_begin_with
//	EXPORT  safe_increment
//	EXPORT  safe_decrement
	
	EXTERN         task_ebp
	
; move stack to r0, call function pointer void(*)(void) in r1
hardware_task_switch_begin
	MOV r4, r1
	MOV r13, r0
	ORR lr, lr, #4
	BX r4
	
	
; move stack to r1, call function pointer void(*)(unsigned) in r2 with argument r0
; r0 is passed through silently
hardware_task_switch_begin_with
	MOV r4, r2
	MOV r13, r1
	ORR lr, lr, #4
	BX r4

	

/*	
safe_increment
  ldrex r1,[r0]
  add   r1,r1,#1
  strex r1,r1,[r0]
  cmp   r1,#0  ; Test if non-zero
  bne   safe_increment
  mov r0, #0
  mov pc, lr
  
safe_decrement
  ldrex r1,[r0]
  add   r1,r1,#-1
  strex r1,r1,[r0]
  cmp   r1,#0  ; Test if non-zero
  bne   safe_decrement
  mov r0, #0
  mov pc, lr
*/

	END

