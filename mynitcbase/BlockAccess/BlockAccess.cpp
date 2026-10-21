#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    // get the searchIndex from relational cache table - this contains the position where last search ended
    RecId prevRecID;
    RecId invalid = {-1, -1};
    int ret = RelCacheTable::getSearchIndex(relId, &prevRecID);
    if(ret != SUCCESS) {
        return invalid;
    }
    int block, slot;
    // if it is fresh search
    if(prevRecID.block == -1 && prevRecID.slot == -1) {
        RelCatEntry relCatBuf;
        ret = RelCacheTable::getRelCatEntry(relId, &relCatBuf);
        if(ret != SUCCESS) {
            return invalid;
        }
        block = relCatBuf.firstBlk;
        slot = 0;
    }
    // if there is a hit in previous search
    else {
        block = prevRecID.block;
        slot = prevRecID.slot + 1;
    }

    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS) {
        return invalid;
    }

    int offset = attrCatEntry.offset;
    int type = attrCatEntry.attrType;

    // iterate over all blocks of this relation(start from prev-search index if exists)
    while(block != -1) {

        
        // create a recbuffer object
        RecBuffer recBuffer(block);

        // get header of the block
        HeadInfo head;
        ret = recBuffer.getHeader(&head);
        if(ret != SUCCESS){
            return invalid;
        }

        // Handle case where slot was advanced past the last slot of prev block
        if (slot >= head.numSlots) {
            block = head.rblock;
            slot = 0;
            continue;
        }
        
        // get slotmap of the block
        unsigned char *slotMap = new unsigned char[head.numSlots];
        ret = recBuffer.getSlotMap(slotMap);
        if(ret != SUCCESS) {
            delete[] slotMap;
            return invalid;
        }

        while(slot < head.numSlots) {
            // check if the slot is valid
            if(slotMap[slot]==SLOT_UNOCCUPIED) {
                slot++;
                continue;
            }

            // if the slot is occupied, get that record from buffer
            Attribute record[head.numAttrs];
            ret = recBuffer.getRecord(record, slot);
            if(ret != SUCCESS) {
                delete[] slotMap;
                return invalid;
            }

            // compare using function and decide based on the argument operator
            int cmpVal = compareAttrs(record[offset], attrVal, type);
            if (
                (op == EQ && cmpVal == 0) ||
                (op == NE && cmpVal != 0) ||
                (op == LT && cmpVal < 0)  ||
                (op == LE && cmpVal <= 0) ||
                (op == GT && cmpVal > 0)  ||
                (op == GE && cmpVal >= 0)
            ) {
                RecId searchIndex = {block, slot};
                RelCacheTable::setSearchIndex(relId,&searchIndex);
                delete[] slotMap;
                return RecId{block, slot};
            }
            slot++;
        }
        delete[] slotMap;
        block = head.rblock;
        slot = 0;
    }

    // if nothing is found
    RelCacheTable::setSearchIndex(relId, &invalid);
    return invalid;

}

