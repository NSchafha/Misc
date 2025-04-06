#include "LinkedListAPI.h"
#include "VCParser.h"
#include "VCHelpers.h"
#include "VCValidate.h"


VCardErrorCode createCard(char* fileName, Card** obj)
{
    VCardErrorCode filenameErr = validateFileName(fileName);

    if (filenameErr != OK)
    {
        return filenameErr;
    }

    FILE *fptr = fopen(fileName, "r");

    if (fptr == NULL) 
    {
        return INV_FILE;
    }

    (*obj) = (Card*)malloc(sizeof(Card));

    if ((*obj) == NULL)
    {
        fclose(fptr);
        return INV_CARD;
    }


    (*obj)->fn = NULL;
    (*obj)->optionalProperties = initializeList(&propertyToString, &deleteProperty, &compareProperties);
    (*obj)->birthday = NULL;
    (*obj)->anniversary = NULL;

    VCardErrorCode validateErr = validateFileCard(fptr, (*obj));

    if (validateErr != OK)
    {
        deleteCard((*obj));
        fclose(fptr);
        return validateErr;
    }

    //Skip over FN in while loop, 80 = _\t + 75 + \n\r\0
    char buffer[160];

    while (fgets(buffer, sizeof(buffer), fptr) != NULL)
    {
        VCardErrorCode clrErr = removeCRLF(buffer);
        if (clrErr != OK)
        {
            return clrErr;
        }

        if (strncmp(buffer, "END:VCARD", 9) == 0)
        {
            break;
        }
        if (strncmp(buffer, "FN:", 3) == 0)
        {
            continue;
        }
        if (strncmp(buffer, " ", 1) == 0 || strncmp(buffer, "\t", 1) == 0)
        {
            continue;
        }
        if (strncmp(buffer, "BDAY", 4) == 0)
        {
            DateTime* bday = (DateTime*)malloc(sizeof(DateTime));
            if (bday == NULL)
            {
                fclose(fptr);
                return INV_DT;
            }

            VCardErrorCode bdayErr = createDateTime(bday, buffer);
            if (bdayErr != OK)
            {
                fclose(fptr);
                deleteDate(bday);
                return bdayErr;
            }

            (*obj)->birthday = bday;
            continue;
        }
        if (strncmp(buffer, "ANNIVERSARY", 11) == 0)
        {
            DateTime* ann = (DateTime*)malloc(sizeof(DateTime));
            if (ann == NULL)
            {
                fclose(fptr);
                return INV_DT;
            }

            VCardErrorCode annErr = createDateTime(ann, buffer);
            if (annErr != OK)
            {
                fclose(fptr);
                deleteDate(ann);
                return annErr;
            }

            (*obj)->anniversary = ann;
            continue;
        }

        char propBuffer[320] = "";
        strcpy(propBuffer, buffer);

        while (1)
        {
            int nextChar = fgetc(fptr);
            if (nextChar != ' ' && nextChar != '\t')
            {
                if (nextChar != EOF)
                {
                    ungetc(nextChar, fptr);
                }                    
                break;
            }

            if (fgets(buffer, sizeof(buffer), fptr) == NULL)
            {
                break;
            }

            VCardErrorCode clrErr = removeCRLF(buffer);
            if (clrErr != OK)
            {
                return clrErr;
            }

            removeSpace(buffer);

            if (strlen(propBuffer) + strlen(buffer) < sizeof(propBuffer) - 1)
            {
                strcat(propBuffer, buffer);
            }
            else
            {
                break;
            }
        }

        Property* prop = (Property*)malloc(sizeof(Property));

        if (prop == NULL)
        {
            fclose(fptr);
            return INV_PROP;
        }
        
        VCardErrorCode propErr = createProperty(prop, propBuffer);

        if (propErr != OK)
        {
            fclose(fptr);
            deleteProperty(prop);
            return propErr;
        }

        insertBack((*obj)->optionalProperties, prop);
    }
    fclose(fptr);
    return OK;
}

void deleteCard(Card* obj)
{
    if (obj == NULL)
    {
        return;
    }

    deleteProperty(obj->fn);

    freeList(obj->optionalProperties);
    
    deleteDate(obj->birthday);
    deleteDate(obj->anniversary);

    free(obj);
}

char* cardToString(const Card* obj)
{
    if (obj == NULL)
    {
        return NULL;
    }

    int len = 0;

    char* fnStr = propertyToString(obj->fn);
    char* propStr = toString(obj->optionalProperties);
    char* fullBirStr = bdayText(obj->birthday);
    char* fullAnnStr = annText(obj->anniversary);

    len += strlen(fnStr) + strlen(propStr) + strlen(fullBirStr) + strlen(fullAnnStr) + 1;
    char* str = (char*)malloc(len);
    if (str == NULL)
    {
        return NULL;
    }

    sprintf(str, "%s%s%s%s", fnStr, propStr, fullBirStr, fullAnnStr);

    free(fnStr);
    free(propStr);
    free(fullBirStr);
    free(fullAnnStr);
    return str;
}

