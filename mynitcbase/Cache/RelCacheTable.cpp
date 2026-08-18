#include "RelCacheTable.h"
#include <cstring>


// Define the static relation cache array
RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];


/*
 * Get the relation catalog entry for the relation with
 * rel-id relId from the cache.
 *
 * The caller must allocate memory for relCatBuf.
 */
int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf) {

    // Check whether relId is within valid range.
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    // Check whether this relation is present in the cache.
    if (relCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Copy the cached RelCatEntry into the memory
    // provided by the caller.
    *relCatBuf = relCache[relId]->relCatEntry;

    return SUCCESS;
}


/*
 * Convert a Relation Catalog record
 * (Attribute array)
 * into a RelCatEntry structure.
 *
 * The caller must allocate memory for relCatEntry.
 */
void RelCacheTable::recordToRelCatEntry(
    union Attribute record[RELCAT_NO_ATTRS],
    RelCatEntry* relCatEntry
) {

    // Copy relation name.
    strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);

    // Copy number of attributes.
    relCatEntry->numAttrs =(int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    // Copy number of records.
    relCatEntry->numRecs =(int)record[RELCAT_NO_RECORDS_INDEX].nVal;

    // Copy first block number.
    relCatEntry->firstBlk =(int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;

    // Copy last block number.
    relCatEntry->lastBlk =(int)record[RELCAT_LAST_BLOCK_INDEX].nVal;

    // Copy number of slots per block.
    relCatEntry->numSlotsPerBlk =(int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}