int BlockAccess::renameRelation(char oldName[ATTRCAT_BLOCK], char newName[ATTR_SIZE]) {

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelName;
    strcpy(newRelName.sVal, newName);
    char relName[ATTR_SIZE];
    strcpy(relName, RELCAT_ATTR_RELNAME);
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, relName, newRelName, EQ);

    // what if the newName already exists
    if(recId.block != -1 && recId.slot != -1) {
        return E_RELEXIST;
    }

    /**********Update new name in relation catalog entries**********/

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute oldRelName;
    strcpy(oldRelName.sVal, oldName);
    recId = BlockAccess::linearSearch(RELCAT_RELID, relName, oldRelName, EQ);

    // what is the oldName never existed
    if(recId.block == -1 && recId.slot == -1) {
        return E_RELNOTEXIST;
    }

    // get the record of this relation in relation catalog
    RecBuffer buffer(recId.block);
    Attribute rec[RELCAT_NO_ATTRS];
    buffer.getRecord(rec, recId.slot);

    // rename the relation
    strcpy(rec[RELCAT_REL_NAME_INDEX].sVal, newName);
    buffer.setRecord(rec, recId.slot);

    /************Update new name in attribute catalog entries of this relation***********/
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    strcpy(relName, ATTRCAT_ATTR_RELNAME);

    while(true) {
        recId = BlockAccess::linearSearch(ATTRCAT_RELID, relName, oldRelName, EQ);
        if(recId.block == -1 && recId.slot == -1) {
            break;
        }

        // change the name of relation in each record of this 
        RecBuffer attrCatBuffer(recId.block);
        Attribute attrRec[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrRec, recId.slot);
        
        strcpy(attrRec[ATTRCAT_REL_NAME_INDEX].sVal, newName);

        attrCatBuffer.setRecord(attrRec, recId.slot);
    }

    return SUCCESS;
    
}   

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

    // reset search index before a fresh search
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute rel;
    strcpy(rel.sVal, relName);
    char relCatRelName[ATTR_SIZE];
    strcpy(relCatRelName, RELCAT_ATTR_RELNAME);

    // search to see if such a relation exists
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, relCatRelName, rel, EQ);

    if(recId.block == -1 && recId.slot == -1) {
        return E_RELNOTEXIST;
    }

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    /* declare variable attrToRenameRecId used to store the attr-cat recId
    of the attribute to rename */
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
    char attrCatRelName[ATTR_SIZE];
    strcpy(attrCatRelName, ATTRCAT_ATTR_RELNAME);
    Attribute attribute;
    strcpy(attribute.sVal, relName);

    /* iterate over all Attribute Catalog Entry record corresponding to the relation to find the required attribute */
    while(true) {

        RecId attrRecId = BlockAccess::linearSearch(ATTRCAT_RELID, attrCatRelName, attribute, EQ);
        if(attrRecId.block == -1 && attrRecId.slot == -1) {
            break;
        }

        RecBuffer buffer(attrRecId.block);
        buffer.getRecord(attrCatEntryRecord, attrRecId.slot);

        // if an attribute with new name already exists
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName)==0) {
            return E_ATTREXIST;
        }

        // collect the block number and slot number of the target attribute
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0){
            attrToRenameRecId = attrRecId;
        }
    }

    if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1) {
        return E_ATTRNOTEXIST;
    }

    // load the block into buffer and get the record
    RecBuffer attrCatBuffer(attrToRenameRecId.block);
    attrCatBuffer.getRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    
    // Rename the attribute
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);

    // write it back
    attrCatBuffer.setRecord(attrCatEntryRecord, attrToRenameRecId.slot);

    return SUCCESS;

}


