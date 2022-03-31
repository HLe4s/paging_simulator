#include <stdio.h>
#include <stdlib.h>

#define MEMSIZE	1024 * 1024 * 2
#define PGSIZE 1024 * 4
#define PGFAULT_HANDLER 0

// I WILL PROPOSE PAE FLAGS IN CR4 ALWAYS BE SETTED TO 1

extern void * page_fault(void *); // you can register a page_fault func

struct cr3_struct
{
	union 
	{
		struct 
		{
			unsigned ignore : 3;
			unsigned wrtie_through : 1;
			unsigned cache_disable : 1;
			unsigned ignore2 : 7;
			unsigned pml4_addr : 28;
			unsigned reserved : 24;
		};
		size_t cr3 : 64;
	};
};

struct pml4_entry
{
	union 
	{
		struct 
		{
			unsigned present : 1;
			unsigned rw : 1;
			unsigned us : 1;
			unsigned write_through : 1;
			unsigned cache_disable : 1;
			unsigned accessed : 1;
			unsigned ignore : 1;
			unsigned reserved : 1;
			unsigned ignore2 : 4;
			size_t next_addr : 51;
			unsigned exb : 1;
		};
		size_t pml4 : 64;
	};
};

struct pdpt_entry
{
	union 
	{
		struct 
		{
			unsigned present : 1;
			unsigned rw : 1;
			unsigned us : 1;
			unsigned write_through : 1;
			unsigned cache_disable : 1;
			unsigned accessed : 1;
			unsigned dirty : 1;
			unsigned ps : 1;
			unsigned global : 1;
			unsigned ignore : 3;
			size_t next_addr : 51;
			unsigned exb : 1;
		};
		size_t pdpte : 64;
	};
};

struct pd_entry
{
	union 
	{
		struct 
		{
			unsigned present : 1;
			unsigned rw : 1;
			unsigned us : 1;
			unsigned write_through : 1;
			unsigned cache_disable : 1;
			unsigned accessed : 1;
			unsigned dirty : 1;
			unsigned ps : 1;
			unsigned global : 1;
			unsigned ignore : 3;
			size_t next_addr : 28;
			unsigned exb : 1;
		};
		size_t pde : 64;
	};
};

struct pt_entry
{
	union 
	{
		struct 
		{
			unsigned present : 1;
			unsigned rw : 1;
			unsigned us : 1;
			unsigned write_through : 1;
			unsigned cache_disable : 1;
			unsigned accessed : 1;
			unsigned dirty : 1;
			unsigned pat : 1;
			unsigned global : 1;
			unsigned ignore : 3;
			size_t next_addr : 28;
			unsigned exb : 1;
		};
		size_t pte : 64;
	};
};

struct addr_struct
{
	union
	{
		struct
		{
			unsigned frame_offset : 12;
			unsigned pt_offset : 9;
			unsigned pd_offset : 9;
			unsigned pdp_offset : 9;
			unsigned pml4_offset : 9;
			unsigned reserved : 16;
		};
		size_t addr : 64;
	};
};

unsigned char * physical_memory[MEMSIZE];
struct cr3_struct cr3;

void enable_paging(void * pml4)
{
	cr3.pml4_addr = ((size_t)pml4) >> 12;
	cr3.cache_disable = 1;
}

void init_memory()
{
	struct pml4_entry * pml4_table;
	struct pdpt_entry * pdp_table;
	struct pd_entry * pd_table;
	struct pt_entry * page_table;
	int i;

	for ( i = 0; i < MEMSIZE ; i ++){
		physical_memory[i] = (unsigned char *) malloc(16 * 1024);
	}
	pml4_table = physical_memory[1];
	pdp_table = physical_memory[2];
	pd_table = physical_memory[3];
	page_table = physical_memory[4];

	pml4_table->pml4 = 0x2003;
	pdp_table->pdpte = 0x3003;
	pd_table->pde = 0x4003;
	page_table->pte = 0x0;
	page_table[1].pte = 0x1003;
	page_table[2].pte = 0x2003;
	page_table[3].pte = 0x3003;
	page_table[4].pte = 0x4003;

	enable_paging(0x1003);
}

void abort_handler(char * str)
{
	printf("[PANIC] %s\n", str);
	exit(-1);
}

void * access_memory_p(int frame, int offset)
{
	return ((void *)physical_memory[frame] + offset * 0x8);
}

