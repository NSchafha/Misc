#include "LinkedListAPI.h"
#include "VCParser.h"
#include "VCHelpers.h"


VCardErrorCode validateFileName(const char* fileName)
{
    if (fileName == NULL) return INV_FILE;
    int len = strlen(fileName);
    if (len >= 4 && strncmp(fileName + len - 4, ".vcf", 4) == 0) return OK;
    if (len >= 6 && strncmp(fileName + len - 6, ".vcard", 6) == 0) return OK;
    return INV_FILE;
}

VCardErrorCode validateFileCard(FILE* fptr, Card* obj)
{
    char buffer[78];

    if (fgets(buffer, sizeof(buffer), fptr) == NULL)
    {
        return INV_CARD;
    } 
    if (strncmp(buffer, "BEGIN:VCARD", 11) != 0)
    {
        return INV_CARD;
    }
    
    if (fgets(buffer, sizeof(buffer), fptr) == NULL)
    {
        return INV_CARD;
    } 
    if (strncmp(buffer, "VERSION:4.0", 11) != 0)
    {
        return INV_CARD;
    }

    int startPos = ftell(fptr);
    if (startPos == -1)
    {
        fclose(fptr);
        return OTHER_ERROR;
    }
    
    int fn = 0;
    while (fgets(buffer, sizeof(buffer), fptr) != NULL)
    {
        VCardErrorCode err = removeCRLF(buffer);
        if (err != OK)
        {
            return err;
        }
        if (strncmp(buffer, "FN:", 3) == 0)
        {
            fn = 1;

            char ch = checkNextChar(fptr);
            char propBuffer[320] = "";
            strcpy(propBuffer, buffer);

            while (ch == ' ' || ch == '\t')
            {
                if (fgets(buffer, sizeof(buffer), fptr) == NULL)
                {
                    break;
                }
                
                err = removeCRLF(buffer);
                if (err != OK)
                {
                    return err;
                }
                removeSpace(buffer);

                if (strlen(propBuffer) + strlen(buffer) < 319)
                {
                    strcat(propBuffer, buffer);
                }
                ch = checkNextChar(fptr);
            }

            Property* prop = (Property*)malloc(sizeof(Property));

            if (prop == NULL)
            {
                fclose(fptr);
                return OTHER_ERROR;
            }
        
            VCardErrorCode err = createProperty(prop, propBuffer);
            if (err != OK)
            {
                fclose(fptr);
                deleteProperty(prop);
                return err;
            }

            obj->fn = prop;
            break;
        }
    }
    if (!fn) 
    {
        return INV_PROP;
    }

    int end = 0;
    while (fgets(buffer, sizeof(buffer), fptr) != NULL)
    {
        if (strncmp(buffer, "END:VCARD", 9) == 0)
        {
            end = 1;
            break;
        }
    }
    if (!end)
    {
        return INV_CARD;
    }

    while (fgets(buffer, sizeof(buffer), fptr) != NULL)
    {
        if (strlen(buffer) > 1)
        {
            return INV_CARD;
        }
    }

    if (fseek(fptr, startPos, SEEK_SET) != 0)
    {
        return OTHER_ERROR;
    }

    return OK;
}


VCardErrorCode validateDateTime(const DateTime* date)
{
    if (date->date == NULL || date->time == NULL || date->text == NULL) return INV_DT;

    if (date->isText)
    {
        if (date->UTC) return INV_DT;
        if (strlen(date->date) != 0 || strlen(date->time) != 0) return INV_DT;
        if (strlen(date->text) == 0) return INV_DT;
    }
    else
    {
        if (strlen(date->text) != 0) return INV_DT;
    }

    return OK;
}

VCardErrorCode validateProperty(const Property* prop)
{
    if (prop == NULL) return INV_PROP;

    if (prop->name == NULL || prop->group == NULL || prop->parameters == NULL || prop->values == NULL) return INV_PROP;

    if (strlen(prop->name) == 0) return INV_PROP;

    if (getLength(prop->values) == 0) return INV_PROP;

    if (getLength(prop->parameters) > 0)
    {
        ListIterator iter = createIterator(prop->parameters);
        void* elem;
        while ((elem = nextElement(&iter)) != NULL)
        {
            Parameter* param = (Parameter*)elem;
            VCardErrorCode err = validateParameter(param);
            if (err != OK) return err;
            param = nextElement(&iter);
        }
    }

    ListIterator iter = createIterator(prop->values);
    void* elem;
    while ((elem = nextElement(&iter)) != NULL)
    {
        char* value = (char*)elem;
        if (value == NULL) return INV_PROP;
        value = nextElement(&iter);
   }

    return OK;
}

VCardErrorCode cardnalityCheck(const char* name)
{
    static int kind = 0;
    static int n = 0;
    static int gender = 0;
    static int prodid = 0;
    static int rev = 0;
    static int uid = 0;


    if (strcmp(name, "VERSION") == 0 || strcmp(name, "FN") == 0)
    {
        kind = 0;
        n = 0;
        gender = 0;
        prodid = 0;
        rev = 0;
        uid = 0;
        return INV_CARD;
    }

    if (strcmp(name, "BDAY") == 0 || strcmp(name, "ANN") == 0)
    {
        kind = 0;
        n = 0;
        gender = 0;
        prodid = 0;
        rev = 0;
        uid = 0;
        return INV_DT;
    }

    if (strcmp(name, "KIND") == 0)
    {
        kind++;
        if (kind > 1)
        {
            kind = 0;
            return INV_PROP;
        }
    }

    if (strcmp(name, "N") == 0)
    {
        n++;
        if (n > 1)
        {
            n = 0;
            return INV_PROP;
        }
    }

    if (strcmp(name, "GENDER") == 0)
    {
        gender++;
        if (gender > 1)
        {
            gender = 0;
            return INV_PROP;
        }
    }
    
    if (strcmp(name, "PRODID") == 0)
    {
        prodid++;
        if (prodid > 1)
        {
            prodid = 0;
            return INV_PROP;
        }
    }

    if (strcmp(name, "REV") == 0)
    {
        rev++;
        if (rev > 1)
        {
            rev = 0;
            return INV_PROP;
        }
    }

    if (strcmp(name, "UID") == 0)
    {
        uid++;
        if (uid > 1)
        {
            uid = 0;
            return INV_PROP;
        }
    }

    return OK;
}

VCardErrorCode validateParameter(const Parameter* param)
{
    if (param == NULL) return INV_PROP;

    if (param->name == NULL || param->value == NULL) return INV_PROP;

    if (strlen(param->name) == 0 || strlen(param->value) == 0) return INV_PROP;

    return OK;
}