int BlockAccess::insert(int relId, Attribute *record) {
    RelCatEntry relCatBuf;
    RelCacheTable::getRelCatEntry(relId, &relCatBuf);

    int blockNum = relCatBuf.firstBlk;

    // Useful to store where the new record will be inserted
    RecId rec_id = {INVALID_BLOCKNUM, INVALID_BLOCKNUM};

    int numOfSlots = relCatBuf.numSlotsPerBlk;
    int numOfAttributes = relCatBuf.numAttrs;

    int prevBlockNum = -1; // block number of the prev seen block in the linked list

    // Traversing the linked list of existing record blocks of the relation
    // until a free slot is found OR until the end of the list is reached
    while (blockNum != -1) {
        RecBuffer recBuff(blockNum);

        struct HeadInfo head; // Can't be a pointer because unless heap allocation is used
        recBuff.getHeader(&head);

        unsigned char slotMap[numOfSlots];
        recBuff.getSlotMap(slotMap);

        // Every block has the same number of slots, so we can just iterate over it
        // and find a free slot which we will store in rec_id
        for (int i = 0; i < numOfSlots; i++) {
            if (slotMap[i] == SLOT_UNOCCUPIED) {
                rec_id = {blockNum, i}; // store the blockNum and slot
                break;
            }
        }

        // if slot is found, then break the traversal
        if (rec_id.block != INVALID_BLOCKNUM && rec_id.slot != INVALID_BLOCKNUM) {
            break;
        }

        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }

    // if no free slot is found in existing record blocks
    if (rec_id.block == INVALID_BLOCKNUM && rec_id.slot == INVALID_BLOCKNUM) {
        // if relation is RELCAT, do not allocate any more blocks
        if (!strcmp(relCatBuf.relName, RELCAT_RELNAME)) {
            return E_MAXRELATIONS;
        } else {
            RecBuffer newRecBuff; // No need of argument here, this will create a new block
            int newBlockNum = newRecBuff.getBlockNum();

            if (newBlockNum == E_DISKFULL)
                return E_DISKFULL;

            rec_id.block = newBlockNum;
            rec_id.slot = 0;

            // set the header of the new record block
            struct HeadInfo newHead;
            newHead.blockType = REC;
            newHead.pblock = -1;
            newHead.lblock = prevBlockNum;
            newHead.rblock = -1;
            newHead.numEntries = 0;
            newHead.numSlots = numOfSlots;
            newHead.numAttrs = numOfAttributes;

            newRecBuff.setHeader(&newHead);

            // set block's slot map with all slots marked as free
            unsigned char newSlotMap[numOfSlots];
            for (int i = 0; i < numOfSlots; i++) {
                newSlotMap[i] = SLOT_UNOCCUPIED;
            }

            newRecBuff.setSlotMap(newSlotMap);

            // prev != -1 only when >=1 block already allocated (prev tracks tail seen in traversal)
            if (prevBlockNum != -1) {
                RecBuffer prevBlockBuffer(prevBlockNum);

                // Update references
                struct HeadInfo prevBlockHeader;
                prevBlockBuffer.getHeader(&prevBlockHeader);
                prevBlockHeader.rblock = newBlockNum;
                prevBlockBuffer.setHeader(&prevBlockHeader);
            } else {
                // first insertion: update firstBlk in rel cat
                RelCacheTable::getRelCatEntry(relId, &relCatBuf);
                relCatBuf.firstBlk = newBlockNum;
                RelCacheTable::setRelCatEntry(relId, &relCatBuf);
            }

            // new block is always appended at end, so it is the new lastBlk in both cases
            // so, we have to update relCat
            RelCacheTable::getRelCatEntry(relId, &relCatBuf);
            relCatBuf.lastBlk = newBlockNum;
            RelCacheTable::setRelCatEntry(relId, &relCatBuf);
        }
    }

    // insert the record into rec_id'th slot
    RecBuffer recBuffer(rec_id.block);
    recBuffer.setRecord(record, rec_id.slot);

    // mark slot as occupied
    unsigned char slotMap[numOfSlots];
    recBuffer.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    recBuffer.setSlotMap(slotMap);

    // increment numEntries in block header
    struct HeadInfo head;
    recBuffer.getHeader(&head);
    head.numEntries++;
    recBuffer.setHeader(&head);

    // increment numRecs in rel cat
    RelCacheTable::getRelCatEntry(relId, &relCatBuf);
    relCatBuf.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatBuf);

    return SUCCESS;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    // this function is like a cover for linearSearch to return a record instead of recId
    RecId recId;
    BlockAccess::linearSearch(relId, attrName, attrVal, op);

    if(recId.block==-1 && recId.block==-1) {
        return E_NOTFOUND;
    }

    //instantiate a Recbuffer object and use it to get record into record variable
    RecBuffer recBuffer(recId.block);

    recBuffer.getRecord(record, recId.slot);

    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {

    // relation catalog and attibute catalog cannot be deleted
    if(!strcmp(relName, RELCAT_RELNAME) || !strcmp(relName, ATTRCAT_RELNAME)) {
        return E_NOTPERMITTED;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    char relCatRelName[ATTR_SIZE];
    strcpy(relNameAttr.sVal, relName);
    strcpy(relCatRelName, RELCAT_ATTR_RELNAME);

    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, relCatRelName, relNameAttr, EQ);

    if(recId.block==-1 && recId.slot==-1) {
        return E_RELNOTEXIST;
    }

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    RecBuffer relCatBuffer(recId.block);
    relCatBuffer.getRecord(relCatEntryRecord, recId.slot);

    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    int current = firstBlock;
    // release all the blocks in linked list
    while(current != -1) {

        RecBuffer currentBuffer(current);
        HeadInfo currentHeader;
        currentBuffer.getHeader(&currentHeader);

        current = currentHeader.rblock;

        currentBuffer.releaseBlock();
    }

    /***
        Deleting attribute catalog entries corresponding the relation and index
        blocks corresponding to the relation with relName on its attributes
    ***/

    //reset searchIndex of attribute catalog table
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    
    int numberOfAttributesDeleted = 0;
    char attrCatRelName[ATTR_SIZE];
    strcpy(attrCatRelName, ATTRCAT_ATTR_RELNAME);

    while (true) {
        // Find all occurrence of attributes of this relation in attribute catalog
        // load that block, header and slotmap
        // mark the slot as unoccupied and set that slotmap

        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, attrCatRelName, relNameAttr, EQ);

        if(attrCatRecId.block==-1 && attrCatRecId.slot==-1) {
            // no more attributes to delete
            break;
        }

        numberOfAttributesDeleted++;

        RecBuffer recBuffer(attrCatRecId.block);
        Attribute record[ATTRCAT_NO_ATTRS];
        recBuffer.getRecord(record, attrCatRecId.slot);
        
        int rootBlock = record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;// this will be used later

        HeadInfo attrCatHeader;
        recBuffer.getHeader(&attrCatHeader);

        //task1: decrement number of entries in header and set it back
        attrCatHeader.numEntries -= 1;
        recBuffer.setHeader(&attrCatHeader);
        
        //task2: mark the slot as unoccupied in the slot map
        unsigned char slotMap[attrCatHeader.numSlots];
        recBuffer.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        recBuffer.setSlotMap(slotMap);
        

        //If number of entries in that block becomes 0, releaseBlock is called after fixing the linked list.

        if(attrCatHeader.numEntries==0) {
            // lblock and rblock should be connected after deleting this entry
            RecBuffer leftBlock(attrCatHeader.lblock);
            HeadInfo leftHeader;
            leftBlock.getHeader(&leftHeader);
            leftHeader.rblock = attrCatHeader.rblock;
            leftBlock.setHeader(&leftHeader);

            // what if this block is last in linked list?
            if(attrCatHeader.rblock != -1) {
                //if not, connect the left block to it
                RecBuffer rightBlock(attrCatHeader.rblock);
                HeadInfo rightHeader;
                rightBlock.getHeader(&rightHeader);
                rightHeader.lblock = attrCatHeader.lblock;
                rightBlock.setHeader(&rightHeader);
            }
            else {
                // if this is the last block, update last block of attribute catalog in relation catalog
                RelCatEntry relCatEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
                relCatEntry.lastBlk = attrCatHeader.lblock;
            }

            // Qn: the linked list of attribute catalog will never become empty why?
            // ans: anyways it has to store attributes of relation catalog and attribute catalog

            // It's time to release that block
            recBuffer.releaseBlock();
        }

        /*
        // (the following part is only relevant once indexing has been implemented)
        // if index exists for the attribute (rootBlock != -1), call bplus destroy
        if (rootBlock != -1) {
            // delete the bplus tree rooted at rootBlock using BPlusTree::bPlusDestroy()
        }
		*/
    }

    // Now delete the entries of this relation from relation catalog

    // task1: update header of relation catalog
    HeadInfo relCatHeader;
    relCatBuffer.getHeader(&relCatHeader);
    relCatHeader.numEntries -= 1;
    relCatBuffer.setHeader(&relCatHeader);

    //task2: mark the slot of this relation in relation catalog slotmap as unoccupied
    unsigned char slotMap[relCatHeader.numSlots];
    relCatBuffer.getSlotMap(slotMap);
    slotMap[recId.slot] = SLOT_UNOCCUPIED;
    relCatBuffer.setSlotMap(slotMap);

    //task3: update relation cache entry of relation catalog (number of relation decreased by 1)
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    relCatEntry.numRecs -= 1;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

    //task: update relation cache entry of attribute catalog (number of attributes decreased by numberOfAttributesDeleted)
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
    relCatEntry.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);

    return SUCCESS;
}