void * access_memory(struct addr_struct addr)
{
        struct pdpt_entry * pdp_table;
        struct pdpt_entry tmp;
        struct pd_entry * pd_table;
        struct pt_entry * page_table;
	struct pt_entry * frame_ptr; // temp use..
	int frame = -1;

	pdp_table = access_memory_p(cr3.pml4_addr, addr.pml4_offset);
	if (!pdp_table->present)
		goto fault;
	pdp_table->accessed = 1;

	pd_table = access_memory_p(pdp_table->next_addr, addr.pdp_offset);
	if (!pd_table->present)
		goto fault;
	pd_table->accessed = 1;

	page_table = access_memory_p(pd_table->next_addr, addr.pd_offset);
	if (!page_table->present)
		goto fault;
	page_table->accessed = 1;

	frame_ptr = access_memory_p(page_table->next_addr, addr.pt_offset);
	if (!frame_ptr->present)
		goto fault;
	frame_ptr->accessed = 1;
	frame = frame_ptr->next_addr;

	if(frame <= 0)
fault:
	#if PGFAULT_HANDLER == 1
		if(page_fault(&addr) == -1)
			abort("Segment fault occured !");
	#elif PGFAULT_HANDLER == 0
		abort_handler("Segment fault occured !");
	#endif

	return (physical_memory[frame] + addr.frame_offset);
}

void set_pml4_addr(struct addr_struct addr)
{
	cr3.pml4_addr = addr.addr >> 12;
}

unsigned char arbitrary_address_read_byte(struct addr_struct addr)
{
	unsigned char * ptr = access_memory(addr);
	return *ptr;
}

void arbitrary_address_write_byte(struct addr_struct addr, unsigned char data)
{
	unsigned char * ptr = access_memory(addr);
	*ptr = data;
}

size_t arbitrary_address_read(struct addr_struct addr)
{
	size_t data;
	unsigned char * ptr = &data;
	int i;

	for ( i = 0 ; i < 8 ; i ++)
	{
		ptr[i] = arbitrary_address_read_byte(addr);
		addr.addr += 1;
	}
	return data;
}

void arbitrary_address_write(struct addr_struct addr, size_t data)
{
	int i;
	unsigned char * src = &data;
	unsigned char * dst;

	for ( i = 0 ;i < 8 ;i ++ )
	{
		arbitrary_address_write_byte(addr, src[i]);
		addr.addr += 1;
	}
}

void arbitrary_address_read_string(struct addr_struct addr)
{
	unsigned char ch;
	while (1)
	{
		ch = arbitrary_address_read_byte(addr);
		if (ch == 0x0)
			break;
		putchar(ch);
		addr.addr += 1;
	}
}

void arbitrary_address_write_string(struct addr_struct addr)
{
	unsigned char ch;
        while (1)
        {
                ch = getchar();
		arbitrary_address_write_byte(addr, ch);
                addr.addr += 1;
                if (ch == '\n')
			break;
        }
	arbitrary_address_write_byte(addr, 0x00);
}

int main(void)
{
	struct addr_struct addr;
	size_t data;
	int ch;

	init_memory();

	while ( 1 )
	{
		printf("\n\n\n---------- Memory Paging simulator -----------\n");
		printf("1. cr3 setting\n");
		printf("2. page table's setting\n");
		printf("3. read from arbitrary address\n");
		printf("4. write to arbitrary address\n");
		printf("5. read string from arbitrary address\n");
		printf("6. write string to arbitrary address\n");
		printf("7. exit\n");
		printf(">> ");
		scanf("%d", &ch);

		switch (ch)
		{
		case 1:
			printf("enter the pml4's address : ");
			scanf("%llx", &(addr));
			set_pml4_addr(addr);
			break;
		case 2:
			break;
		case 3:
			printf("enter address : ");
			scanf("%llx", &(addr));
			printf("data : 0x%llx\n", arbitrary_address_read(addr));
			break;
		case 4:
			printf("enter address : ");
			scanf("%llx", &(addr));
			printf("original data : 0x%llx\n", arbitrary_address_read(addr));
			printf("type >> ");
			scanf("%llx", &data);
			arbitrary_address_write(addr, data);
			break;
		case 5:
			printf("enter address : ");
			scanf("%llx", &(addr));
			printf("data : ");
			arbitrary_address_read_string(addr);
			break;
		case 6:
			printf("enter address : ");
			scanf("%llx", &(addr));
			getchar();
			printf("type >> ");
			arbitrary_address_write_string(addr);
			break;
		case 7:
			return;
		}
	}
}
