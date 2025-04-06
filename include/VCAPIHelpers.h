#ifndef VCAPIHelpers_H
#define VCAPIHelpers_H

#include "VCParser.h"

typedef struct cardContact
{
    char file_name[60];
    char name[256];
	char birthday[256];
	char anniversary[256];
    int prop_count;
} Contact;

Contact getContact(char* file_name, Card* obj);

VCardErrorCode updateName(char* file_name, char* fn, Card** obj);
VCardErrorCode newCard(char* file_name, char* fn, Card** obj);

char* encodeDate(char* date);
char* decodeDate(char* date);

#endif