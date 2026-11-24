/* The portable Vincent core only needs the public calling-convention macros. */
#ifndef VINCENT_E32DEF_H
#define VINCENT_E32DEF_H
#define GLAPI __attribute__((visibility("default")))
#define IMPORT_C
#define EXPORT_C __attribute__((visibility("default")))
#endif
