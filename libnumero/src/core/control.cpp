#include "stdafx.h"

#include "corecalc.h"
#include "control.h"
#include "device.h"
#include "alu.h"

#pragma warning(push)
// byte conversion
#pragma warning(disable : 4244)
// uninit var
#pragma warning(disable : 4701)

//-----------------------------
// ED OPCODES

int ednop(CPU_t *) {
	return 8;
}

int ldd(CPU_t *cpu) {
	int tmp;
	int reg;
	
	reg = CPU_mem_read(cpu,cpu->hl);
	CPU_mem_write(cpu,cpu->de,reg);
	tmp = cpu->a + reg;
	cpu->bc--;
	cpu->hl--;
	cpu->de--;
	cpu->f = dox5((tmp&2)!=0) + dox3((tmp&8)!=0) + 
		 doparity(cpu->bc!=0) + 
		 unaffect(SIGN_MASK + ZERO_MASK + CARRY_MASK);

	return 16;
}

int lddr(CPU_t *cpu) {
	int tmp;
	int reg;
	
	reg = CPU_mem_read(cpu,cpu->hl);
	CPU_mem_write(cpu,cpu->de,reg);
	tmp = cpu->a + reg;
	cpu->bc--;
	cpu->hl--;
	cpu->de--;
	cpu->f = dox5((tmp&2)!=0) + dox3((tmp&8)!=0) + 
		 doparity(cpu->bc!=0) + 
		 unaffect(SIGN_MASK + ZERO_MASK + CARRY_MASK);
	if (cpu->bc!=0) {
		cpu->pc -=2;
		return 21;
	}

	return 16;
}
int ldi(CPU_t *cpu) {
	int tmp;
	int reg;
	
	reg = CPU_mem_read(cpu,cpu->hl);
	CPU_mem_write(cpu,cpu->de,reg);
	tmp = cpu->a + reg;
	cpu->bc--;
	cpu->hl++;
	cpu->de++;
	cpu->f = dox5((tmp&2)!=0) + dox3((tmp&8)!=0) + 
		 doparity(cpu->bc!=0) + 
		 unaffect(SIGN_MASK + ZERO_MASK + CARRY_MASK);
	return 16;
}

int ldir(CPU_t *cpu) {
	int tmp;
	int reg;
	
	reg = CPU_mem_read(cpu,cpu->hl);
	CPU_mem_write(cpu,cpu->de,reg);
	tmp = cpu->a + reg;
	cpu->bc--;
	cpu->hl++;
	cpu->de++;
	cpu->f = dox5((tmp&2)!=0) + dox3((tmp&8)!=0) + 
		 doparity(cpu->bc!=0) + 
		 unaffect(SIGN_MASK + ZERO_MASK + CARRY_MASK);
	if (cpu->bc!=0) {
		cpu->pc -=2;
		return 21;
	}

	return 16;
}

int ld_mem16_reg16(CPU_t *cpu) {
	int result;
	int test = (cpu->bus >> 4) & 3;
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc++) << 8;

	switch (test) {
		case 00:
			result = cpu->bc;
			break;
		case 01:
			result = cpu->de;
			break;
		case 02:
			result = cpu->hl;
			break;
		case 03:
			result = cpu->sp;
			break;
	}
	CPU_mem_write(cpu, address++, result & 0xFF);
	CPU_mem_write(cpu, address, result >> 8);
	return 20;
}

int ld_reg16_mem16(CPU_t *cpu) {
	int test = (cpu->bus >> 4) & 3;
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc++) << 8;

	unsigned short result = CPU_mem_read(cpu, address);
	result |= CPU_mem_read(cpu, address + 1) << 8;

	switch (test) {
		case 00:
			cpu->bc = result;
			break;
		case 01:
			cpu->de = result;
			break;
		case 02:
			cpu->hl = result;
			break;
		case 03:
			cpu->sp = result;
			break;
	}
	return 20;
}

int IM0(CPU_t *cpu) {
	cpu->imode = 0;
	return 8;
}
int IM1(CPU_t *cpu) {
	cpu->imode = 1;
	return 8;
}
int IM2(CPU_t *cpu) {
	cpu->imode = 2;
	return 8;
}

