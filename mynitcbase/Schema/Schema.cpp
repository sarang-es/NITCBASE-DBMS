#include "Schema.h"

#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]) {
    int ret = OpenRelTable::openRel(relName);
    if(ret >= 0) {
        return SUCCESS;
    }
    return ret;
    // OpenRelTable::openrel returns relid which is positive is successful, otherwise a negative error value
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
    // rel cat and attr cat should not be closed
    if(strcmp(relName, RELCAT_RELNAME)==0 || strcmp(relName, ATTRCAT_RELNAME)==0) {
        return E_NOTPERMITTED;
    }

    // get the relId of relation
    // this call returns relId if relation is open, else an error E_RELNOTOPEN
    int relId = OpenRelTable::getRelId(relName);

    if(relId==E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
    if(
            !strcmp(oldRelName, RELCAT_RELNAME) || 
            !strcmp(oldRelName, ATTRCAT_RELNAME) ||
            !strcmp(newRelName, RELCAT_RELNAME) ||
            !strcmp(newRelName, ATTRCAT_RELNAME)
    ) {
        return E_NOTPERMITTED;
    }

    int ret = OpenRelTable::getRelId(oldRelName);

    if(ret != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    int retVal = BlockAccess::renameRelation(oldRelName, newRelName);
    return retVal;
}

int Schema::renameAttr(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]) {
        if(
            !strcmp(relName, RELCAT_RELNAME) || 
            !strcmp(relName, ATTRCAT_RELNAME)
    ) {
        return E_NOTPERMITTED;
    }

    int ret = OpenRelTable::getRelId(relName);

    if(ret != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    int retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
    return retVal;
}

int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrType[]) {
    Attribute relNameAsAttribute;
    strcpy(relNameAsAttribute.sVal, relName);

    RecId targetRelId;
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    //check if the relation is already present
    char relationName[ATTR_SIZE];
    strcpy(relationName, RELCAT_ATTR_RELNAME);
    RecId ret = BlockAccess::linearSearch(RELCAT_RELID, relationName, relNameAsAttribute, EQ);

    if(ret.block != -1 && ret.slot != -1) {
        return E_RELEXIST;
    }

    for(int i=0; i<nAttrs; i++) {
        for(int j=i+1; j<nAttrs; j++) {
            if(!strcmp(attrs[i], attrs[j]))
                return E_DUPLICATEATTR;
        }
    }

    // Lets insert metadata of this new relation into relation catalog
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0; // no records yet
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
    // for each record, 16*k bytes for record and 1 byte for its slotmap entry
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = floor((2016/(16*nAttrs + 1)));

    int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);

    // Let's insert metadata of attributes to attribute catalog
    for(int i=0; i<nAttrs; i++) {
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrType[i];
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

        retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
        /* if attribute catalog insert fails:
             delete the relation by calling deleteRel(targetrel) of schema layer
             return E_DISKFULL
            (this is necessary because we had already created the relation catalog entry which needs to be removed)
        */
        if(retVal != SUCCESS) {
            Schema::deleteRel(relName);
            return E_DISKFULL;
        }
    }

    return SUCCESS;

}

int Schema::deleteRel(char relName[]) {
    if(!strcmp(relName, RELCAT_RELNAME) || !strcmp(relName, ATTRCAT_RELNAME)) {
        return E_NOTPERMITTED;
    }

    int relId = OpenRelTable::getRelId(relName);

    if(relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    int ret = BlockAccess::deleteRelation(relName);

    return ret;

     /* the only that should be returned from deleteRelation() is E_RELNOTEXIST.
       The deleteRelation call may return E_OUTOFBOUND from the call to
       loadBlockAndGetBufferPtr, but if your implementation so far has been
       correct, it should not reach that point. That error could only occur
       if the BlockBuffer was initialized with an invalid block number.
    */
}