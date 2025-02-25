#ifndef STATE_H_
#define STATE_H_

#include "corecalc.h"

typedef struct apphdr {
	TCHAR name[12];
	unsigned int page, page_count;
} apphdr_t;

typedef struct applist {
	unsigned int count;
	apphdr_t apps[255];
} applist_t;

typedef struct {
	uint8_t type_ID;
	uint8_t type_ID2;
	uint8_t version;
	uint16_t address;
	uint8_t page;
	uint8_t name_len;
	uint16_t length;
	char name[9];
} symbol83P_t;

typedef struct symlist {
	symbol83P_t *programs;
	symbol83P_t *last;
	symbol83P_t symbols[2048];
	unsigned int count;
} symlist_t;

// 83p
#define PTEMP_83P			0x982E
#define PROGPTR_83P			0x9830
#define SYMTABLE_83P		0xFE66
// 84PCSE
#define PTEMP_84PCSE		0x9E0F
#define PROGPTR_84PCSE		0x9E11
#define SYMTABLE_84PCSE		0xFD9E
// 86
#define VAT_END			0xD298

typedef struct upages {
	unsigned int start, end;
} upages_t;

#define circ10(z) ((((unsigned char) z) < 10) ? ((z) + 1) % 10 : (z))
#define tAns	0x72

void state_build_applist(CPU_t *, applist_t *);
void state_userpages(CPU_t *, upages_t *);
symlist_t *state_build_symlist_86(CPU_t *, symlist_t *);
symlist_t *state_build_symlist_83P(CPU_t *, symlist_t *);
TCHAR *GetRealAns(CPU_t *, TCHAR *);
TCHAR *Symbol_Name_to_String(int model, symbol83P_t *symbol, TCHAR * buffer, int bufferSize);
TCHAR *App_Name_to_String(apphdr_t *, TCHAR *);
unsigned char find_field(unsigned char *dest, unsigned char id1, unsigned char id2, unsigned char **output);
unsigned int get_page_size(unsigned char *dest);

#endif /*STATE_H_*/