int in_reg_c(CPU_t *cpu) {
	int test = (cpu->bus>>3)&7;
	device_input(cpu, cpu->c);
	switch(test) {
		case 0x00:
			cpu->b=cpu->bus;
			break;
		case 0x01:
			cpu->c=cpu->bus;
			break;
		case 0x02:
			cpu->d=cpu->bus;
			break;
		case 0x03:
			cpu->e=cpu->bus;
			break;
		case 0x04:
			cpu->h=cpu->bus;
			break;
		case 0x05:
			cpu->l=cpu->bus;
			break;
		case 0x06:
			break;
		case 0x07:
			cpu->a=cpu->bus;
			break;
	}
	cpu->f = signchk(cpu->bus) + zerochk(cpu->bus) +
		 x5chk(cpu->bus) + x3chk(cpu->bus) + 
		 parity(cpu->bus) + unaffect(CARRY_MASK);
	return 12;
}


int ind(CPU_t *cpu) {
	int result,tmp;
	device_input(cpu, cpu->c);
	result = cpu->bus;
	CPU_mem_write(cpu,cpu->hl,cpu->bus);
	cpu->b--;
	cpu->hl--;
	tmp = result+ ((cpu->c-1) & 255);
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&7)^cpu->b) + 
		 (((result&128)!=0)? N_MASK:0) +
		 carry( tmp>255 ); 
	return 16;
}

int indr(CPU_t *cpu) {
	int result,tmp;
	device_input(cpu, cpu->c);
	result = cpu->bus;
	CPU_mem_write(cpu,cpu->hl,cpu->bus);
	cpu->b--;
	cpu->hl--;
	tmp = result+ ((cpu->c-1) & 255);
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&7)^cpu->b) + 
		 (((result&128)!=0)? N_MASK:0) +
		 carry( tmp>255 );
	if (cpu->b!=0) {
		cpu->pc -=2;
		return 21;
	}

	return 16;
}
int ini(CPU_t *cpu) {
	int result,tmp;
	
	device_input(cpu, cpu->c);
	result = cpu->bus;
	CPU_mem_write(cpu,cpu->hl,cpu->bus);
	cpu->b--;
	cpu->hl++;
	tmp = result+ ((cpu->c+1) & 255);
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&7)^cpu->b) + 
		 (((result&128)!=0)? N_MASK:0) +
		 carry( tmp>255 ); 
	return 16;
}

int inir(CPU_t *cpu) {
	int result,tmp;
	
	device_input(cpu, cpu->c);
	result = cpu->bus;
	CPU_mem_write(cpu,cpu->hl,cpu->bus);
	cpu->b--;
	cpu->hl++;
	tmp = result+ ((cpu->c+1) & 255);
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&7)^cpu->b) + 
		 (((result&128)!=0)? N_MASK:0) +
		 carry( tmp>255 );
	if (cpu->b!=0) {
		cpu->pc -=2;
		return 21;
	}

	return 16;
}

int ld_i_a(CPU_t *cpu) {
	cpu->i = cpu->a;
	return 9;
}
int ld_r_a(CPU_t *cpu) {
	cpu->r = cpu->a;
	return 9;
}
int ld_a_i(CPU_t *cpu) {
	cpu->a = cpu->i;
	cpu->f = signchk(cpu->a) + zerochk(cpu->a) +
		 x5chk(cpu->a) + x3chk(cpu->a) + 
		 doparity(cpu->iff2!=0) + unaffect(CARRY_MASK);
	return 9;
}
int ld_a_r(CPU_t *cpu) {
	cpu->a = cpu->r;
	cpu->f = signchk(cpu->a) + zerochk(cpu->a) +
		 x5chk(cpu->a) + x3chk(cpu->a) + 
		 doparity(cpu->iff2!=0) + unaffect(CARRY_MASK);
	return 9;
}


int out_reg(CPU_t *cpu) { 
	 int test = (cpu->bus>>3)&7; 
	 switch(test) { 
		  case 0x00: 
			   cpu->bus=cpu->b; 
			   break; 
		  case 0x01: 
			   cpu->bus=cpu->c; 
			   break; 
		  case 0x02: 
			   cpu->bus=cpu->d; 
			   break; 
		  case 0x03: 
			   cpu->bus=cpu->e; 
			   break; 
		  case 0x04: 
			   cpu->bus=cpu->h; 
			   break; 
		  case 0x05: 
			   cpu->bus=cpu->l; 
			   break; 
		  case 0x06: 
			   cpu->bus=0xFF; 
			   break; 
		  case 0x07: 
			   cpu->bus=cpu->a; 
			   break; 
	 } 
	 device_output(cpu, cpu->c); 
	 return 12;
}



