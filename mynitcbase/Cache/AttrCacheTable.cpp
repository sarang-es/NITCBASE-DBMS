#include "AttrCacheTable.h"

#include <cstring>

AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrcatBuf) {
    if(relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }
    
    if(attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }
    for(AttrCacheEntry *entry = attrCache[relId]; entry!=nullptr; entry = entry->next) {
        if(entry->attrCatEntry.offset == attrOffset) {
            *attrcatBuf = entry->attrCatEntry;
            return SUCCESS; 
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf) {
    if(relId < 0 || relId > MAX_OPEN) {
        return E_OUTOFBOUND;
    }
    
    if(attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
        if(strcmp(entry->attrCatEntry.attrName, attrName)==0) {
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}



void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry* attrCatEntry) {
    strcpy(attrCatEntry->relName , record[ATTRCAT_REL_NAME_INDEX].sVal);
    strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
    attrCatEntry->attrType = (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    attrCatEntry->offset = (int)record[ATTRCAT_OFFSET_INDEX].nVal;
    attrCatEntry->primaryFlag = (bool)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
}

void AttrCacheTable::attrCatEntryToRecord(AttrCatEntry *attrCatEntry, union Attribute record[ATTRCAT_NO_ATTRS]) {
    strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, attrCatEntry->relName);
    strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, attrCatEntry->attrName);
    record[ATTRCAT_ATTR_TYPE_INDEX].nVal = (double)attrCatEntry->attrType;
    record[ATTRCAT_OFFSET_INDEX].nVal = (double)attrCatEntry->offset;
    record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = (double)attrCatEntry->primaryFlag;
    record[ATTRCAT_ROOT_BLOCK_INDEX].nVal = (double)attrCatEntry->rootBlock;
}


int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE]) {
    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
            entry->searchIndex.block = -1;
            entry->searchIndex.index = -1;
            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset) {
    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {
        if (entry->attrCatEntry.offset == attrOffset) {
            entry->searchIndex.block = -1;
            entry->searchIndex.index = -1;
            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}


int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE],
                                   IndexId *searchIndex) {

    // Check whether relId is within the valid range
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    // Check whether the relation is open
    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Traverse the attribute cache entries for this relation
    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {

        // Find the attribute by its name
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {

            // Copy the search index into the output parameter
            *searchIndex = entry->searchIndex;

            return SUCCESS;
        }

        entry = entry->next;
    }

    // The attribute was not found
    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset,
                                   IndexId *searchIndex) {

    // Check whether relId is within the valid range
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    // Check whether the relation is open
    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Traverse the attribute cache entries for this relation
    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {

        // Find the attribute by its offset
        if (entry->attrCatEntry.offset == attrOffset) {

            // Copy the search index into the output parameter
            *searchIndex = entry->searchIndex;

            return SUCCESS;
        }

        entry = entry->next;
    }

    // The attribute was not found
    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE],
                                   IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
            entry->searchIndex = *searchIndex;
            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset,
                                   IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {
        if (entry->attrCatEntry.offset == attrOffset) {
            entry->searchIndex = *searchIndex;
            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}


int AttrCacheTable::setAttrCatEntry(int relId, char attrName[ATTR_SIZE],
                                    AttrCatEntry *attrCatBuf) {

    // Check whether relId is within the valid range
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    // Check whether the relation is open
    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Traverse the attribute cache entries
    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {

        // Find the attribute by its name
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {

            // Copy the new attribute catalog entry
            entry->attrCatEntry = *attrCatBuf;

            // Mark the entry as dirty
            entry->dirty = 1;

            return SUCCESS;
        }

        entry = entry->next;
    }

    // Attribute not found
    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setAttrCatEntry(int relId, int attrOffset,
                                    AttrCatEntry *attrCatBuf) {

    // Check whether relId is within the valid range
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    // Check whether the relation is open
    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Traverse the attribute cache entries
    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {

        // Find the attribute by its offset
        if (entry->attrCatEntry.offset == attrOffset) {

            // Copy the new attribute catalog entry
            entry->attrCatEntry = *attrCatBuf;

            // Mark the entry as dirty
            entry->dirty = 1;

            return SUCCESS;
        }

        entry = entry->next;
    }

    // Attribute not found
    return E_ATTRNOTEXIST;
}
