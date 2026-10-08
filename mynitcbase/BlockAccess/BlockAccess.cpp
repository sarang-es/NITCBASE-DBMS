#include "BlockAccess.h"
#include <cstring>

RecId BlockAccess::linearSearch(
    int relId,
    char attrName[ATTR_SIZE],
    union Attribute attrVal,
    int op
) {

    RecId prevRecId;

    int status =RelCacheTable::getSearchIndex(relId,&prevRecId);

    if (status != SUCCESS) {
        return RecId{-1, -1};
    }

    int block, slot;

    if (prevRecId.block == -1 && prevRecId.slot == -1) {

        RelCatEntry relCatEntry;

        status =RelCacheTable::getRelCatEntry(relId,&relCatEntry);

        if (status != SUCCESS) {
            return RecId{-1, -1};
        }

        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else {

        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    AttrCatEntry attrCatEntry;

    status =AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatEntry);

    if (status != SUCCESS) {
        return RecId{-1, -1};
    }

    while (block != -1) {

        RecBuffer recBlock(block);

        HeadInfo head;

        recBlock.getHeader(&head);

        unsigned char slotMap[head.numSlots];

        recBlock.getSlotMap(slotMap);

        if (slot >= head.numSlots) {

            block = head.rblock;
            slot = 0;

            continue;
        }

        if (slotMap[slot] == SLOT_UNOCCUPIED) {

            slot++;

            continue;
        }

        Attribute record[head.numAttrs];

        recBlock.getRecord(
            record,
            slot
        );

        Attribute recordAttr =
            record[attrCatEntry.offset];

        int cmpVal =compareAttrs(recordAttr,attrVal,attrCatEntry.attrType);

        bool cond = false;

        if (
            (op == NE && cmpVal != 0) ||
            (op == LT && cmpVal < 0) ||
            (op == LE && cmpVal <= 0) ||
            (op == EQ && cmpVal == 0) ||
            (op == GT && cmpVal > 0) ||
            (op == GE && cmpVal >= 0)
        ) {

            RecId foundRecId;

            foundRecId.block = block;
            foundRecId.slot = slot;

            RelCacheTable::setSearchIndex(relId,&foundRecId);

            return foundRecId;
        }

        slot++;
    }

    RelCacheTable::resetSearchIndex(relId);

    return RecId{-1, -1};
}

// Rename Relation
int BlockAccess::renameRelation(char oldName[ATTR_SIZE],char newName[ATTR_SIZE]) {

    //  Step 1: Check whether new relation name already exists

    // Reset search index of Relation Catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // Store new relation name in Attribute
    Attribute newRelationName;
    strcpy(newRelationName.sVal, newName);

    // Search Relation Catalog for new relation name
    RecId recId = linearSearch(RELCAT_RELID,RELCAT_ATTR_RELNAME,newRelationName,EQ);

    // If relation already exists
    if (recId.block != -1 || recId.slot != -1) {
        return E_RELEXIST;
    }

    //  Step 2: Find the old relation
    
    // Reset search index of Relation Catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // Store old relation name in Attribute
    Attribute oldRelationName;
    strcpy(oldRelationName.sVal, oldName);

    // Search Relation Catalog for old relation name
    recId = linearSearch(RELCAT_RELID,RELCAT_ATTR_RELNAME,oldRelationName,EQ);

    // If old relation does not exist
    if (recId.block == -1 && recId.slot == -1) {
        return E_RELNOTEXIST;
    }

    // Step 3: Read the Relation Catalog record

    RecBuffer relCatBuffer(recId.block);

    Attribute relCatRecord[RELCAT_NO_ATTRS];

    int retVal = relCatBuffer.getRecord(relCatRecord,recId.slot);

    if (retVal != SUCCESS) {
        return retVal;
    }

    // Step 4: Get number of attributes of the relation
    int numAttributes = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    //  Change relation name in Relation Catalog
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,newName);

    // Write modified record back
    retVal = relCatBuffer.setRecord(relCatRecord,recId.slot);

    if (retVal != SUCCESS) {
        return retVal;
    }


    // Step 6: Update Attribute Catalog

    // Reset search index of Attribute Catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    /*
     * Find every Attribute Catalog entry whose RelName
     * is oldName and change it to newName.
     */
    for (int i = 0; i < numAttributes; i++) {

        // Search Attribute Catalog for old relation name
        RecId attrRecId = linearSearch(ATTRCAT_RELID,ATTRCAT_ATTR_RELNAME,oldRelationName,EQ);

        // No more matching records
        if (attrRecId.block == -1 && attrRecId.slot == -1) {
            break;
        }

        // Read the Attribute Catalog record

        RecBuffer attrCatBuffer(attrRecId.block);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        retVal = attrCatBuffer.getRecord(attrCatRecord,attrRecId.slot);

        if (retVal != SUCCESS) {
            return retVal;
        }


        // Change RelName from oldName to newName
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,newName);

        //  Write modified record back
        retVal = attrCatBuffer.setRecord(attrCatRecord,attrRecId.slot);

        if (retVal != SUCCESS) {
            return retVal;
        }
    }

    return SUCCESS;
}