int outd(CPU_t *cpu) {
	int result,tmp;

	result=CPU_mem_read(cpu,cpu->hl);
	device_output(cpu, cpu->c);
	cpu->b--;
	cpu->hl--;
	tmp = result+ cpu->l;
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&0x07)^cpu->b) + 
		 (((result&0x80)!=0)? N_MASK:0) +
		 carry( tmp>255 ); 
	return 16;
}

int otdr(CPU_t *cpu) {
	int result,tmp;
	
	result=CPU_mem_read(cpu,cpu->hl);
	device_output(cpu, cpu->c);
	cpu->b--;
	cpu->hl--;
	tmp = result+ cpu->l;
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&0x07)^cpu->b) + 
		 (((result&0x80)!=0)? N_MASK:0) +
		 carry( tmp>255 ); 
	if (cpu->b!=0) {
		cpu->pc -=2;
		return 21;
	}

	return 16;
}
int outi(CPU_t *cpu) {
	int result,tmp;
	
	result=CPU_mem_read(cpu,cpu->hl);
	device_output(cpu, cpu->c);
	cpu->b--;
	cpu->hl++;
	tmp = result+ cpu->l;
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&0x07)^cpu->b) + 
		 (((result&0x80)!=0)? N_MASK:0) +
		 carry( tmp>255 ); 
	return 16;
}

int otir(CPU_t *cpu) {
	int result,tmp;
	
	result=CPU_mem_read(cpu,cpu->hl);
	device_output(cpu, cpu->c);
	cpu->b--;
	cpu->hl++;
	tmp = result+ cpu->l;
	cpu->f = signchk(cpu->b) + zerochk(cpu->b) +
		 x5chk(cpu->b) + dohc( tmp>255 ) + 
		 x3chk(cpu->b) +  parity((tmp&0x07)^cpu->b) + 
		 (((result&0x80)!=0)? N_MASK:0) +
		 carry( tmp>255 ); 
	if (cpu->b!=0) {
		cpu->pc -=2;
		return 21;
	}

	return 16;
}

int reti(CPU_t *cpu) {
	cpu->pc = CPU_mem_read(cpu, cpu->sp++);
	cpu->pc |= CPU_mem_read(cpu, cpu->sp++) << 8;
	cpu->iff1 = cpu->iff2;
	return 14;
}
int retn(CPU_t *cpu) {
	cpu->pc = CPU_mem_read(cpu, cpu->sp++);
	cpu->pc |= CPU_mem_read(cpu, cpu->sp++) << 8;
	cpu->iff1 = cpu->iff2;
	return 14;
}

//
//----------------


//-----------------
// SCF
int scf(CPU_t *cpu) {
	cpu->f = unaffect( SIGN_MASK+ZERO_MASK+PV_MASK) +
		 x5chk(cpu->a) + x3chk(cpu->a) + CARRY_MASK;
	return 4;
}
//-----------------
// CCF
int ccf(CPU_t *cpu) {
	cpu->f = unaffect( SIGN_MASK+ZERO_MASK+PV_MASK) +
		 x5chk(cpu->a|cpu->f) + dohc( (cpu->f&CARRY_MASK)!=0 ) + 
		 x3chk(cpu->a|cpu->f) + ((cpu->f&CARRY_MASK)^CARRY_MASK);
	return 4;
}
int rst(CPU_t *cpu) {
	int reg = (cpu->bus&0x38);
	CPU_mem_write(cpu,--cpu->sp,(cpu->pc>>8)&0xFF);
	CPU_mem_write(cpu,--cpu->sp,cpu->pc&0xFF);
	cpu->pc = reg;
	return 11;
}

int out(CPU_t *cpu) {
	int port = CPU_mem_read(cpu, cpu->pc++);
	cpu->bus = cpu->a;
	device_output(cpu, port);
	return 11;
}
int in(CPU_t *cpu) {
	int port = CPU_mem_read(cpu, cpu->pc++);
	device_input(cpu, port);
	cpu->a = cpu->bus;
	return 11;
}

