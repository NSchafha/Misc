#ifndef VCVALIDATE_H
#define VCVALIDATE_H

#include "LinkedListAPI.h"
#include "VCParser.h"
#include "VCHelpers.h"

VCardErrorCode validateFileName(const char* fileName);
VCardErrorCode validateFileCard(FILE* fptr, Card* obj);

VCardErrorCode validateDateTime(const DateTime* date);
VCardErrorCode validateProperty(const Property* prop);
VCardErrorCode cardnalityCheck(const char* name);
VCardErrorCode validateParameter(const Parameter* param);

#endif