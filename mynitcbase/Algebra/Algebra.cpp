#include "Algebra.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>


// Returns true if the given string represents a number
static bool isNumber(char *str)
{
    int len;
    float ignore;

    int ret = sscanf(str, "%f %n", &ignore, &len);

    return ret == 1 && len == strlen(str);
}


int Algebra::select(
    char srcRel[ATTR_SIZE],
    char targetRel[ATTR_SIZE],
    char attr[ATTR_SIZE],
    int op,
    char strVal[ATTR_SIZE])
{
    // Find relId of source relation
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }


    // Get metadata of the attribute used in WHERE
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        srcRelId,
        attr,
        &attrCatEntry
    );

    if (ret != SUCCESS)
    {
        return ret;
    }


    // Convert strVal into proper Attribute type
    Attribute attrVal;

    if (attrCatEntry.attrType == NUMBER)
    {
        if (isNumber(strVal))
        {
            attrVal.nVal = atof(strVal);
        }
        else
        {
            return E_ATTRTYPEMISMATCH;
        }
    }
    else if (attrCatEntry.attrType == STRING)
    {
        strcpy(attrVal.sVal, strVal);
    }


    // Start search from beginning
    RelCacheTable::resetSearchIndex(srcRelId);


    // 5. Get relation metadata
    RelCatEntry relCatEntry;

    RelCacheTable::getRelCatEntry(
        srcRelId,
        &relCatEntry
    );


    // Print column names
    printf("|"); 

    for (int i = 0; i < relCatEntry.numAttrs; i++)
    {
        AttrCatEntry columnEntry;

        AttrCacheTable::getAttrCatEntry(
            srcRelId,
            i,
            &columnEntry
        );

        printf(" %s |", columnEntry.attrName);
    }

    printf("\n");


    // Repeatedly find matching records
    while (true)
    {
        RecId searchRes = BlockAccess::linearSearch(
            srcRelId,
            attr,
            attrVal,
            op
        );

        // No more matches
        if (searchRes.block == -1 &&
            searchRes.slot == -1)
        {
            break;
        }


        // Read actual matching record from buffer
        RecBuffer block(searchRes.block);

        Attribute record[relCatEntry.numAttrs];

        block.getRecord(
            record,
            searchRes.slot
        );


        // Print the record
        printf("|");

        for (int i = 0; i < relCatEntry.numAttrs; i++)
        {
            AttrCatEntry columnEntry;

            AttrCacheTable::getAttrCatEntry(
                srcRelId,
                i,
                &columnEntry
            );

            if (columnEntry.attrType == NUMBER)
            {
                printf(" %g |", record[i].nVal);
            }
            else
            {
                printf(" %s |", record[i].sVal);
            }
        }

        printf("\n");
    }


    return SUCCESS;
}