int ret(CPU_t *cpu) {
	cpu->pc = CPU_mem_read(cpu, cpu->sp++);
	cpu->pc |= CPU_mem_read(cpu, cpu->sp++) << 8;
	return 10;
}
int ret_condition(CPU_t *cpu) {
	int succeed = FALSE;
	switch ((cpu->bus >> 3) & 0x07) {
		case 0:	if (!(cpu->f & ZERO_MASK)) succeed = TRUE;
				break;
		case 1:	if ((cpu->f & ZERO_MASK)) succeed = TRUE;
				break;
		case 2:	if (!(cpu->f & CARRY_MASK)) succeed = TRUE;
				break;
		case 3:	if ((cpu->f & CARRY_MASK)) succeed = TRUE;
				break;
		case 4:	if (!(cpu->f & PV_MASK)) succeed = TRUE;
				break;
		case 5:	if ((cpu->f & PV_MASK)) succeed = TRUE;
				break;
		case 6:	if (!(cpu->f & SIGN_MASK)) succeed = TRUE;
				break;
		case 7:	if ((cpu->f & SIGN_MASK)) succeed = TRUE;
				break;
	}
	if (succeed == TRUE) {
		cpu->pc = CPU_mem_read(cpu,cpu->sp++);
		cpu->pc |= CPU_mem_read(cpu, cpu->sp++) << 8;
		return 11;
	}

	return 5;
}
int call(CPU_t *cpu) {
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc++) << 8;
	
	CPU_mem_write(cpu, --cpu->sp, cpu->pc >> 8);
	CPU_mem_write(cpu, --cpu->sp, cpu->pc & 0xFF);
	cpu->pc = address;
	return 17;
}

int call_condition(CPU_t *cpu) {
	int succeed = FALSE;
	int condition = (cpu->bus >> 3) & 0x07;
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc++) << 8;
	
	switch (condition) {
		case 0:	if (!(cpu->f & ZERO_MASK)) succeed = TRUE;
				break;
		case 1:	if ((cpu->f & ZERO_MASK)) succeed = TRUE;
				break;
		case 2:	if (!(cpu->f & CARRY_MASK)) succeed = TRUE;
				break;
		case 3:	if ((cpu->f & CARRY_MASK)) succeed = TRUE;
				break;
		case 4:	if (!(cpu->f & PV_MASK)) succeed = TRUE;
				break;
		case 5:	if ((cpu->f & PV_MASK)) succeed = TRUE;
				break;
		case 6:	if (!(cpu->f & SIGN_MASK)) succeed = TRUE;
				break;
		case 7:	if ((cpu->f & SIGN_MASK)) succeed = TRUE;
				break;
	}

	if (succeed) {
		CPU_mem_write(cpu, --cpu->sp, cpu->pc >> 8);
		CPU_mem_write(cpu, --cpu->sp, cpu->pc & 0xFF);
		cpu->pc = address;
		return 17;
	}

	return 10;
}

int push_reg16(CPU_t *cpu) {
	unsigned short reg;
	int time = 11;
	
	switch ((cpu->bus >> 4) & 0x03) {
		case 0:
			reg = cpu->bc;
			break;
		case 1:
			reg = cpu->de;
			break;
		case 2:
			if (!cpu->prefix) {
				reg = cpu->hl;
			} else if (cpu->prefix == 0xDD) {
				reg = cpu->ix;
				time +=4;
			} else {
				reg = cpu->iy;
				time +=4;
			}
			break;
		case 3:
			reg = cpu->af;
			break;
	}
	CPU_mem_write(cpu, --cpu->sp, reg >> 8);
	CPU_mem_write(cpu, --cpu->sp, reg & 0xFF);
	return time;
}

int pop_reg16(CPU_t *cpu) {
	int old_bus = cpu->bus;
	/* Back up the bus, since memory reads will write to it as well */
	unsigned short reg = CPU_mem_read(cpu, cpu->sp++);
	reg |= CPU_mem_read(cpu, cpu->sp++) << 8;

	int time = 10;
	
	switch ( (old_bus>>4) & 0x03) {
		case 0:
			cpu->bc = reg;
			break;
		case 1:
			cpu->de = reg;
			break;
		case 2:
			if (!cpu->prefix) {
				cpu->hl = reg;
			} else if (cpu->prefix == 0xDD) {
				cpu->ix = reg;
				time +=4;
			} else {
				cpu->iy = reg;
				time +=4;
			}
			break;
		case 3:
			cpu->af = reg;
			break;
	}
	return time;
}

