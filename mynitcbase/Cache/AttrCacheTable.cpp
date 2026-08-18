#include "AttrCacheTable.h"
#include <cstring>


// Define the static attribute cache array.
AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];


/*
 * Returns the attrOffset-th attribute for the relation
 * corresponding to relId.
 *
 * The caller must allocate memory for attrCatBuf.
 */
int AttrCacheTable::getAttrCatEntry(
    int relId,
    int attrOffset,
    AttrCatEntry* attrCatBuf
) {

    // Check whether relId is valid.
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    // Check whether the relation is present in the cache.
    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Traverse the linked list of attributes.
    for (
        AttrCacheEntry* entry = attrCache[relId];
        entry != nullptr;
        entry = entry->next
    ) {

        // Check whether this is the attribute
        // with the requested offset.
        if (entry->attrCatEntry.offset == attrOffset) {

            // Copy the attribute catalog entry
            // into the memory provided by the caller.
            *attrCatBuf = entry->attrCatEntry;

            return SUCCESS;
        }
    }

    // No attribute with the requested offset was found.
    return E_ATTRNOTEXIST;
}


/*
 * Converts an Attribute Catalog record
 * (union Attribute array)
 * into an AttrCatEntry structure.
 *
 * The caller must allocate memory for attrCatEntry.
 */
void AttrCacheTable::recordToAttrCatEntry(
    union Attribute record[ATTRCAT_NO_ATTRS],
    AttrCatEntry* attrCatEntry
) {

    // Copy relation name.
    strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);

    // Copy attribute name.
    strcpy(attrCatEntry->attrName,record[ATTRCAT_ATTR_NAME_INDEX].sVal);

    // Copy attribute type.
    attrCatEntry->attrType =(int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;

    // Copy primary key flag.
    attrCatEntry->primaryFlag =(bool)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;

    // Copy root block.
    attrCatEntry->rootBlock =(int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

    // Copy attribute offset.
    attrCatEntry->offset =(int)record[ATTRCAT_OFFSET_INDEX].nVal;
}