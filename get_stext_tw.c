#include <stdio.h>
#include <string.h>

int TEXTW(const char* s) {
    return strlen(s) * 10;
}

int main() {
    const char *stext = "hello world";
    int tw, stextw = TEXTW(stext) + 2;
    tw = stextw;

    // In dwm.c:
    // tw = stextw;
    // drw_text(drw, m->ww - tw, 0, tw, bh, 0, stext, 0);

    printf("tw=%d\n", tw);
    return 0;
}