char* errorToString(VCardErrorCode err)
{
    char* errStr = (char*)malloc(sizeof(char) * 20);

    switch (err)
    {
        case OK:
            strcpy(errStr, "OK");
            return errStr;
        case INV_FILE:
            strcpy(errStr, "INV_FILE");
            return errStr;
        case INV_CARD:
            strcpy(errStr, "INV_CARD");
            return errStr;
        case INV_PROP:
            strcpy(errStr, "INV_PROP");
            return errStr;
        case INV_DT:
            strcpy(errStr, "INV_DT");
            return errStr;
        case WRITE_ERROR:
            strcpy(errStr, "WRITE_ERROR");
            return errStr;
        case OTHER_ERROR:
            strcpy(errStr, "OTHER_ERROR");
            return errStr;
        default:
            strcpy(errStr, "Invalid error code");
            return errStr;
    }
}


void deleteProperty(void* toBeDeleted)
{
    if (toBeDeleted == NULL)
    {
        return;
    }

    Property* toDelete = (Property*)toBeDeleted;

    free(toDelete->name);
    free(toDelete->group);

    freeList(toDelete->parameters);
    freeList(toDelete->values);

    free(toDelete);
}

int compareProperties(const void* first,const void* second)
{
    if (first == NULL || second == NULL)
    {
        return false;
    }

    Property* pro1 = (Property*)first;
    Property* pro2 = (Property*)second;

    if (strcmp(pro1->name, pro2->name) != 0)
    {
        return false;
    }
    
    if (strcmp(pro1->group, pro2->group) != 0)
    {
        return false;
    }
    
    if (getLength(pro1->parameters) != getLength(pro2->parameters))
    {
        return false;
    }
    
    if (getLength(pro1->values) != getLength(pro2->values))
    {
        return false;
    }

    //Add testing individual parameters and values

    return true;
}

char* propertyToString(void* prop)
{
    if (prop == NULL)
    {
        char* temp = (char*)malloc(sizeof(char) * 1);
        temp[0] = '\0';
        return temp;
    }

    Property* pro = (Property*)prop;

    char* paramStr = toString(pro->parameters);
    char* valStr = toString(pro->values);

    int len = strlen(pro->name) + strlen(pro->group) + strlen(paramStr) + strlen(valStr) + 5;
    char* str = (char*)malloc(sizeof(char)*len);

    size_t paramLen = strlen(paramStr);
    if (paramLen > 0 && paramStr[paramLen - 1] == ';') {
        paramStr[paramLen - 1] = '\0';
    }

    size_t valLen = strlen(valStr);
    if (valLen > 0 && valStr[valLen - 1] == ';') {
        valStr[valLen - 1] = '\0';
    }


    if (pro->group == NULL || strlen(pro->group) == 0)
    {
        sprintf(str, "%s%s:%s", pro->name, paramStr, valStr);
    }
    else
    {
        sprintf(str, "%s.%s%s:%s", pro->group, pro->name, paramStr, valStr);
    }
        
    free(paramStr);
    free(valStr);

    return str;
}


void deleteParameter(void* toBeDeleted)
{
    if (toBeDeleted == NULL)
    {
        return;
    }

    Parameter* toDelete = (Parameter*)toBeDeleted;

    free(toDelete->name);
    free(toDelete->value);
    free(toDelete);
}

int compareParameters(const void* first,const void* second)
{
    if (first == NULL || second == NULL)
    {
        return false;
    }

    Parameter* par1 = (Parameter*)first;
    Parameter* par2 = (Parameter*)second;

    if (strcmp(par1->name, par2->name) != 0)
    {
        return false;
    }
    
    if (strcmp(par1->value, par2->value) != 0)
    {
        return false;
    }

    return true;
}

char* parameterToString(void* param)
{
    if (param == NULL)
    {
        char* temp = (char*)malloc(sizeof(char) * 1);
        temp[0] = '\0';
        return temp;
    }

    Parameter* par = (Parameter*)param;

    int len = strlen(par->name) + strlen(par->value) + 3;
	char *str = (char*)malloc(sizeof(char)*len);

	sprintf(str, ";%s=%s", par->name, par->value);
    
    return str;
}


void deleteValue(void* toBeDeleted)
{
    if (toBeDeleted == NULL)
    {
        return;
    }

    char* toDelete = (char*)toBeDeleted;

    free(toDelete);
}

int compareValues(const void* first,const void* second)
{
    if (first == NULL || second == NULL)
    {
        return false;
    }

    char* val1 = (char*)first;
    char* val2 = (char*)second;

    return (strcmp(val1, val2) == 0) ? true : false;
}

