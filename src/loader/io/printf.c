#include <io/printf.h>
#include <io/ocdev.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

typedef struct {
    uint8_t flags;
    char specifier;
    uint16_t width, precision;
    uint16_t length;
} io_printf_format_specifier_t;

#define IO_PRINTF_FLAG_PLUS (1)
#define IO_PRINTF_FLAG_MINUS (2)
#define IO_PRINTF_FLAG_SPACE (4)
#define IO_PRINTF_FLAG_SHARP (8)
#define IO_PRINTF_FLAG_ZERO (16)
#define IO_PRINTF_FLAG_PRECISION_SPECIFIED (32)
#define IO_PRINTF_FLAG_PARSE_ERROR (INT8_MIN)

#define IO_PRINTF_LENGTH_none (0)
#define IO_PRINTF_LENGTH_hh (1)
#define IO_PRINTF_LENGTH_h (2)
#define IO_PRINTF_LENGTH_l (3)
#define IO_PRINTF_LENGTH_ll (4)
// j, z, t and L are just fucked up
// No, they are not. Please, implement them.
// TODO j, z, t, L

uint16_t io_internal_atoi(const char **str) {
    uint16_t ans = 0;
    while (**str >= '0' && **str <= '9') {
        ans *= 10;
        ans += **str - '0';
        ++*str;
    }
    return ans;
}

// Format specifier structure: %[flags][width][.precision][length]specifier
io_printf_format_specifier_t io_parse_format_specifier(const char **format_ptr, va_list *vlist_ptr) {
    const char *format = *format_ptr;
    io_printf_format_specifier_t ans;
    ans.flags = 0;

    // Flags
    bool are_there_flags = true;

    while (are_there_flags) {
        ++format;
        switch (*format) {
            case '-':
                ans.flags |= IO_PRINTF_FLAG_MINUS;
                break;
            case '+':
                ans.flags |= IO_PRINTF_FLAG_PLUS;
                break;
            case ' ':
                ans.flags |= IO_PRINTF_FLAG_SPACE;
                break;
            case '#':
                ans.flags |= IO_PRINTF_FLAG_SHARP;
                break;
            case '0':
                ans.flags |= IO_PRINTF_FLAG_ZERO;
                break;
            default:
                are_there_flags = false;
                break;
        }
    }

    // Width
    ans.width = 0;
    if (*format == '*') {
        ans.width = va_arg(*vlist_ptr, int);
        ++format;
    } else {
        ans.width = io_internal_atoi(&format);
    }

    // Precision
    ans.precision = 0;
    if (*format == '.') {
        ++format;
        ans.flags |= IO_PRINTF_FLAG_PRECISION_SPECIFIED;
        if (*format == '*') {
            ans.precision = va_arg(*vlist_ptr, int);
            ++format;
        } else {
            ans.precision = io_internal_atoi(&format);
        }
    }

    // Length
    ans.length = IO_PRINTF_LENGTH_none;
    if (*format == 'h') {
        ++format;
        ans.length = IO_PRINTF_LENGTH_h;
        if (*format == 'h') {
            ++format;
            ans.length = IO_PRINTF_LENGTH_hh;
        }
    } else if (*format == 'l') {
        ++format;
        ans.length = IO_PRINTF_LENGTH_l;
        if (*format == 'l') {
            ++format;
            ans.length = IO_PRINTF_LENGTH_ll;
        }
    }

    if (strchr("diuoxXfFeEgGaAcspn%", *format) != NULL) {
        ans.specifier = *format++;
    } else {
        ans.specifier = '!';
        ans.flags |= IO_PRINTF_FLAG_PARSE_ERROR;
    }

    *format_ptr = format;
    return ans;
}

#define INIT_PRINTF_SUBROUTINE \
    bool left_justify = spec.flags & IO_PRINTF_FLAG_MINUS; \
    bool forced_sign = spec.flags & IO_PRINTF_FLAG_PLUS; \
    bool space_for_sign = spec.flags & IO_PRINTF_FLAG_SPACE; \
    bool use_prefix = spec.flags & IO_PRINTF_FLAG_SHARP; \
    bool pad_with_zeroes = spec.flags & IO_PRINTF_FLAG_ZERO; \
    bool precision_specified = spec.flags & IO_PRINTF_FLAG_PRECISION_SPECIFIED;

#define INIT_DIGITS(uppercase) \
    const char *DIGITS = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

#define BUF_SIZE 25

#define PRINTER_HEADER(suffix, type) \
bool io_internal_print_##suffix (ocdev_t ocdev, io_printf_format_specifier_t spec, type d)