//Rename attribute
int BlockAccess::renameAttribute(char relName[ATTR_SIZE],char oldName[ATTR_SIZE],char newName[ATTR_SIZE]) {

    /*
     * ---------------------------------------------------------
     * Step 1: Check whether the relation exists
     * ---------------------------------------------------------
     */

    // Reset search index of Relation Catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // Store relation name in Attribute
    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    // Search for the relation in Relation Catalog
    RecId relRecId = linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME,relNameAttr,EQ);

    // If relation does not exist
    if (relRecId.block == -1 && relRecId.slot == -1) {
        return E_RELNOTEXIST;
    }


    /*
     * ---------------------------------------------------------
     * Step 2: Search Attribute Catalog
     * ---------------------------------------------------------
     */

    // Reset search index of Attribute Catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    // RecId of the attribute which has to be renamed
    RecId attrToRenameRecId{-1, -1};

    // Buffer to store an Attribute Catalog record
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];


    /*
     * ---------------------------------------------------------
     * Step 3: Search through all attributes of the relation
     * ---------------------------------------------------------
     */

    while (true) {

        // Find next Attribute Catalog entry
        // where RelName = relName
        RecId attrRecId = linearSearch(ATTRCAT_RELID,ATTRCAT_ATTR_RELNAME,relNameAttr,EQ);

        // No more attributes belonging to this relation
        if (attrRecId.block == -1 && attrRecId.slot == -1) {
            break;
        }


        /*
         * Get the Attribute Catalog record
         */
        RecBuffer attrCatBuffer(attrRecId.block);

        int retVal = attrCatBuffer.getRecord(attrCatEntryRecord,attrRecId.slot);

        if (retVal != SUCCESS) {
            return retVal;
        }


        /*
         * Check whether this is the attribute
         * that we want to rename.
         */
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,oldName ) == 0) {
            attrToRenameRecId.block = attrRecId.block;
            attrToRenameRecId.slot = attrRecId.slot;
        }


        /*
         * Check whether new attribute name already exists.
         */
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newName) == 0) {

            return E_ATTREXIST;
        }
    }


    /*
     * ---------------------------------------------------------
     * Step 4: Check whether old attribute was found
     * ---------------------------------------------------------
     */

    if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1) {
        return E_ATTRNOTEXIST;
    }


    /*
     * ---------------------------------------------------------
     * Step 5: Update Attribute Catalog record
     * ---------------------------------------------------------
     */

    // Create buffer for the attribute record
    RecBuffer attrCatBuffer(attrToRenameRecId.block);

    // Get the record
    int retVal = attrCatBuffer.getRecord(attrCatEntryRecord,attrToRenameRecId.slot);

    if (retVal != SUCCESS) {
        return retVal;
    }

    /*
     * Change old attribute name to new attribute name
     */
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newName);

    /*
     * Write the modified record back
     */
    retVal = attrCatBuffer.setRecord(attrCatEntryRecord,attrToRenameRecId.slot);

    if (retVal != SUCCESS) {
        return retVal;
    }

    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record)
{
    // Get the relation catalog entry
    RelCatEntry relCatEntry;

    int retVal =
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    // First record block of the relation
    int blockNum = relCatEntry.firstBlk;

    // rec_id stores the location where the record will be inserted
    RecId rec_id = {-1, -1};

    // Information needed to create a new block
    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;

    // Last block visited
    int prevBlockNum = -1;

    /*
     * Search existing record blocks for a free slot.
     */
    while (blockNum != -1)
    {
        // Open the existing record block
        RecBuffer recBuffer(blockNum);

        // Get the block header
        HeadInfo head;

        retVal = recBuffer.getHeader(&head);

        if (retVal != SUCCESS)
        {
            return retVal;
        }

        // Get the slot map
        unsigned char slotMap[numOfSlots];

        retVal = recBuffer.getSlotMap(slotMap);

        if (retVal != SUCCESS)
        {
            return retVal;
        }

        // Search for a free slot
        for (int i = 0; i < numOfSlots; i++)
        {
            if (slotMap[i] == SLOT_UNOCCUPIED)
            {
                rec_id.block = blockNum;
                rec_id.slot = i;
                break;
            }
        }

        // If a free slot was found, stop traversal
        if (rec_id.block != -1)
        {
            break;
        }

        // No free slot in this block.
        // Move to the next block.
        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }

    /*
     * If no free slot was found, create a new record block.
     */
    if (rec_id.block == -1)
    {
        // RELCAT cannot have more record blocks
        if (relId == RELCAT_RELID)
        {
            return E_MAXRELATIONS;
        }

        // Create a new record block
        RecBuffer newRecBuffer;

        // Get the block number of the newly allocated block
        int newBlockNum = newRecBuffer.getBlockNum();

        if (newBlockNum == E_DISKFULL)
        {
            return E_DISKFULL;
        }

        if (newBlockNum < 0)
        {
            return newBlockNum;
        }

        // The record will be inserted into slot 0
        rec_id.block = newBlockNum;
        rec_id.slot = 0;

        // Prepare header of new block
        HeadInfo newHead;

        newHead.blockType = REC;
        newHead.pblock = -1;

        if (prevBlockNum == -1)
        {
            newHead.lblock = -1;
        }
        else
        {
            newHead.lblock = prevBlockNum;
        }

        newHead.rblock = -1;
        newHead.numEntries = 0;
        newHead.numAttrs = numOfAttributes;
        newHead.numSlots = numOfSlots;

        // Write the header
        retVal = newRecBuffer.setHeader(&newHead);

        if (retVal != SUCCESS)
        {
            return retVal;
        }

        // Initialize slot map
        unsigned char slotMap[numOfSlots];

        for (int i = 0; i < numOfSlots; i++)
        {
            slotMap[i] = SLOT_UNOCCUPIED;
        }

        retVal = newRecBuffer.setSlotMap(slotMap);

        if (retVal != SUCCESS)
        {
            return retVal;
        }

        // Link previous block to new block
        if (prevBlockNum != -1)
        {
            RecBuffer prevBuffer(prevBlockNum);

            HeadInfo prevHead;

            retVal = prevBuffer.getHeader(&prevHead);

            if (retVal != SUCCESS)
            {
                return retVal;
            }

            prevHead.rblock = newBlockNum;

            retVal = prevBuffer.setHeader(&prevHead);

            if (retVal != SUCCESS)
            {
                return retVal;
            }
        }
        else
        {
            // This is the first block of the relation
            relCatEntry.firstBlk = newBlockNum;
        }

        // New block becomes the last block
        relCatEntry.lastBlk = newBlockNum;

        // Update relation cache
        retVal =
            RelCacheTable::setRelCatEntry(
                relId,
                &relCatEntry
            );

        if (retVal != SUCCESS)
        {
            return retVal;
        }
    }

    /*
     * Insert the record into the selected block and slot.
     */
    RecBuffer recBuffer(rec_id.block);

    retVal = recBuffer.setRecord(record, rec_id.slot);

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    /*
     * Update the slot map.
     */
    unsigned char slotMap[numOfSlots];

    retVal = recBuffer.getSlotMap(slotMap);

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    slotMap[rec_id.slot] = SLOT_OCCUPIED;

    retVal = recBuffer.setSlotMap(slotMap);

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    /*
     * Increment number of entries in the block.
     */
    HeadInfo head;

    retVal = recBuffer.getHeader(&head);

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    head.numEntries++;

    retVal = recBuffer.setHeader(&head);

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    /*
     * Increment number of records in the relation.
     */
    relCatEntry.numRecs++;

    retVal =
        RelCacheTable::setRelCatEntry(
            relId,
            &relCatEntry
        );

    if (retVal != SUCCESS)
    {
        return retVal;
    }

    return SUCCESS;
}