char* valueToString(void* val)
{
    if (val == NULL)
    {
        char* temp = (char*)malloc(sizeof(char) * 1);
        temp[0] = '\0';
        return temp;
    }

    char* st = (char*)val;

    int len = strlen(st) + 2;
	char *str = (char*)malloc(sizeof(char)*len);

	sprintf(str, "%s;", st);
    
    return str;
}


void deleteDate(void* toBeDeleted)
{
    if (toBeDeleted == NULL)
    {
        return;
    }

    DateTime* toDelete = (DateTime*)toBeDeleted;

    free(toDelete->date);
    free(toDelete->time);
    free(toDelete->text);

    free(toDelete);
}

int compareDates(const void* first,const void* second)
{
    if (first == NULL || second == NULL)
    {
        return false;
    }

    DateTime* date1 = (DateTime*)first;
    DateTime* date2 = (DateTime*)second;

    if (date1->UTC != date2->UTC || date1->isText != date2->isText)
    {
        return false;
    }

    if (strcmp(date1->date, date2->date) != 0)
    {
        return false;
    }
    
    if (strcmp(date1->time, date2->time) != 0)
    {
        return false;
    }

    if (strcmp(date1->text, date2->text) != 0)
    {
        return false;
    }

    return true;
}

char* dateToString(void* date)
{
    if (date == NULL)
    {
        char* temp = (char*)malloc(1);
        temp[0] = '\0';
        return temp;
    }

    DateTime* dateTime = (DateTime*)date;

    if (dateTime->isText)
    {
        char* str = (char*)malloc(strlen(dateTime->text) + 1);
        if (str != NULL)
        {
            strcpy(str, dateTime->text);
        }
        return str;
    }

    size_t len = strlen(dateTime->date) + strlen(dateTime->time) + 3;
    char* str = (char*)malloc(len);

    if (strlen(dateTime->date) > 0 && strlen(dateTime->time) > 0)
    {
        sprintf(str, "%sT%s", dateTime->date, dateTime->time);
    }
    else if (strlen(dateTime->date) > 0)
    {
        sprintf(str, "%s", dateTime->date);
    }
    else if (strlen(dateTime->time) > 0)
    {
        sprintf(str, "T%s", dateTime->time);
    }

    if (dateTime->UTC)
    {
        strcat(str, "Z");
    }

    return str;
}



VCardErrorCode writeCard(const char* fileName, const Card* obj)
{
    if (obj == NULL) return WRITE_ERROR;

    VCardErrorCode filenameErr = validateFileName(fileName);
    if (filenameErr != OK) return WRITE_ERROR;

    FILE * fptr = fopen(fileName, "w");
    if (fptr == NULL) return WRITE_ERROR;


    fprintf(fptr, "BEGIN:VCARD\r\n");
    fprintf(fptr, "VERSION:4.0\r\n");


    if (obj->fn == NULL) return WRITE_ERROR;
    char* fnStr = propertyToString(obj->fn);
    fprintf(fptr, "%s\r\n", fnStr);

    free(fnStr);


    if (obj->birthday != NULL) 
    {
        char* fullBirStr = bdayText(obj->birthday);
        fprintf(fptr, "%s\r\n", fullBirStr);
        free(fullBirStr);
    }

    if (obj->anniversary != NULL) 
    {
        char* fullAnnStr = annText(obj->anniversary);
        fprintf(fptr, "%s\r\n", fullAnnStr);
        free(fullAnnStr);
    }

    
    ListIterator iter = createIterator(obj->optionalProperties);
    void * elem;
    while ((elem = nextElement(&iter)) != NULL)
    {
        Property* prop = (Property*)elem;
        
        char* propStr = propertyToString(prop);
        fprintf(fptr, "%s\r\n", propStr);
        free(propStr);
    }

    fprintf(fptr, "END:VCARD\r\n");


    fclose(fptr);
    return OK;
}

VCardErrorCode validateCard(const Card* obj)
{
    if (obj == NULL) return INV_CARD;

    if (obj->fn == NULL) return INV_CARD;
    else
    {
        VCardErrorCode fnErr = validateProperty(obj->fn);
        if (fnErr != OK) return fnErr;
    }


    if (obj->optionalProperties == NULL) return INV_CARD;

    ListIterator iter = createIterator(obj->optionalProperties);
    void * elem;
    while ((elem = nextElement(&iter)) != NULL)
    {
        Property* prop = elem;

        VCardErrorCode propErr = validateProperty(prop);
        if (propErr != OK) return propErr;
        
        VCardErrorCode cardnalityErr = cardnalityCheck(prop->name);
        if (cardnalityErr != OK) return cardnalityErr;
    }


    if (obj->birthday != NULL)
    {
        VCardErrorCode bdayErr = validateDateTime(obj->birthday);
        if (bdayErr != OK) return bdayErr;
    }

    if (obj->anniversary != NULL)
    {
        VCardErrorCode annErr = validateDateTime(obj->anniversary);
        if (annErr != OK) return annErr;
    }

    return OK;
}

