#include "LinkedListAPI.h"
#include "VCParser.h"
#include "VCHelpers.h"
#include "VCValidate.h"
#include "VCAPIHelpers.h"

// testFiles/invCard/testCard .vcf
// testFiles/invProp/testCard .vcf
// testFiles/valid/testCard .vcf

int main(void)
{
    char * filename = "./bin/cards/juneTest1.vcf";
    Card * obj;

    printf("Test1\n");
    VCardErrorCode err = createCard(filename, &obj);
    printf("Test2\n");

    char* carsStr = cardToString(obj);

    printf("Test3\n");

    printf("%s\n", carsStr);
    free(carsStr);

    // char* errStr = errorToString(err);
    // printf("%s\n", errStr);
    // free(errStr);

    // VCardErrorCode writeErr = writeCard("testOut.vcf", obj);
    // errStr = errorToString(writeErr);
    // printf("%s\n", errStr);
    // free(errStr);

    VCardErrorCode validateErr = validateCard(obj);
    char* errStr = errorToString(validateErr);
    printf("%s\n", errStr);
    free(errStr);

    Contact contact = getContact(filename, obj);
    printf("%s\n", contact.file_name);
    printf("%s\n", contact.name);
    printf("%s\n", contact.birthday);
    printf("%s\n", contact.anniversary);
    printf("%d\n", contact.prop_count);
    
    deleteCard(obj);
    return 0;
}