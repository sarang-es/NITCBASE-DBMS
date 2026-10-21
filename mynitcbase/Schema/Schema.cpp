#include "Schema.h"

#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  // the OpenRelTable::openRel() function returns the rel-id if successful
  // a valid rel-id will be within the range 0 <= relId < MAX_OPEN and any
  // error codes will be negative
  if(ret >= 0){
    return SUCCESS;
  }
  //otherwise it returns an error message
    return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // this function returns the rel-id of a relation if it is open or
  // E_RELNOTOPEN if it is not. we will implement this later.
  int relId = OpenRelTable::getRelId(relName);

  if (relId== E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  // close the relation 
  return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {

    // Cannot rename the system catalogs
    if (strcmp(oldRelName, RELCAT_RELNAME) == 0 ||
        strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 ||
        strcmp(newRelName, ATTRCAT_RELNAME) == 0) {

        return E_NOTPERMITTED;
    }

    // Check whether the relation is currently open
    if (OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Rename the relation in the catalogs
    int retVal = BlockAccess::renameRelation(oldRelName, newRelName);

    return retVal;
}

int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]) {

    // declare variable relNameAsAttribute of type Attribute
    // copy the relName into relNameAsAttribute.sVal
    Attribute relNameAsAttribute;
    strcpy(relNameAsAttribute.sVal, relName);

    // Reset the searchIndex and search RELCAT for a relation with same name
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    RecId targetRelId = BlockAccess::linearSearch(
        RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAsAttribute, EQ
    );

    // if a relation with name `relName` already exists, return E_RELEXIST
    if (targetRelId.block != -1 || targetRelId.slot != -1) {
        return E_RELEXIST;
    }

    // compare every pair of attribute names for duplicates
    for (int i = 0; i < nAttrs; i++) {
        for (int j = i + 1; j < nAttrs; j++) {
            if (strcmp(attrs[i], attrs[j]) == 0) {
                return E_DUPLICATEATTR;
            }
        }
    }

    // Build and insert the RELATIONCAT record
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,        relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal          = nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal             = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal            = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal             = -1;
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal     = floor(2016.0 / (16 * nAttrs + 1));

    int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);
    if (retVal != SUCCESS) {
        return retVal;
    }

    // Insert one ATTRIBUTECAT record per attribute
    for (int i = 0; i < nAttrs; i++) {
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,  relName);
        strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal         = attrtype[i];
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal      = -1;
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal        = -1;
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal            = i;

        retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
        if (retVal != SUCCESS) {
            // rollback: delete the partially created relation
            Schema::deleteRel(relName);
            return E_DISKFULL;
        }
    }

    return SUCCESS;
}

int Schema::deleteRel(char *relName) {

    // catalog relations cannot be deleted
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // if the relation is currently open, it cannot be deleted
    int relId = OpenRelTable::getRelId(relName);
    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // delete the relation from the catalogs and free its blocks
    return BlockAccess::deleteRelation(relName);
}

int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {

    // Cannot modify the system catalogs
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {

        return E_NOTPERMITTED;
    }

    // Check whether the relation is currently open
    if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Rename the attribute in the Attribute Catalog
    int retVal = BlockAccess::renameAttribute(relName,oldAttrName,newAttrName);

    return retVal;
}