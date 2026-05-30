#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "sageos_build/sage_pkg/SageLang/core/include/token.h"
#include "sageos_build/sage_pkg/SageLang/core/include/diagnostic.h"

int g_repl_mode = 0;
jmp_buf g_repl_error_jmp;

void sage_print_token_diagnosticf(const char* severity, const Token* token, const char* source_ctx, int span, const char* help, const char* fmt, ...) {
    printf("SYNTAX ERROR: %s\n", severity);
}
void console_write(const char* s) { printf("%s", s); }
void console_putc(char c) { putchar(c); }

#include "sageos_build/sage_pkg/SageLang/core/src/c/lexer.c"

int main() {
    FILE* f = fopen("sageos_build/kernel/etc/init.sage", "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = 0;
    fclose(f);

    init_lexer(source, "/etc/init.sage");
    while (1) {
        Token t = scan_token();
        printf("Token: %d\n", t.type);
        if (t.type == TOKEN_EOF) break;
    }
    return 0;
}
