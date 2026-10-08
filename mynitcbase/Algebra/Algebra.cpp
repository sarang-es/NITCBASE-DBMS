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

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE])
{
    // RELATIONCAT and ATTRIBUTECAT cannot be modified.
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return E_NOTPERMITTED;
    }

    // Get the relation id.
    int relId = OpenRelTable::getRelId(relName);

    // Check whether the relation is open.
    if (relId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }

    // Get the relation catalog entry from the Relation Cache.
    RelCatEntry relCatEntry;

    int retVal =
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    // Check whether the number of attributes matches.
    if (relCatEntry.numAttrs != nAttrs)
    {
        return E_NATTRMISMATCH;
    }

    // Array to store the converted record.
    union Attribute recordValues[nAttrs];

    /*
     * Convert the input character strings into
     * Attribute values.
     */
    for (int i = 0; i < nAttrs; i++)
    {
        // Get the attribute catalog entry.
        AttrCatEntry attrCatEntry;

        retVal =
            AttrCacheTable::getAttrCatEntry(
                relId,
                i,
                &attrCatEntry
            );

        if (retVal != SUCCESS)
        {
            return retVal;
        }

        int type = attrCatEntry.attrType;

        if (type == NUMBER)
        {
            // Check whether the input is a valid number.
            if (!isNumber(record[i]))
            {
                return E_ATTRTYPEMISMATCH;
            }

            // Convert string to number.
            recordValues[i].nVal = atof(record[i]);
        }
        else if (type == STRING)
        {
            // Copy string into the Attribute.
            strcpy(recordValues[i].sVal, record[i]);
        }
    }

    // Insert the converted record into the relation.
    retVal = BlockAccess::insert(relId, recordValues);

    return retVal;
}