#include <mapper.h>
#include <string.h>

void with_interrupts_disabled(void *, ...);

static interrupt_descriptor_t *idt_page = 0xffffffffffffd000;


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
} interrupt_descriptor_t __attribute__ ((packed));
typedef struct {
    uint16_t limit;
    uint64_t offset;
} idtr_t __attribute__ ((packed));


void init_interrupts() {
    ASSERT(sizeof(interrupt_descriptor_t) == 16);
    map_page(idt_page, MAP_ANON, PAGE_P_BIT | PAGE_RW_BIT | PAGE_G_BIT);
    memset(idt_page, 0, 256 * sizeof(interrupt_descriptor_t));
    idtr_t *idtr = idt_page + 256;
    idtr->limit = 256 * sizeof(interrupt_descriptor_t) - 1;
    idtr->offset = idt_page;
    asm volatile ("lidt (%0)"::"r" (idtr));
}

void install_interrupt(void *isr, uint8_t interrupt_no) {
    interrupt_descriptor_t desc = { 0 };
    desc.offset_1 = isr & 0xffff;
    desc.offset_2 = isr & 0xffff0000;
    desc.offset_3 = isr & 0xffffffff00000000;
    desc.seg_sel = 8;
    desc.dpl = 0;
    desc.present = 1;
    desc.type = 0xE;
    idt_page[interrupt_no] = desc;
}