int ld_sp_hl(CPU_t *cpu) {
	if (!cpu->prefix) {
		cpu->sp = cpu->hl;
		return 6;
	} else if (cpu->prefix == 0xDD) {
		cpu->sp = cpu->ix;
		return 10;
	} else {
		cpu->sp = cpu->iy;
		return 10;
	}
}
int ld_mem16_hlf(CPU_t *cpu) {
	unsigned short reg = CPU_mem_read(cpu, cpu->pc++);
	reg |= CPU_mem_read(cpu, cpu->pc++) << 8;

	if (!cpu->prefix) {
		CPU_mem_write(cpu, reg++, cpu->l);
		CPU_mem_write(cpu, reg, cpu->h);
		return 16;
	} else if (cpu->prefix == 0xDD) {
		CPU_mem_write(cpu, reg++, cpu->ixl);
		CPU_mem_write(cpu, reg, cpu->ixh);
		return 20;
	} else {
		CPU_mem_write(cpu, reg++, cpu->iyl);
		CPU_mem_write(cpu, reg, cpu->iyh);
		return 20;
	}		
}

int ld_hlf_mem16(CPU_t *cpu) {
	unsigned short mem = CPU_mem_read(cpu, cpu->pc++);
	mem |= CPU_mem_read(cpu, cpu->pc++) << 8;
	unsigned short reg = CPU_mem_read(cpu, mem);
	reg |= CPU_mem_read(cpu, mem + 1) << 8;
	if (!cpu->prefix) {
		cpu->hl = reg;
		return 16;
	} else if (cpu->prefix == 0xDD) {
		cpu->ix = reg;
		return 20;
	} else {
		cpu->iy = reg;
		return 20;
	}		
}

int ld_hl_num16(CPU_t *cpu) {
	unsigned short reg = CPU_mem_read(cpu, cpu->pc++);
	reg |= CPU_mem_read(cpu, cpu->pc++) << 8;

	if (!cpu->prefix) {
		cpu->hl = reg;
		return 10;
	} else if (cpu->prefix == 0xDD) {
		cpu->ix = reg;
		return 14;
	} else {
		cpu->iy = reg;
		return 14;
	}	
}
int ld_de_num16(CPU_t *cpu) {
	cpu->de = CPU_mem_read(cpu, cpu->pc++);
	cpu->de |= CPU_mem_read(cpu, cpu->pc++) << 8;
	return 10;
}
int ld_bc_num16(CPU_t *cpu) {
	cpu->bc = CPU_mem_read(cpu, cpu->pc++);
	cpu->bc |= CPU_mem_read(cpu, cpu->pc++) << 8;
	return 10;
}
int ld_sp_num16(CPU_t *cpu) {
	cpu->sp = CPU_mem_read(cpu, cpu->pc++);
	cpu->sp |= CPU_mem_read(cpu, cpu->pc++) << 8;
	return 10;
}
int ld_a_mem16(CPU_t *cpu) {
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc++) << 8;
	cpu->a = CPU_mem_read(cpu, address);
	return 13;
}
int ld_a_bc(CPU_t *cpu) {
	cpu->a = CPU_mem_read(cpu, cpu->bc);
	return 7;
}
int ld_a_de(CPU_t *cpu) {
	cpu->a = CPU_mem_read(cpu,cpu->de);
	return 7;
}
int ld_mem16_a(CPU_t *cpu) {
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc++) << 8;
	CPU_mem_write(cpu, address, cpu->a);
	return 13;
}
int ld_bc_a(CPU_t *cpu) {
	CPU_mem_write(cpu, cpu->bc, cpu->a);
	return 7;
}
int ld_de_a(CPU_t *cpu) {
	CPU_mem_write(cpu, cpu->de, cpu->a);
	return 7;
}

