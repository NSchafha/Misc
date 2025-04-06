#include "LinkedListAPI.h"
#include "VCParser.h"
#include "VCHelpers.h"
#include "VCValidate.h"


VCardErrorCode createProperty(Property* prop, const char* propStr)
{
    prop->name = (char*)malloc(sizeof(char) * 1);
    prop->name[0] = '\0';
    prop->group = (char*)malloc(sizeof(char) * 1);
    prop->group[0] = '\0';

    prop->parameters = initializeList(&parameterToString, &deleteParameter, &compareParameters);
    prop->values = initializeList(&valueToString, &deleteValue, &compareValues);

    char* copy = (char*)malloc(strlen(propStr) + 1);
    if (copy == NULL) return OTHER_ERROR;

    strcpy(copy, propStr);

    //Split group.name;params : values
    const char* splitPos = strchr(copy, ':');
    if (splitPos == NULL)
    {
        free(copy);
        return INV_PROP;
    }   

    //group.name;params
    size_t mixedSize = (size_t)(splitPos - copy);
    char* mixedStr = (char*)malloc(mixedSize + 1);
    if (mixedStr == NULL)
    {
        free(copy);
        return OTHER_ERROR;
    }
    strncpy(mixedStr, copy, mixedSize + 1);
    mixedStr[mixedSize] = '\0';

    //group.
    const char* endOfGroup = strchr(mixedStr, '.');
    const char* endOfName = strchr(mixedStr, ';');

    size_t groupSize = 0;
    if (endOfGroup != NULL)
    {
        groupSize = endOfGroup - mixedStr;
        prop->group = (char*)realloc(prop->group, groupSize + 1);

        if (prop->group == NULL) 
        {
            free(copy);
            free(mixedStr);
            return OTHER_ERROR;
        }

        strncpy(prop->group, mixedStr, groupSize + 1);
        prop->group[groupSize] = '\0';
    }

    const char* nameStart = (endOfGroup != NULL) ? endOfGroup + 1 : mixedStr;

    size_t nameSize = 0;
    if (endOfName != NULL) nameSize = endOfName - nameStart;
    else nameSize = mixedSize - groupSize;

    if (nameSize > 0)
    {
        prop->name = (char*)realloc(prop->name, nameSize + 1);
        if (prop->name == NULL)
        {
            free(copy);
            free(mixedStr);
            return OTHER_ERROR;
        }
        strncpy(prop->name, nameStart, nameSize);
        prop->name[nameSize] = '\0';
    } 
    else return INV_PROP;

    if (endOfName != NULL)
    {
        const char* paramsStart = endOfName + 1;
        size_t paramsSize = strlen(mixedStr) - (paramsStart - mixedStr);

        if (paramsSize > 0)
        {
            char* paramStr = (char*)malloc(paramsSize + 1);
            if (paramStr == NULL)
            {
                free(copy);
                free(mixedStr);
                return OTHER_ERROR;
            }
            strncpy(paramStr, paramsStart, paramsSize);
            paramStr[paramsSize] = '\0';

            VCardErrorCode paramErr = createParameterList(prop->parameters, paramStr);
            if (paramErr != OK) return paramErr;

            free(paramStr);
        }
    }

    createValueList(prop->values, splitPos + 1);
    
    free(copy);
    free(mixedStr);
    return OK;
}

VCardErrorCode createDateTime(DateTime* date, const char* dateStr)
{
    if (date == NULL || dateStr == NULL || strlen(dateStr) == 0) return INV_PROP;

    char* actualDateStr = strchr(dateStr, ':');
    if (actualDateStr == NULL || *(actualDateStr + 1) == '\0') return INV_PROP;
    actualDateStr++;

    date->UTC = 0;
    date->isText = 0;
    date->date = (char*)malloc(1);
    date->time = (char*)malloc(1);
    date->text = (char*)malloc(1);

    if (date->date == NULL || date->time == NULL || date->text == NULL) return OTHER_ERROR;

    date->date[0] = '\0';
    date->time[0] = '\0';
    date->text[0] = '\0';

    char* copy = (char*)malloc(strlen(actualDateStr) + 1);
    if (copy == NULL) return OTHER_ERROR;

    strcpy(copy, actualDateStr);

    size_t len = strlen(copy);

    if (copy[len - 1] == 'Z')
    {
        date->UTC = 1;
        copy[len - 1] = '\0';
    }

    if (strstr(copy, "circa") != NULL)
    {
        date->isText = 1;
        date->text = (char*)realloc(date->text, strlen(copy) + 1);
        if (date->text == NULL)
        {
            free(copy);
            return OTHER_ERROR;
        }
        strcpy(date->text, copy);
        return OK;
    }

    char* tFound = strchr(copy, 'T');

    if (tFound == NULL)
    {
        date->date = (char*)realloc(date->date, strlen(copy) + 1);

        if (date->date == NULL)
        {
            free(copy);
            return OTHER_ERROR;
        }

        strcpy(date->date, copy);
    } else
    {
        *tFound = '\0';
        date->date = (char*)realloc(date->date, strlen(copy) + 1);
        date->time = (char*)realloc(date->time, strlen(tFound + 1) + 1);

        if (date->date == NULL || date->time == NULL)
        {
            free(copy);
            return OTHER_ERROR;
        }

        strcpy(date->date, copy);
        strcpy(date->time, tFound + 1);
    }

    free(copy);
    return OK;
}