#define INTEGER_PRINTER(suffix, type, base, prefix, uppercase) \
PRINTER_HEADER(suffix, type) { \
    INIT_PRINTF_SUBROUTINE; \
    INIT_DIGITS(uppercase); \
    char buf[BUF_SIZE]; \
    buf[BUF_SIZE - 1] = 0; \
    char sign_c = 0; \
    if (d < 0) { \
        sign_c = '-'; \
        d = -d; \
    } else if (forced_sign) { \
        sign_c = '+'; \
    } else if (space_for_sign) { \
        sign_c = ' '; \
    } \
    char *buf_writer = buf + BUF_SIZE - 1; \
    int prefix_len = (use_prefix) ? strlen(prefix) : 0; /* I want to print 0x0 instead of 0 */ \
    while (d > 0) { \
        *(--buf_writer) = DIGITS[d % (base)]; \
        d /= (base); \
    } \
    if (!precision_specified) { \
        spec.precision = 1; \
    } \
    int digits = (buf + BUF_SIZE - 1) - buf_writer; \
    int non_space_characters = (sign_c ? 1 : 0) + max(spec.precision, digits) + prefix_len; \
    char width_padder = pad_with_zeroes ? '0' : ' '; \
    if (!left_justify) {\
        for (int i = non_space_characters; i < spec.width; ++i) { \
            ocdev.putc(width_padder); \
        } \
    } \
    if (sign_c) { \
        ocdev.putc(sign_c); \
    } \
    ocdev.putsl(prefix, prefix_len); \
    for (int i = digits; i < spec.precision; ++i) { \
        ocdev.putc('0'); \
    } \
    ocdev.puts(buf_writer); \
    if (left_justify) {\
        for (int i = non_space_characters; i < spec.width; ++i) { \
            ocdev.putc(' ' /*width_padder*/); \
        } \
    } \
    return true; \
}

#define GEN_PRINTERS(common_suffix, base, sign, prefix, uppercase) \
INTEGER_PRINTER( common_suffix##_int, sign int, base, prefix, uppercase); \
INTEGER_PRINTER( common_suffix##_char, sign char, base, prefix, uppercase); \
INTEGER_PRINTER( common_suffix##_s_int, sign short int, base, prefix, uppercase); \
INTEGER_PRINTER( common_suffix##_l_int, sign long int, base, prefix, uppercase); \
INTEGER_PRINTER( common_suffix##_ll_int, sign long long int, base, prefix, uppercase);

GEN_PRINTERS(s, 10, signed, "", false);
GEN_PRINTERS(u, 10, unsigned, "", false);
GEN_PRINTERS(o, 8, unsigned, "0", false);
GEN_PRINTERS(h, 16, unsigned, "0x", false);
GEN_PRINTERS(H, 16, unsigned, "0X", true);

#define SUBROUTINE_HEADER(suffix) \
bool io_printf_subroutine_##suffix (ocdev_t ocdev, io_printf_format_specifier_t spec, va_list *vlist_ptr)

#define GEN_SUBROUTINE(subroutine_suffix, printer_type) \
SUBROUTINE_HEADER(subroutine_suffix) { \
    switch (spec.length) { \
        case IO_PRINTF_LENGTH_none: \
            return io_internal_print_##printer_type##_int(ocdev, spec, va_arg(*vlist_ptr, int)); \
        case IO_PRINTF_LENGTH_hh: \
            return io_internal_print_##printer_type##_char(ocdev, spec, va_arg(*vlist_ptr, int)); \
        case IO_PRINTF_LENGTH_h: \
            return io_internal_print_##printer_type##_s_int(ocdev, spec, va_arg(*vlist_ptr, int)); \
        case IO_PRINTF_LENGTH_l: \
            return io_internal_print_##printer_type##_l_int(ocdev, spec, va_arg(*vlist_ptr, long int)); \
        case IO_PRINTF_LENGTH_ll: \
            return io_internal_print_##printer_type##_ll_int(ocdev, spec, va_arg(*vlist_ptr, long long int)); \
    } \
    return false; \
} \

GEN_SUBROUTINE(d, s);
SUBROUTINE_HEADER(i) __attribute__ ((alias("io_printf_subroutine_d")));
GEN_SUBROUTINE(u, u);
GEN_SUBROUTINE(o, o);
GEN_SUBROUTINE(x, h);
GEN_SUBROUTINE(X, H);

PRINTER_HEADER(string, char *) {
    INIT_PRINTF_SUBROUTINE;
    uint32_t len = strlen(d);
    uint32_t chars = precision_specified ? min(len, spec.precision) : len;
    if (!left_justify) {
        for (uint32_t i = chars; i < spec.width; ++i) {
            ocdev.putc(' ');
        }
    }
    ocdev.putsl(d, chars);
    if (left_justify) {
        for (uint32_t i = chars; i < spec.width; ++i) {
            ocdev.putc(' ');
        }
    }
    return true;
}

SUBROUTINE_HEADER(s) {
    if (spec.length == IO_PRINTF_LENGTH_none) {
        return io_internal_print_string(ocdev, spec, va_arg(*vlist_ptr, char *));
    }
    return false;
}

int io_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ans = io_vdprintf(io_get_std_ocdev(), format, args);
    va_end(args);
    return ans;
}

int io_dprintf(const ocdev_t ocdev, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ans = io_vdprintf(ocdev, format, args);
    va_end(args);
    return ans;
}

int io_vprintf(const char *format, va_list vlist) {
    return io_vdprintf(io_get_std_ocdev(), format, vlist);
}

#define CASE_SUBROUTINE(symbol, suffix) \
    case symbol : \
        io_printf_subroutine_##suffix (ocdev, spec, &vlist); \
        break;

int io_vdprintf(const ocdev_t ocdev, const char *format, va_list vlist) {
    while (*format) {
        if (*format == '%') {
            io_printf_format_specifier_t spec = io_parse_format_specifier(&format, &vlist);
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
                    break;
                case '%': // The simpliest subroutine
                    ocdev.putc('%');
                    break;
                default:
                    ocdev.puts("<Not-Implemented-Yet>");
                    break;
            }
        } else {
            ocdev.putc(*format++);
        }
    }
    return 0;
}
