#include <io/printf.h>
#include <io/ocdev.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#if defined(__i386__)
#define VA_LIST_PTR     va_list *
#define VA_LIST_ARG(X)  (&(X))
#define VA_LIST_REF(X)  (*(X))
#elif defined(__x86_64__)
#define VA_LIST_PTR     va_list
#define VA_LIST_ARG(X)  (X)
#define VA_LIST_REF(X)  (X)
#else
#error unsupported architecture
#endif

#define FLAG_PLUS                 1
#define FLAG_MINUS                2
#define FLAG_SPACE                4
#define FLAG_SHARP                8
#define FLAG_ZERO                 16
#define FLAG_PRECISION_SPECIFIED  32

#define LENGTH_none   0
#define LENGTH_hh     1
#define LENGTH_h      2
#define LENGTH_l      3
#define LENGTH_ll     4

typedef struct {
    uint8_t flags;
    char specifier;
    uint16_t width, precision;
    uint16_t length;
} printf_format_specifier_t;

static uint16_t atoi(const char **str) {
    uint16_t ans = 0;
    while (**str >= '0' && **str <= '9') {
        ans *= 10;
        ans += **str - '0';
        ++*str;
    }
    return ans;
}

/* Format specifier structure: %[flags][width][.precision][length]specifier */
static printf_format_specifier_t parse_format_specifier(const char
        **format_ptr, VA_LIST_PTR vlist) {
    const char *format = *format_ptr;
    printf_format_specifier_t ans;
    ans.flags = 0;

    /* Flags */
    bool are_there_flags = true;

    while (are_there_flags) {
        ++format;
        switch (*format) {
        case '-':
            ans.flags |= FLAG_MINUS;
            break;
        case '+':
            ans.flags |= FLAG_PLUS;
            break;
        case ' ':
            ans.flags |= FLAG_SPACE;
            break;
        case '#':
            ans.flags |= FLAG_SHARP;
            break;
        case '0':
            ans.flags |= FLAG_ZERO;
            break;
        default:
            are_there_flags = false;
            break;
        }
    }

    /* Width */
    ans.width = 0;
    if (*format == '*') {
        ans.width = va_arg(VA_LIST_REF(vlist), int);
        ++format;
    } else {
        ans.width = atoi(&format);
    }

    /* Precision */
    ans.precision = 0;
    if (*format == '.') {
        ++format;
        ans.flags |= FLAG_PRECISION_SPECIFIED;
        if (*format == '*') {
            ans.precision = va_arg(VA_LIST_REF(vlist), int);
            ++format;
        } else {
            ans.precision = atoi(&format);
        }
    }

    /* Length */
    ans.length = LENGTH_none;
    if (*format == 'h') {
        ++format;
        ans.length = LENGTH_h;
        if (*format == 'h') {
            ++format;
            ans.length = LENGTH_hh;
        }
    } else if (*format == 'l') {
        ++format;
        ans.length = LENGTH_l;
        if (*format == 'l') {
            ++format;
            ans.length = LENGTH_ll;
        }
    } else if (*format == 'j') { /* We are only supporting i386 and x86_64,
                                    so it's okay */
        ++format;
        ans.length = LENGTH_ll;
    } else if (*format == 'z' || *format == 't') {
        ++format;
        ans.length = LENGTH_l;
    }

    /* Specifier */
    ans.specifier = *format++;

    if (ans.specifier == 'p') {
        ans.flags |= FLAG_PRECISION_SPECIFIED;
        ans.width = 0;
        ans.precision = sizeof(void *) * 2;
        ans.length = LENGTH_l;
        ans.specifier = 'x';
    }

    *format_ptr = format;
    return ans;
}

#define INIT_DIGITS(UPPERCASE) \
    const char *DIGITS = UPPERCASE ? "0123456789ABCDEF" : "0123456789abcdef";

#define BUF_SIZE 25

