#include <isr.h>
#include <mapper.h>
#include <string.h>
#include <cpu.h>
#include <terminate.h>

typedef struct {
    uint16_t offset_1;
    uint16_t seg_sel;
    uint8_t ist:3;
    uint8_t mbz_1:5;
    uint8_t type:4;
    uint8_t mbz_2:1;
    uint8_t dpl:2;
    uint8_t present:1;
    uint16_t offset_2;
    uint32_t offset_3;
    uint32_t mbz_3;
} __attribute__ ((packed)) interrupt_descriptor_t;

struct {
    uint16_t limit;
    uint64_t offset;
} __attribute__ ((packed)) idtr;

extern isr_t _isr_table[256];
extern isr_t _c_isr_table[256];
extern void *_idt_page;

static interrupt_descriptor_t *idt_page = (void *) &_idt_page;

static const char *interrupt_mnemonic[20] = {
    "DE", "DB", "NMI", "BP", "OF", "BR", "UD", "NM", "DF", "", "TS", "NP",
    "SS", "GP", "PF", "", "MF", "AC", "MC", "XF",
};

static const char *rflags_mnemonics[22] = {
    "CF ", "" "PF ", "" "AF ", "" "AF ", "SF ", "TF ", "IF ", "DF ", "OF ", "",
    "" "NT ", "", "RT ", "VM ", "AC ", "VIF ", "VIP ", "ID ",
};

static const char *selector_error_mnemonics[3] = {
    "EXT ", "IDT ", "TI ",
};

static const char *pf_error_mnemonics[5] = {
    "P ", "R/W ", "U/S ", "RSV ", "I/D ",
};

__attribute__ ((force_align_arg_pointer))
void _default_c_isr(interrupt_frame_t frame) {
    printf("hOI!!!!!! I'm dEFAuLT iSR!!\n");
    printf("vector   = %#.2x", frame.vector);
    if (frame.vector < 20) {
        printf(" #%s", interrupt_mnemonic[frame.vector]);
    } else if (frame.vector == SX_VECTOR) {
        printf(" #SX");
    }
    switch (frame.vector) {
    case TS_VECTOR:
    case NP_VECTOR:
    case SS_VECTOR:
    case GP_VECTOR:
        printf("(%#.4x)\n", frame.error);
        for (int i = 0; i < 3; ++i) {
            if (frame.rflags & (1 << i)) {
                printf(selector_error_mnemonics[i]);
            }
        }
        printf("\nselector = %#.16x\n", frame.error >> 3);
        break;
    case PF_VECTOR:
        printf("(%#.4x)\n", frame.error);
        for (int i = 0; i < 5; ++i) {
            if (frame.rflags & (1 << i)) {
                printf(pf_error_mnemonics[i]);
            }
        }
        printf("\ncr2      = %#.16lx\n", rdcr2());
        break;
    default:
        printf("\n");
        break;
    }
    printf("return   = %#.2x:%#.16lx\n", frame.cs, frame.rip);
    printf("stack    = %#.2x:%#.16lx\n", frame.ss, frame.rsp);
    printf("rflags   = %#.16lx\n", frame.rflags);
    for (int i = 0; i < 22; ++i) {
        if (frame.rflags & (1 << i)) {
            printf(rflags_mnemonics[i]);
        }
    }
    printf("\niopl     = %ld\n", (frame.rflags >> 12) & 0x3);
}

static void install_asm_isr(isr_t isr, uint8_t vector) {
    interrupt_descriptor_t desc = {0};
    desc.offset_1 = (uint64_t) isr & 0xffff;
    desc.offset_2 = ((uint64_t) isr & 0xffff0000) >> 16;
    desc.offset_3 = ((uint64_t) isr & 0xffffffff00000000ULL) >> 32;
    desc.seg_sel = 8;
    desc.dpl = 0;
    desc.present = 1;
    desc.type = 0xE;
    idt_page[vector] = desc;
}

void isr_init(void) {
    ASSERT(sizeof(interrupt_descriptor_t) == 16);
    map_page(idt_page, MAP_ANON, PAGE_P_BIT | PAGE_RW_BIT | PAGE_G_BIT);
    memset(idt_page, 0, PAGE_SIZE);
    idtr.limit = PAGE_SIZE - 1;
    idtr.offset = (uint64_t) idt_page;

    for (int i = 0; i < 256; ++i) {
        install_asm_isr(_isr_table[i], i);
    }

    asm volatile ("lidt (%0)"::"r" (&idtr));
}

void install_isr(isr_t isr, uint8_t vector) {
    _c_isr_table[vector] = isr;
}