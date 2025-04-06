#ifndef VCHELPERS_H
#define VCHELPERS_H

#include "LinkedListAPI.h"
#include "VCParser.h"
#include "VCValidate.h"

VCardErrorCode createProperty(Property* property, const char* propString);
VCardErrorCode createDateTime(DateTime* dateTime, const char* dateTimeString);

VCardErrorCode createParameterList(List* parameterss, const char* paramsSting);
VCardErrorCode createValueList(List* values, const char* valueString);

char* bdayText(DateTime* date);
char* annText(DateTime* date);

int checkNextChar(FILE* fptr);
VCardErrorCode removeCRLF(char* string);
void removeSpace(char* string);

#endif