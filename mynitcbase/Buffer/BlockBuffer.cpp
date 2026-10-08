#include "BlockBuffer.h"
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum){
    this->blockNum=blockNum;
}

int BlockBuffer::getBlockNum() {
    return this->blockNum;
}

BlockBuffer::BlockBuffer(char blockType) {

    int retVal = getFreeBlock(blockType);

    this->blockNum = retVal;
}


RecBuffer::RecBuffer(int blockNum)
    : BlockBuffer(blockNum) {
}

RecBuffer::RecBuffer() : BlockBuffer('R') {
}

int BlockBuffer::getHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;

    //Load the block into the static buffer and get a pointer to it
    int status = loadBlockAndGetBufferPtr(&bufferPtr);

    if(status!= SUCCESS){
        return status;
    }

    //Extract the header field from the block
    memcpy(&head->blockType,  bufferPtr + 0,  4);
    memcpy(&head->pblock,    bufferPtr + 4,  4);
    memcpy(&head->lblock,    bufferPtr + 8,  4);
    memcpy(&head->rblock,    bufferPtr + 12, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs,   bufferPtr + 20, 4);
    memcpy(&head->numSlots,   bufferPtr + 24, 4);
    memcpy(head->reserved,    bufferPtr + 28, 4);

    return SUCCESS;

}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;

    //Get the number of attributes and slots from the block header
    int status = this->getHeader(&head);

    if(status!= SUCCESS){
        return status;
    }

    unsigned char *bufferPtr;

    //Load the block into the static buffer and get a pointer to it
    status = loadBlockAndGetBufferPtr(&bufferPtr);

    if(status!= SUCCESS){
        return status;
    }

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    //size of one record
    int recordSize = attrCount * ATTR_SIZE;

    /*
     * Record area begins after:
     * 1. the block header
     * 2. the slot map
     *
     * Then move forward by slotNum records.
     */
    unsigned char *slotPointer =
        bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    //Copy the complete record into the caller's array
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}



