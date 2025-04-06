#include "VCParser.h"
#include "LinkedListAPI.h"
#include "VCValidate.h"
#include "VCAPIHelpers.h"
#include <ctype.h>

Contact getContact(char* filename, Card* obj)
{
    Contact contact;
    strcpy(contact.file_name, filename);
    strcpy(contact.name, obj->fn->values->head->data);
    
    if (obj->birthday == NULL) strcpy(contact.birthday, "");
    else strcpy(contact.birthday, dateToString(obj->birthday));

    if (obj->anniversary == NULL) strcpy(contact.anniversary, "");
    else strcpy(contact.anniversary, dateToString(obj->anniversary));

    contact.prop_count = getLength(obj->optionalProperties);

    return contact;
}

VCardErrorCode updateName(char* filename, char* fn, Card** obj)
{
    clearList((*obj)->fn->values);
    char* fnCopy = malloc(strlen(fn) + 1);
    strcpy(fnCopy, fn);
    insertBack((*obj)->fn->values, fnCopy);

    VCardErrorCode err = validateCard(*obj);
    if (err != OK) return err;

    VCardErrorCode writeErr = writeCard(filename, *obj);
    if (writeErr != OK) return writeErr;

    return OK;
}

VCardErrorCode newCard(char* filename, char* fn, Card** obj)
{
    VCardErrorCode filenameErr = validateFileName(filename);

    if (filenameErr != OK) return filenameErr;

    (*obj) = (Card*)malloc(sizeof(Card));

    if ((*obj) == NULL) return INV_CARD;

    Property* prop = (Property*)malloc(sizeof(Property));
    if (prop == NULL) return OTHER_ERROR;

    prop->name = malloc(strlen("FN") + 1);
    strcpy(prop->name, "FN");

    prop->group = (char*)malloc(sizeof(char) * 1);
    prop->group[0] = '\0';

    prop->parameters = initializeList(&parameterToString, &deleteParameter, &compareParameters);
    prop->values = initializeList(&valueToString, &deleteValue, &compareValues);

    char* fnCopy = malloc(strlen(fn) + 1);
    strcpy(fnCopy, fn);
    insertBack(prop->values, fnCopy);

    (*obj)->fn = prop;
    (*obj)->optionalProperties = initializeList(&propertyToString, &deleteProperty, &compareProperties);
    (*obj)->birthday = NULL;
    (*obj)->anniversary = NULL;


    VCardErrorCode err = validateCard(*obj);
    if (err != OK) return err;

    VCardErrorCode writeErr = writeCard(filename, *obj);
    if (writeErr != OK) return writeErr;

    return OK;
}

char* decodeDate(char* date) 
{
    if (date == NULL || strlen(date) == 0) return NULL;

    if (!isdigit(date[0])) return date;

    int year, month, day, hour = 0, minute = 0, second = 0;
    int is_utc = 0;

    if (sscanf(date, "%4d%2d%2dT%2d%2d%2dZ", &year, &month, &day, &hour, &minute, &second) == 6) is_utc = 1;
    else if (sscanf(date, "%4d%2d%2dT%2d%2d%2d", &year, &month, &day, &hour, &minute, &second) == 6) {}
    else if (sscanf(date, "%4d%2d%2d", &year, &month, &day) == 3) {}
    else return date;

    char* result = malloc(64);
    if (result == NULL) return NULL;
    if (hour == 0 && minute == 0 && second == 0) snprintf(result, 64, "%04d-%02d-%02d", year, month, day);
    else 
    {
        if (is_utc) snprintf(result, 64, "%04d-%02d-%02d %02d:%02d:%02d (UTC)", year, month, day, hour, minute, second);
        else snprintf(result, 64, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    }

    return result;
}

char* encodeDate(char* date)
{
    if (date == NULL || strlen(date) == 0) return NULL;
    if (!isdigit(date[0])) return NULL;

    int year, month, day, hour = 0, minute = 0, second = 0;

    if (sscanf(date, "%4d-%2d-%2d %2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) == 6) {}
    else if (sscanf(date, "%4d%2d%2dT%2d%2d%2dZ", &year, &month, &day, &hour, &minute, &second) == 6) {}
    else if (sscanf(date, "%4d%2d%2d", &year, &month, &day) == 3) {}
    else return "";

    char* result = malloc(20);
    if (result == NULL) return NULL;
    if (hour == 0 && minute == 0 && second == 0) snprintf(result, 20, "%04d-%02d-%02d", year, month, day);
    else snprintf(result, 20, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);

    return result;
}