#define PRINTER_HEADER(SUFFIX, TYPE) \
int print_##SUFFIX(ocdev_t ocdev, printf_format_specifier_t spec, TYPE d)

#define SIGN_CHECKER \
    if (d < 0) { \
        sign_c = '-'; \
        d = -d; \
    } else

#define INTEGER_PRINTER(SUFFIX, TYPE, BASE, PREFIX, UPPERCASE, SIGN_CHECK) \
PRINTER_HEADER(SUFFIX, TYPE) { \
    int ret = 0; \
    INIT_DIGITS((UPPERCASE)); \
    char buf[BUF_SIZE]; \
    buf[BUF_SIZE - 1] = 0; \
    char sign_c = 0; \
    SIGN_CHECK if (spec.flags & FLAG_PLUS) { \
        sign_c = '+'; \
    } else if (spec.flags & FLAG_SPACE) { \
        sign_c = ' '; \
    } \
    char *buf_writer = buf + BUF_SIZE - 1; \
    /* I want to print 0x0 instead of 0 */ \
    int prefix_len = \
            (spec.flags & FLAG_SHARP) ? strlen((PREFIX)) : 0; \
    while (d > 0) { \
        *(--buf_writer) = DIGITS[d % (BASE)]; \
        d /= (BASE); \
    } \
    if (!(spec.flags & FLAG_PRECISION_SPECIFIED)) { \
        spec.precision = 1; \
    } \
    int digits = (buf + BUF_SIZE - 1) - buf_writer; \
    int non_space_characters = \
            (sign_c ? 1 : 0) + max(spec.precision, digits) + prefix_len; \
    char width_padder = (spec.flags & FLAG_ZERO) ? '0' : ' '; \
    if (!(spec.flags & FLAG_MINUS)) {\
        for (int i = non_space_characters; i < spec.width; ++i) { \
            ocdev.putc(width_padder); \
            ++ret; \
        } \
    } \
    if (sign_c) { \
        ocdev.putc(sign_c); \
        ++ret; \
    } \
    ocdev.putns(PREFIX, prefix_len); \
    ret += prefix_len; \
    for (int i = digits; i < spec.precision; ++i) { \
        ocdev.putc('0'); \
        ++ret; \
    } \
    ret += ocdev.puts(buf_writer); \
    if (spec.flags & FLAG_MINUS) {\
        for (int i = non_space_characters; i < spec.width; ++i) { \
            ocdev.putc(' '); \
            ++ret; \
        } \
    } \
    return ret; \
}

#define GEN_PRINTERS(COMMON_SUFFIX, BASE, SIGN, PREFIX, \
        UPPERCASE, SIGN_CHECK) \
INTEGER_PRINTER(COMMON_SUFFIX##_int, SIGN int, BASE, PREFIX, \
        UPPERCASE, SIGN_CHECK); \
INTEGER_PRINTER(COMMON_SUFFIX##_char, SIGN char, BASE, PREFIX, \
        UPPERCASE, SIGN_CHECK); \
INTEGER_PRINTER(COMMON_SUFFIX##_s_int, SIGN short int, BASE, PREFIX, \
        UPPERCASE, SIGN_CHECK); \
INTEGER_PRINTER(COMMON_SUFFIX##_l_int, SIGN long int, BASE, PREFIX, \
        UPPERCASE, SIGN_CHECK); \
INTEGER_PRINTER(COMMON_SUFFIX##_ll_int, SIGN long long int, BASE, PREFIX, \
        UPPERCASE, SIGN_CHECK);

GEN_PRINTERS(s, 10, signed, "", false, SIGN_CHECKER);
GEN_PRINTERS(u, 10, unsigned, "", false,);
GEN_PRINTERS(o, 8, unsigned, "0", false,);
GEN_PRINTERS(h, 16, unsigned, "0x", false,);
GEN_PRINTERS(H, 16, unsigned, "0X", true,);

