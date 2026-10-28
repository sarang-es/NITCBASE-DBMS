#include "Algebra.h"

#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
using namespace std;


bool isNumber(char *str) {
    int len;
    float ignore;
    int ret = sscanf(str, "%f %n", &ignore, &len);
    return ret == 1 && len == strlen(str);
    // f reads the actual floating point number and %n reads number of characters read into len
    // 123.01 -> len will become 6 and it matches strlen
    // 123.01abc -> len will become 6 and it doesnt match strlen - cause %f can read only float values
}


/* used to select all the records that satisfy a condition.
the arguments of the function are
- srcRel - the source relation we want to select from
- targetRel - the relation we want to select into. (ignore for now)
- attr - the attribute that the condition is checking
- op - the operator of the condition
- strVal - the value that we want to compare against (represented as a string)
*/

int count=0;
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {
    
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // get attr cat entry
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    if(ret != SUCCESS) {
        return E_ATTRNOTEXIST;
    }

     /*** Convert strVal (string) to an attribute of data type NUMBER or STRING ***/
     int type = attrCatEntry.attrType;
     Attribute attrVal;
     if(type == NUMBER) {
        if(isNumber(strVal)) {
            attrVal.nVal = atof(strVal);
        }
        else {
            return E_ATTRTYPEMISMATCH;
        }
     }
     else if(type == STRING) {
        strcpy(attrVal.sVal, strVal);
     }


    // Creating and opening target relation
    // task1: prepare arguments to all createRel()
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    int src_nAttrs = relCatEntry.numAttrs;
    
    char attrNames [src_nAttrs][ATTR_SIZE];
    int attrTypes [src_nAttrs];
    
    // fill the above arrays using data from attrCat
    for(int i=0; i < src_nAttrs; i++) {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    ret = Schema::createRel(targetRel, src_nAttrs, attrNames, attrTypes);
    if(ret != SUCCESS) {
        return ret;
    }

    // open the newly created relation and store the relId
    int targetRelId = OpenRelTable::openRel(targetRel);

    // if opening fails, return the value returned by above call
    if(targetRelId < 0 || targetRelId >= MAX_OPEN)
        return targetRelId;

    /*** Selecting and inserting records into the target relation ***/
    Attribute record[src_nAttrs];

    // first of all, reset the searchIndex
    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId, attr);

    //task: repeatedly call BlockAccess::Search function till no records left
    while(BlockAccess::search(srcRelId, record, attr, attrVal, op)==SUCCESS) {
        
        // insert the search result to new relation
        ret = BlockAccess::insert(targetRelId, record);
        if(ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    cout << "Number of records selected: " << count << endl;

    ret = Schema::closeRel(targetRel);
    if(ret != SUCCESS) {
        return ret;
    }
    return SUCCESS;
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]) {
    if (!strcmp(relName, RELCAT_RELNAME) || !strcmp(relName, ATTRCAT_RELNAME))
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);
    if (relId == E_RELNOTOPEN)
        return relId;

    RelCatEntry relCatBuf;
    RelCacheTable::getRelCatEntry(relId, &relCatBuf);

    // If no. of attributes mismatch, throw error
    if (relCatBuf.numAttrs != nAttrs)
        return E_NATTRMISMATCH;

    Attribute recordValues[nAttrs];

    for (int i = 0; i < nAttrs; i++) {
        // get the attrCat entry for the i'th attribute from the attrCache
        AttrCatEntry attrCatBuf;
        AttrCacheTable::getAttrCatEntry(relId, i, &attrCatBuf);

        int type = attrCatBuf.attrType;

        if (type == NUMBER) {
            if (isNumber(record[i])) {
                recordValues[i].nVal = atof(record[i]);
            } else {
                return E_ATTRTYPEMISMATCH;
            }
        } else if (type == STRING) {
            strcpy(recordValues[i].sVal, record[i]);
        }
    }

    // insert the record by calling BlockAccess::insert()
    int ret = BlockAccess::insert(relId, recordValues);
    return ret;
}

// This Project function simply copies all records to a new relation
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {

    int srcRelId = OpenRelTable::getRelId(srcRel);

    if(srcRelId < 0 || srcRelId >= MAX_OPEN) 
        return E_RELNOTOPEN;

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    int src_nAttrs = relCatEntry.numAttrs;

    char attrNames[src_nAttrs][ATTR_SIZE];
    int attrTypes[src_nAttrs];

    for(int i=0; i < src_nAttrs; i++) {
        
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    // arguments are ready, create target relation
    int ret = Schema::createRel(targetRel, src_nAttrs, attrNames, attrTypes);

    if(ret != SUCCESS) {
        return ret;
    }

    // now open the new relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if(targetRelId < 0 || targetRelId >= MAX_OPEN) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/
    // search index must be reset before using linearsearch or project
    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[src_nAttrs];

    while(BlockAccess::project(srcRelId, record)==SUCCESS) {
        // record will contain the next record
        
        ret = BlockAccess::insert(targetRelId, record);
        
        // if insertion fails, close and delete the target relation
        if(ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    } 

    OpenRelTable::closeRel(targetRelId);
    return SUCCESS;
}

// This project takes only specified columns to target relation
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {

    int srcRelId = OpenRelTable::getRelId(srcRel);

    if(srcRelId < 0 || srcRelId >= MAX_OPEN) {
        return E_RELNOTOPEN;
    } 

    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    int src_nAttrs = relCatEntry.numAttrs;
    
    int attr_Offset[tar_nAttrs];
    int attr_Types[tar_nAttrs];

    for(int i=0; i < tar_nAttrs; i++) {
        AttrCatEntry attrCatEntry;
        int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
        
        // if not such attribute
        if(ret != SUCCESS) {
            return E_ATTRNOTEXIST;
        }

        attr_Offset[i] = attrCatEntry.offset;
        attr_Types[i] = attrCatEntry.attrType;
    }

    // Create the target Relation
    int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_Types);

    if(ret != SUCCESS) {
        return ret;
    }

    // open the new relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if(targetRelId < 0 || targetRelId >= MAX_OPEN ) {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/

    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[src_nAttrs];

    while(BlockAccess::project(srcRelId, record)==SUCCESS) {
        
        // but we dont want to copy all attributes in record to new relation
        Attribute proj_record[tar_nAttrs];

        for(int i=0; i < tar_nAttrs; i++) {
            // no need to take the value outside, just assign the union
            proj_record[i] = record[attr_Offset[i]];
        } 
        ret = BlockAccess::insert(targetRelId, proj_record);
        
        if(ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    return SUCCESS;
}