VCardErrorCode createParameterList(List* params, const char* paramsStr)
{
    if (params == NULL || paramsStr == NULL || strlen(paramsStr) == 0) return INV_PROP;

    char* copy = malloc(strlen(paramsStr) + 1);
    if (copy == NULL) return OTHER_ERROR;

    strcpy(copy, paramsStr);

    char* token = copy;
    do
    {
        char* equalSign = strchr(token, '=');
        if (equalSign == NULL)
        {
            free(copy);
            return INV_PROP;
        }

        *equalSign = '\0';
        char* name = token;
        char* value = equalSign + 1;
        
        Parameter* param = (Parameter*)malloc(sizeof(Parameter));
        if (param == NULL)
        {
            free(copy);
            return OTHER_ERROR;
        }

        param->name = (char*)malloc(strlen(name) + 1);
        param->value = (char*)malloc(strlen(value) + 1);

        if (param->name == NULL || param->value == NULL)
        {
            free(param);
            free(copy);
            return OTHER_ERROR;
        }

        strcpy(param->name, name);
        strcpy(param->value, value);

        insertBack(params, param);

        token = strchr(token, ';');
        if (token != NULL) token++;
    } while (token != NULL && *token != '\0');

    free(copy);
    return OK;
}

VCardErrorCode createValueList(List* values, const char* valueStr)
{
    if (values == NULL || valueStr == NULL || strlen(valueStr) == 0) return INV_PROP;

    char* copy = malloc(strlen(valueStr) + 1);
    if (copy == NULL) return OTHER_ERROR;
    strcpy(copy, valueStr);

    char* token = copy;
    char* lastToken = copy;
    while ((token = strpbrk(token, ";")) != NULL)
    {
        size_t len = token - lastToken;

        char* newValue = (char*)malloc(len + 1);
        if (newValue == NULL)
        {
            free(copy);
            return OTHER_ERROR;
        }

        strncpy(newValue, lastToken, len);
        newValue[len] = '\0';
        insertBack(values, newValue);

        token++;
        lastToken = token;
    }

    if (*lastToken != '\0')
    {
        char* newValue = (char*)malloc(strlen(lastToken) + 1);
        if (newValue == NULL)
        {
            free(copy);
            return OTHER_ERROR;
        }
        strcpy(newValue, lastToken);
        insertBack(values, newValue);
    }

    free(copy);
    return OK;
}


char* bdayText(DateTime* date)
{
    char * fullBirStr = malloc(1);
    fullBirStr[0] = '\0';

    if (date != NULL)
    {
        char* birStr = dateToString(date);
        int birLen = 5 + strlen(birStr) + 1;

        char* tmp = realloc(fullBirStr, birLen);
        if (tmp == NULL)
        {
            free(fullBirStr);
            free(birStr);
            return NULL;
        }
        fullBirStr = tmp;

        sprintf(fullBirStr, "BDAY:%s", birStr);
        free(birStr);
    }

    return fullBirStr;

}

char* annText(DateTime* date)
{
    char * fullAnnStr = malloc(1);
    fullAnnStr[0] = '\0';

    if (date != NULL)
    {
        char* annStr = dateToString(date);
        int annLen = 12 + strlen(annStr) + 1;

        char* tmp = realloc(fullAnnStr, annLen);
        if (tmp == NULL)
        {
            free(fullAnnStr);
            free(annStr);
            return NULL;
        }
        fullAnnStr = tmp;

        sprintf(fullAnnStr, "ANNIVERSARY:%s", annStr);
        free(annStr);
    }

    return fullAnnStr;
}


int checkNextChar(FILE* fptr)
{
    int ch = fgetc(fptr);
    if (ch == EOF) return EOF;

    ungetc(ch, fptr);

    return ch;
}

VCardErrorCode removeCRLF(char* string)
{
    size_t len = strlen(string);

    int hasn = 0;
    int hasr = 0;
    if (len > 0 && (string[len - 1] == '\n' || string[len - 1] == '\r'))
    {
        string[len - 1] = '\0';
        hasn = 1;
        if (len > 1 && string[len - 2] == '\r')
        {
            hasr = 1;
            string[len - 2] = '\0';
        }   
    }   
    if (hasn && hasr) return OK;
    else return OK;
    //else return INV_PROP;
}

void removeSpace(char* string)
{
    if (string == NULL) return;

    int i = 0;
    while (string[i] == ' ' || string[i] == '\t') i++;

    if (i > 0) memmove(string, string + i, strlen(string) - i + 1);
}
