#include <ctype.h>

int isspace(int ch) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f') ? 1 : 0;
}

int isdigit(int ch) {
    return (ch >= '0' && ch <= '9') ? 1 : 0;
}

int isalpha(int ch) {
    return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) ? 1 : 0;
}

int isalnum(int ch) {
    return (isalpha(ch) != 0 || isdigit(ch) != 0) ? 1 : 0;
}

int isxdigit(int ch) {
    return (isdigit(ch) != 0 || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) ? 1 : 0;
}

int isupper(int ch) {
    return (ch >= 'A' && ch <= 'Z') ? 1 : 0;
}

int islower(int ch) {
    return (ch >= 'a' && ch <= 'z') ? 1 : 0;
}

int isprint(int ch) {
    return (ch >= 0x20 && ch <= 0x7E) ? 1 : 0;
}

int iscntrl(int ch) {
    return (ch >= 0x00 && ch <= 0x1F) || ch == 0x7F ? 1 : 0;
}

int tolower(int ch) {
    return (isupper(ch) != 0) ? (ch - 'A' + 'a') : ch;
}

int toupper(int ch) {
    return (islower(ch) != 0) ? (ch - 'a' + 'A') : ch;
}