int RecBuffer::setRecord(union Attribute *rec, int slotNum) {

    unsigned char *bufferPtr;

    // Load the block into the buffer
    int retVal = loadBlockAndGetBufferPtr(&bufferPtr);

    if (retVal != SUCCESS) {
        return retVal;
    }

    // Get the block header
    struct HeadInfo head;

    retVal = getHeader(&head);

    if (retVal != SUCCESS) {
        return retVal;
    }

    // Get number of attributes and slots
    int numAttrs = head.numAttrs;
    int numSlots = head.numSlots;

    // Check whether slot number is valid
    if (slotNum < 0 || slotNum >= numSlots) {
        return E_OUTOFBOUND;
    }

    // Size of one record
    int recordSize = ATTR_SIZE * numAttrs;

    unsigned char *slotPointer =
        bufferPtr
        + HEADER_SIZE
        + numSlots
        + (slotNum * recordSize);

    // Copy record into the buffer
    memcpy(slotPointer,rec,recordSize);

    // Mark the block as dirty
    retVal = StaticBuffer::setDirtyBit(this->blockNum);

    if (retVal != SUCCESS) {
        return retVal;
    }

    return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {

    unsigned char *bufferPtr;

    // Get the starting address of the buffer containing the block
    int retVal = loadBlockAndGetBufferPtr(&bufferPtr);

    if (retVal != SUCCESS) {
        return retVal;
    }

    // Get the header of the block
    struct HeadInfo head;

    retVal = getHeader(&head);

    if (retVal != SUCCESS) {
        return retVal;
    }

    int numSlots = head.numSlots;

    // Slot map starts immediately after the header
    unsigned char *slotMapInBuffer =
        bufferPtr + HEADER_SIZE;

    // Copy the given slot map into the buffer
    memcpy(slotMapInBuffer, slotMap, numSlots);

    // Mark the block as dirty
    retVal = StaticBuffer::setDirtyBit(this->blockNum);

    if (retVal != SUCCESS) {
        return retVal;
    }

    return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head) {

    unsigned char *bufferPtr;

    // Get the address of the buffer containing this block
    int retVal = loadBlockAndGetBufferPtr(&bufferPtr);

    // If loading the block failed, return the error
    if (retVal != SUCCESS) {
        return retVal;
    }

    // Treat the beginning of the block as a HeadInfo structure
    struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

    // Copy the header fields
    bufferHeader->blockType = head->blockType;
    bufferHeader->pblock = head->pblock;
    bufferHeader->lblock = head->lblock;
    bufferHeader->rblock = head->rblock;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numSlots = head->numSlots;

    // Mark the block as dirty because we modified it
    retVal = StaticBuffer::setDirtyBit(this->blockNum);

    if (retVal != SUCCESS) {
        return retVal;
    }

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType) {

    unsigned char *bufferPtr;

    int retVal = loadBlockAndGetBufferPtr(&bufferPtr);

    if (retVal != SUCCESS) {
        return retVal;
    }

    *((int32_t *)bufferPtr) = blockType;

    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    retVal = StaticBuffer::setDirtyBit(this->blockNum);

    if (retVal != SUCCESS) {
        return retVal;
    }

    return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap) {

    unsigned char *bufferPtr;

    // Load block into static buffer
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if(ret != SUCCESS) {
        return ret;
    }

    HeadInfo head;

    // Read block header
    ret = getHeader(&head);

    if(ret != SUCCESS) {
        return ret;
    }

    int slotCount = head.numSlots;

    // Slot map begins immediately after header
    unsigned char *slotMapInBuffer =
        bufferPtr + HEADER_SIZE;

    // Copy slot map into caller's array
    memcpy(
        slotMap,
        slotMapInBuffer,
        slotCount
    );

    return SUCCESS;
}

// make comparing easy for different attribute types
int compareAttrs(union Attribute attr1,union Attribute attr2,int attrType) {

    double diff;

    if (attrType == STRING) {
        diff = strcmp(attr1.sVal,attr2.sVal);
    }
    else {
        diff = attr1.nVal - attr2.nVal;
    }
    if (diff > 0) {
        return 1;
    }
    if (diff < 0) {
        return -1;
    }

    return 0;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {

    // Check whether the block is already present in the buffer.
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
    
    // Case 1: Block is already present in the buffer
    if (bufferNum != E_BLOCKNOTINBUFFER) {

        /*
         * The block has just been accessed, so make it the
         * most recently used block.
         * Its timestamp becomes 0.
         * Increase timestamps of all other occupied buffers.
         */
        for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY; bufferIndex++) {

            if (StaticBuffer::metainfo[bufferIndex].free == false) {

                if (bufferIndex == bufferNum) {
                    StaticBuffer::metainfo[bufferIndex].timeStamp = 0;
                }
                else {
                    StaticBuffer::metainfo[bufferIndex].timeStamp++;
                }
            }
        }
    }
    
    // Case 2: Block is NOT present in the buffer
     
    else {
    //  Find a free buffer or evict the LRU buffer.
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        /*
         * getFreeBuffer() returns E_OUTOFBOUND if the
         * block number is invalid.
         */
        if (bufferNum == E_OUTOFBOUND) {
            return E_OUTOFBOUND;
        }

        /*
         * Read the requested disk block into the selected
         * buffer slot.
         */
        int retVal = Disk::readBlock(StaticBuffer::blocks[bufferNum],this->blockNum);

        if (retVal != SUCCESS) {
            return retVal;
        }
    }

    /*
     * Give the caller the address of the buffer containing
     * the requested block.
     */
    *buffPtr = StaticBuffer::blocks[bufferNum];


    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType) {

    // Find a free block in the disk
    int freeBlockNum = -1;

    for (int i = 0; i < DISK_BLOCKS; i++) {
        if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK) {
            freeBlockNum = i;
            break;
        }
    }

    // No free block available
    if (freeBlockNum == -1) {
        return E_DISKFULL;
    }

    // Store the block number in this object
    this->blockNum = freeBlockNum;

    // Get a free buffer in StaticBuffer
    int bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (bufferNum == E_OUTOFBOUND) {
        return E_OUTOFBOUND;
    }

    // Initialize the header
    struct HeadInfo head;

    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;

    int retVal = setHeader(&head);

    if (retVal != SUCCESS) {
        return retVal;
    }

    // Set the block type
    retVal = setBlockType(blockType);

    if (retVal != SUCCESS) {
        return retVal;
    }

    // Return the block number
    return freeBlockNum;
}