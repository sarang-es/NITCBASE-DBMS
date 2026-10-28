#include "Algebra.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
using namespace std;


// Returns true if the given string represents a number
static bool isNumber(char *str)
{
    int len;
    float ignore;

    int ret = sscanf(str, "%f %n", &ignore, &len);

    return ret == 1 && len == strlen(str);
}


int counter=0;

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                    char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {

    counter=0;
    
    // Get the relation ID of the source relation
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Get the attribute catalog entry for the condition attribute
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);

    if (ret != SUCCESS) {
        return E_ATTRNOTEXIST;
    }

    // Convert the input string into an Attribute
    Attribute attrVal;
    int type = attrCatEntry.attrType;

    if (type == NUMBER) {
        if (isNumber(strVal)) {
            attrVal.nVal = atof(strVal);
        }
        else {
            return E_ATTRTYPEMISMATCH;
        }
    }
    else if (type == STRING) {
        strcpy(attrVal.sVal, strVal);
    }

    // Get the source relation catalog entry
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    int src_nAttrs = relCatEntry.numAttrs;

    // Prepare arrays to store attribute names and types
    char attr_names[src_nAttrs][ATTR_SIZE];
    int attr_types[src_nAttrs];

    // Copy the source relation's schema
    for (int i = 0; i < src_nAttrs; i++) {
        AttrCatEntry entry;

        ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);

        if (ret != SUCCESS) {
            return ret;
        }

        strcpy(attr_names[i], entry.attrName);
        attr_types[i] = entry.attrType;
    }

    // Create the target relation
    ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);

    if (ret != SUCCESS) {
        return ret;
    }

    // Open the target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    // Reset search indexes so searching starts from the beginning
    ret = RelCacheTable::resetSearchIndex(srcRelId);
    if (ret != SUCCESS) {
        Schema::closeRel(targetRel);
        Schema::deleteRel(targetRel);
        return ret;
    }

    ret = AttrCacheTable::resetSearchIndex(srcRelId, attr);
    if (ret != SUCCESS) {
        Schema::closeRel(targetRel);
        Schema::deleteRel(targetRel);
        return ret;
    }
    // Buffer for one source record
    Attribute record[src_nAttrs];

    // Search for matching records and insert them into the target
    while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {

        ret = BlockAccess::insert(targetRelId, record);

        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    cout << "Total comparisons made during selection: " << counter << endl;

    // Close the target relation
    Schema::closeRel(targetRel);

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

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {

    // Get the source relation ID
    int srcRelId = OpenRelTable::getRelId(srcRel);

    // Check whether the source relation is open
    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Get the source relation catalog entry
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    // Get the number of attributes
    int numAttrs = relCatEntry.numAttrs;

    // Arrays to store attribute names and types
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    // Copy the source relation's schema
    for (int i = 0; i < numAttrs; i++) {

        AttrCatEntry attrCatEntry;

        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    // Create the target relation
    int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);

    if (ret != SUCCESS) {
        return ret;
    }

    // Open the target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    // Reset the source relation's search index
    RelCacheTable::resetSearchIndex(srcRelId);

    // Buffer to store one record
    Attribute record[numAttrs];

    // Copy each source record into the target relation
    while (BlockAccess::project(srcRelId, record) == SUCCESS) {

        ret = BlockAccess::insert(targetRelId, record);

        if (ret != SUCCESS) {

            // Close and delete the target relation on failure
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);

            return ret;
        }
    }

    // Close the target relation
    Schema::closeRel(targetRel);

    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                     int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {

    // Get the source relation ID
    int srcRelId = OpenRelTable::getRelId(srcRel);

    // Check whether the source relation is open
    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Get the source relation catalog entry
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    // Number of attributes in the source relation
    int src_nAttrs = relCatEntry.numAttrs;

    // Store source attribute offsets and target attribute types
    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    // Check whether every target attribute exists in the source
    for (int i = 0; i < tar_nAttrs; i++) {

        AttrCatEntry attrCatEntry;
        int ret = AttrCacheTable::getAttrCatEntry(
            srcRelId, tar_Attrs[i], &attrCatEntry
        );

        if (ret != SUCCESS) {
            return E_ATTRNOTEXIST;
        }

        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
    }

    // Create the target relation
    int ret = Schema::createRel(
        targetRel, tar_nAttrs, tar_Attrs, attr_types
    );

    if (ret != SUCCESS) {
        return ret;
    }

    // Open the target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    // Reset the source relation's search index
    RelCacheTable::resetSearchIndex(srcRelId);

    // Buffer for a complete source record
    Attribute record[src_nAttrs];

    // Read every source record
    while (BlockAccess::project(srcRelId, record) == SUCCESS) {

        // Buffer for the projected record
        Attribute proj_record[tar_nAttrs];

        // Copy only the requested attributes
        for (int attr_iter = 0; attr_iter < tar_nAttrs; attr_iter++) {
            proj_record[attr_iter] = record[attr_offset[attr_iter]];
        }

        // Insert the projected record into the target relation
        ret = BlockAccess::insert(targetRelId, proj_record);

        if (ret != SUCCESS) {

            // Close the target relation
            Schema::closeRel(targetRel);

            // Delete the target relation
            Schema::deleteRel(targetRel);

            return ret;
        }
    }

    // Close the target relation
    Schema::closeRel(targetRel);

    return SUCCESS;
}