int ld_r_num8(CPU_t *cpu) {
	int reg;
	int test = (cpu->bus>>3)&7;
	int time = 7;

	reg = CPU_mem_read(cpu, cpu->pc++);
	switch(test) {
		case 0x00:
			cpu->b=reg;
			break;
		case 0x01:
			cpu->c=reg;
			break;
		case 0x02:
			cpu->d=reg;
			break;
		case 0x03:
			cpu->e=reg;
			break;
		case 0x04:
			index_ext (
				cpu->h = reg;,
				cpu->ixh = reg; time += 4;,
				cpu->iyh = reg; time += 4;
			)
			break;
		case 0x05:
			index_ext (
				cpu->l = reg;,
				cpu->ixl = reg; time += 4;,
				cpu->iyl = reg; time += 4;
			)
			break;
		case 0x06:
			if (!cpu->prefix) {
				CPU_mem_write(cpu,cpu->hl,reg);
				time += 3;
			} else {
				char offset = reg;
				reg = CPU_mem_read(cpu, cpu->pc++);
				if (cpu->prefix == 0xDD) {
					CPU_mem_write(cpu, cpu->ix + offset, reg);
				} else {
					CPU_mem_write(cpu, cpu->iy + offset, reg);
				}

				time += 12;
			}
			break;
		case 0x07:
			cpu->a=reg;
			break;
	}

	return time;
}

int ld_r_r(CPU_t *cpu) {
	int reg;
	int test = (cpu->bus>>3)&7;
	int test2 = cpu->bus&7;
	int time = 4;

	switch(test2) {
		case 0x00:
			reg=cpu->b;
			break;
		case 0x01:
			reg=cpu->c;
			break;
		case 0x02:
			reg=cpu->d;
			break;
		case 0x03:
			reg=cpu->e;
			break;
		case 0x04:
			if (cpu->prefix && test == 6) {
				reg = cpu->h;
			} else {
				index_ext (
					reg = cpu->h;,
					reg = cpu->ixh; time += 4; ,
					reg = cpu->iyh; time += 4;
				)
			}
			break;
		case 0x05:
			if (cpu->prefix && test == 6) {
				reg = cpu->l;
				break;
			}
			index_ext (
				reg = cpu->l;,
				reg = cpu->ixl; time += 4;,
				reg = cpu->iyl; time += 4;
			)
			break;
		case 0x06:
			if (!cpu->prefix) {
				reg = CPU_mem_read(cpu,cpu->hl);
				time += 3;
			} else {
				char offset = CPU_mem_read(cpu, cpu->pc++);
				if (cpu->prefix == 0xDD) {
					reg = CPU_mem_read(cpu, cpu->ix + offset);
				} else {
					reg = CPU_mem_read(cpu, cpu->iy + offset);
				}

				time += 15;
			}
			break;
		case 0x07:
			reg=cpu->a;
			break;
	}
	
	switch(test) {
		case 0x00:
			cpu->b=reg;
			break;
		case 0x01:
			cpu->c=reg;
			break;
		case 0x02:
			cpu->d=reg;
			break;
		case 0x03:
			cpu->e=reg;
			break;
		case 0x04:
			if (cpu->prefix && test2 == 6) {
				cpu->h = reg;
				break;
			}
			index_ext (
				cpu->h = reg;,
				cpu->ixh = reg; time += 4;,
				cpu->iyh = reg; time += 4;
			)
			break;
		case 0x05:
			if (cpu->prefix && test2 == 6) {
				cpu->l = reg;
				break;
			}
			index_ext (
				cpu->l = reg;,
				cpu->ixl = reg; time += 4;,
				cpu->iyl = reg; time += 4;
			)
			break;
		case 0x06:
			if (!cpu->prefix) {
				CPU_mem_write(cpu,cpu->hl,reg);
				time += 3;
			} else {
				char offset = CPU_mem_read(cpu, cpu->pc++);
				if (cpu->prefix == 0xDD) {
					CPU_mem_write(cpu, cpu->ix + offset, reg);
				} else {
					CPU_mem_write(cpu, cpu->iy + offset, reg);
				}
				
				time += 15;
			}
			break;
		case 0x07:
			cpu->a=reg;
			break;
	}

	return time;
}

int halt(CPU_t *cpu) {
	cpu->halt = TRUE;
//	cpu->pc--;			//I'll tell you later...just to sync up.
	return 4;
}

int nop(CPU_t *) {
	return 4;
}