#define SUBROUTINE_HEADER(SUFFIX) \
int printf_subroutine_##SUFFIX(ocdev_t ocdev, \
        printf_format_specifier_t spec, \
        VA_LIST_PTR vlist)

#define GEN_SUBROUTINE(SUBROUTINE_SUFFIX, PRINTER_TYPE) \
SUBROUTINE_HEADER(SUBROUTINE_SUFFIX) { \
    switch (spec.length) { \
    case LENGTH_none: \
        return print_##PRINTER_TYPE##_int(ocdev, spec, \
                va_arg(VA_LIST_REF(vlist), int)); \
    case LENGTH_hh: \
        return print_##PRINTER_TYPE##_char(ocdev, spec, \
                va_arg(VA_LIST_REF(vlist), int)); \
    case LENGTH_h: \
        return print_##PRINTER_TYPE##_s_int(ocdev, spec, \
                va_arg(VA_LIST_REF(vlist), int)); \
    case LENGTH_l: \
        return print_##PRINTER_TYPE##_l_int(ocdev, spec, \
                va_arg(VA_LIST_REF(vlist), long int)); \
    case LENGTH_ll: \
        return print_##PRINTER_TYPE##_ll_int(ocdev, spec, \
                va_arg(VA_LIST_REF(vlist), long long int)); \
    } \
    __builtin_unreachable(); \
} \

GEN_SUBROUTINE(d, s);
SUBROUTINE_HEADER(i) __attribute__ ((alias("printf_subroutine_d")));
GEN_SUBROUTINE(u, u);
GEN_SUBROUTINE(o, o);
GEN_SUBROUTINE(x, h);
GEN_SUBROUTINE(X, H);

SUBROUTINE_HEADER(s) {
    int ret = 0;
    char *d = va_arg(VA_LIST_REF(vlist), char *);
    uint32_t len = strlen(d);
    uint32_t chars = (spec.flags & FLAG_PRECISION_SPECIFIED) ?
            umin(len, spec.precision) : len;
    if (!(spec.flags & FLAG_MINUS)) {
        for (uint32_t i = chars; i < spec.width; ++i) {
            ocdev.putc(' ');
            ++ret;
        }
    }
    ret += ocdev.putns(d, chars);
    if (spec.flags & FLAG_MINUS) {
        for (uint32_t i = chars; i < spec.width; ++i) {
            ocdev.putc(' ');
            ++ret;
        }
    }
    return ret;
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ans = vdprintf(std_ocdev, format, args);
    va_end(args);
    return ans;
}

int dprintf(const ocdev_t ocdev, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ans = vdprintf(ocdev, format, args);
    va_end(args);
    return ans;
}

int vprintf(const char *format, va_list vlist) {
    return vdprintf(std_ocdev, format, vlist);
}

#define CASE_SUBROUTINE(SYMBOL, SUFFIX) \
case SYMBOL: \
    ret += printf_subroutine_##SUFFIX(ocdev, spec, VA_LIST_ARG(vlist)); \
    break;

int vdprintf(const ocdev_t ocdev, const char *format, va_list vlist) {
    int ret = 0;
    while (*format) {
        if (*format == '%') {
            printf_format_specifier_t spec =
                    parse_format_specifier(&format, VA_LIST_ARG(vlist));
            switch (spec.specifier) {
                CASE_SUBROUTINE('d', d);
                CASE_SUBROUTINE('i', i);
                CASE_SUBROUTINE('u', u);
                CASE_SUBROUTINE('o', o);
                CASE_SUBROUTINE('x', x);
                CASE_SUBROUTINE('X', X);
                CASE_SUBROUTINE('s', s);
            case 'c':
                ocdev.putc(va_arg(vlist, int));
                ++ret;
                break;
            case '%':          /* The simpliest subroutine */
                ocdev.putc('%');
                ++ret;
                break;
            }
        } else {
            ocdev.putc(*format++);
            ++ret;
        }
    }
    return ret;
}
