/*
    Copyright (C) 1995-2012, The AROS Development Team. All rights reserved.

    Desc: global include for genmodule. Defines global variables and
          the function prototypes.
*/
#ifndef FUNCTIONHEAD_H
#define FUNCTIONHEAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "stringlist.h"

enum libcall { INVALID, STACK, REGISTER, MIXED, REGISTERMACRO, AUTOREGISTER };

struct functionhead;

struct functionarg {
    struct functionarg *next;
    struct functionhead *parent;
    char *arg;
    char *type;
    char *name;
    char *reg;
    int noargname : 1; /* This argument has no name */
    int varargs : 1; /* This argument is ... */
};

struct functionhead {
    struct functionhead *next;
    char *name, *internalname;
    char *type, *comment;
    enum libcall libcall;
    unsigned int argcount;
    struct functionarg *arguments;
    struct stringlist *aliases;
    unsigned int lvo; /* Only for library functions, not methods */
    struct stringlist *interface; /* Only for HIDD class */
    char *method; /* Only for HIID class */
    int version;  /* First library version number this appeared in*/
    int novararg : 1; /* Are varargs allowed for this function ? */
    int priv     : 1; /* Is function private */
    int unusedlibbase : 1; /* Libbase must no be made available internally */
    int deprecated : 1; /* Should function be avoided due to deprecation */
};

struct functionhead *newfunctionhead(const char *name, enum libcall libcall);
struct functionarg *funcaddarg
(
    struct functionhead *funchead,
    const char *arg, const char *reg
);
struct stringlist *funcaddalias(struct functionhead *funchead, const char *alias);
void funcsetinternalname(struct functionhead *funchead, const char *intername);

/* Write out the function prototypes for the functions in the given
 * cfg may be NULL if the list only contains functions with STACK libcall
 */
struct config;
void writefuncdefs(FILE *out, struct config *cfg, struct functionhead *funclist);
void writefuncprotos(FILE *out, struct config *cfg, struct functionhead *funclist);
void writefuncinternalstubs(FILE *out, struct config *cfg, struct functionhead *funclist);

/* getargtype remove the variable name from a variable definition and leave return
 * the type of the variable
 * [] at the end will be added as * in the variable type
 * e.g. char *var[] => type: char **, name: var
 * This is a destructive function and will change to string pointed to by def
 * to only contain the type afterwards.
 * Function return 0 when it did not understand the input, 1 otherwise
 */
char *getargtype(struct functionarg *funcarg);
char *getargname(const struct functionarg *funcarg);

#endif //FUNCTIONHEAD_H