int ex_sp_hl(CPU_t *cpu) {
	unsigned short reg = CPU_mem_read(cpu, cpu->sp);
	reg |= CPU_mem_read(cpu, cpu->sp + 1) << 8;
	
	if (!cpu->prefix) {
		CPU_mem_write(cpu,cpu->sp + 1, cpu->h);
		CPU_mem_write(cpu,cpu->sp, cpu->l);
		cpu->hl = reg;
		return 19;
	} else {
		if (cpu->prefix == 0xDD) {
			CPU_mem_write(cpu,cpu->sp + 1, cpu->ixh);
			CPU_mem_write(cpu,cpu->sp, cpu->ixl);
			cpu->ix = reg;
		} else {
			CPU_mem_write(cpu,cpu->sp + 1, cpu->iyh);
			CPU_mem_write(cpu,cpu->sp, cpu->iyl);
			cpu->iy = reg;
		}

		return 23;
	}
}







#define swappair( regpair1 , regpair2 ) \
reg = regpair1; \
regpair1 = regpair2; \
regpair2 = reg;

int ex_de_hl(CPU_t *cpu) {
	unsigned short reg;
	swappair(cpu->hl,cpu->de);
	return 4;
}

int exx(CPU_t *cpu) {
	unsigned short reg;
	swappair(cpu->hl,cpu->hlp);
	swappair(cpu->bc,cpu->bcp);
	swappair(cpu->de,cpu->dep);
	return 4;
}

int ex_af_afp(CPU_t *cpu) {
	unsigned short reg;
	swappair(cpu->af,cpu->afp);
	return 4;
}	

int jp_hl(CPU_t *cpu) {
	index_ext (
		cpu->pc = cpu->hl;
		return 4;,
		
		cpu->pc = cpu->ix;
		return 8;,

		cpu->pc = cpu->iy;
		return 8;
	)	
}
int jp(CPU_t *cpu) {
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc) << 8;

	cpu->pc = address;
	return 10;
}

int jr(CPU_t *cpu) {
	char reg = (char) CPU_mem_read(cpu, cpu->pc++);
	cpu->pc = addschar(cpu->pc, reg);
	return 12;
}




int jp_condition(CPU_t *cpu) {
	int condition = (cpu->bus>>3)&7;
	unsigned short address = CPU_mem_read(cpu, cpu->pc++);
	address |= CPU_mem_read(cpu, cpu->pc++) << 8;

	switch (condition) {
		case 0:
			if ((ZERO_MASK&cpu->f)==0) cpu->pc = address;
			break;
		case 1:
			if ((ZERO_MASK&cpu->f)!=0) cpu->pc = address;
			break;
		case 2:
			if ((CARRY_MASK&cpu->f)==0) cpu->pc = address;
			break;
		case 3:
			if ((CARRY_MASK&cpu->f)!=0) cpu->pc = address;
			break;
		case 4:
			if ((PV_MASK&cpu->f)==0) cpu->pc = address;
			break;
		case 5:
			if ((PV_MASK&cpu->f)!=0) cpu->pc = address;
			break;
		case 6:
			if ((SIGN_MASK&cpu->f)==0) cpu->pc = address;
			break;
		case 7:
			if ((SIGN_MASK&cpu->f)!=0) cpu->pc = address;
			break;
	}

	return 10;
}

int jr_condition(CPU_t *cpu) {
	int condition = (cpu->bus >> 3) & 3;
	char reg = (char) CPU_mem_read(cpu, cpu->pc++);
	int time = 7;
	switch (condition) {
		case 0:
			if ((ZERO_MASK&cpu->f)==0) {
				cpu->pc = addschar(cpu->pc,reg);
				time += 5;
			}
			break;
		case 1:
			if ((ZERO_MASK&cpu->f)!=0) {
				cpu->pc = addschar(cpu->pc,reg);
				time += 5;
			}
			break;
		case 2:
			if ((CARRY_MASK&cpu->f)==0) {
				cpu->pc = addschar(cpu->pc,reg);
				time += 5;
			}
			break;
		case 3:
			if ((CARRY_MASK&cpu->f)!=0) {
				cpu->pc = addschar(cpu->pc,reg);
				time += 5;
			}
			break;
	}

	return time;
}

int djnz(CPU_t *cpu) {
	char reg = (char) CPU_mem_read(cpu, cpu->pc++);
	if (--cpu->b != 0) {
		cpu->pc = addschar(cpu->pc, reg);
		return 13;
	}

	return 8;
}

int ei(CPU_t *cpu) {
	cpu->iff1 = cpu->iff2 = TRUE;
	cpu->ei_block = TRUE;
	return 4;
}

int di(CPU_t *cpu) {
	cpu->iff1 = cpu->iff2 = FALSE;
	return 4;
}

#pragma warning(pop)