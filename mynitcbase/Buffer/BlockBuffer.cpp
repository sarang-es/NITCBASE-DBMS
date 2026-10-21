#include "BlockBuffer.h"
#include "StaticBuffer.h"

#include <cstdlib>
#include <cstring>

// constructor 1
BlockBuffer::BlockBuffer(char blockType) {
    // Allocate a new block of given type. On success blockNum = new block,
    // on failure blockNum = error code. Caller must check getBlockNum().
    this->blockNum = BlockBuffer::getFreeBlock(blockType);
}

// constructor 2
BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

// constructor 1
// Allocate a new record block. Caller must check getBlockNum() for errors.
RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer((char)REC) {}

// constructor 2
// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

int BlockBuffer::getBlockNum() {
    return this->blockNum;
}

// Load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {
    // Instead of reading directly from disk, use a function to do it
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS) {
        return ret; // if it is error, return as it is
    }

    // populate the entries in HeadInfo
    memcpy(&head->lblock, bufferPtr + 8, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->numSlots, bufferPtr + 24, 4);

    return SUCCESS;

}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;

    // use getHeader to assign head
    // Properly check error return status
    int ret = BlockBuffer::getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }
    
    // Bounds check slotNum against valid slot range
    if (slotNum < 0 || slotNum >= head.numSlots) {
        return E_OUTOFBOUND;
    }
    
    // attrCount for finding recordSize & slotCount is the length of slotmap
    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    // Instead of reading directly from disk, use a function instead
    unsigned char *bufferPtr; 
    ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS) {
        return ret; // If it is error, return as it is
    }

    // make a pointer pointing to start of our record
    int recordSize = attrCount*ATTR_SIZE;
    int offset = HEADER_SIZE + slotCount + slotNum*recordSize;
    unsigned char *slotPointer = bufferPtr + offset;    

    // copy from slotPointer to the length record size to rec
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}   

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferPtr) {
    // check if the the block is already present in the buffer
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
    if(bufferNum == E_BLOCKNOTINBUFFER) {
        // get free buffer and read the block into it - the getFreeBuffer function will increment timestamp
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if(bufferNum == E_OUTOFBOUND) {
            return E_OUTOFBOUND;
        }
        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    } 
    else {
        // here i have to manually increment all the timestamps
        for(int bufferIndex=0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
            if(StaticBuffer::metainfo[bufferIndex].free == false) {
                StaticBuffer::metainfo[bufferIndex].timeStamp++;
            }
        }
        // even if it was loaded earlier, timeStamp is set to 0 because it is the Least Recently Used
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
    }

    // bufferPtr stores the pointer to block in buffer
    *bufferPtr = StaticBuffer::blocks[bufferNum];
    
    return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap) {
    // load into buffer if not exist
    unsigned char *bufferPtr;
    int status = loadBlockAndGetBufferPtr(&bufferPtr);
    if(status != SUCCESS) {
        return status;
    }

    // get headerInfo
    struct HeadInfo head;
    status = getHeader(&head);
    if(status != SUCCESS) {
        return status;
    }

    // go to offset of slotMap
    int slotCount = head.numSlots;
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;
    
    // copy slotmap to the argument variable
    memcpy(slotMap, slotMapInBuffer, slotCount);

    // return success
    return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    struct HeadInfo head;
    ret = getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    int slotCount = head.numSlots;
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

    memcpy(slotMapInBuffer, slotMap, slotCount);

    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}

// compare two attributes and return -1, 0 or 1
int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {
    double diff;
    if(attrType == STRING) {
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else {
        diff = attr1.nVal - attr2.nVal;
    }

    if(diff < 0) 
        { return -1;}
    else if(diff > 0) 
        { return 1;}
    else 
        { return 0;}
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;
    // get the starting address of the buffer containing the block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    
    if(ret != SUCCESS) {
        return ret;
    }

    HeadInfo head;
    this->getHeader(&head);

    int noOfAttrs = head.numAttrs;
    int noOfSlots = head.numSlots;

    if(slotNum < 0 || slotNum >= noOfSlots) {
        return E_OUTOFBOUND;
    }

    // calculate the pointer pointing to the record within the block
    int recordSize = noOfAttrs * ATTR_SIZE;
    int offset = HEADER_SIZE + noOfSlots + (slotNum*recordSize);
    unsigned char *recPtr = bufferPtr + offset;

    // copy the argument record to the record in buffer
    memcpy(recPtr, rec, recordSize);

    StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;

    int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    // cast bufferPtr to type HeadInfo*
    struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr; // it is possible to convert directly like ((struct HeadInfo *)bufferPtr)->*

    // copy the fields of the HeadInfo pointed to by head (except reserved) to the header of the block (pointed to by bufferHeader)
    bufferHeader->numSlots = head->numSlots;
    bufferHeader->pblock = head->pblock;
    bufferHeader->lblock = head->lblock;
    bufferHeader->blockType = head->blockType;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->rblock = head->rblock;

    // update dirty bit in StaticBuffer
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType) {
    unsigned char *bufferPtr;

    int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    // store the argument block type in the header of buffer
    ((HeadInfo *)bufferPtr)->blockType = blockType;

    // update the StaticBuffer::blockAllocMap entry
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    // update dirty bit in StaticBuffer
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType) {
    // iterate through the StaticBuffer::blockAllocMap and find the block number of an unused block in the disk
    int blockNum;
    for (blockNum = 0; blockNum < DISK_BLOCKS; blockNum++) {
        if (StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK) {
            break;
        }
    }

    if (blockNum == DISK_BLOCKS)
        return E_DISKFULL;

    // set the object's blockNum to the block number of the free block
    this->blockNum = blockNum;

    // find a free buffer (LRU ensures a free buffer)
    int bufferNum = StaticBuffer::getFreeBuffer(blockNum); 
    if (bufferNum == E_OUTOFBOUND)
        return bufferNum;

    // initialize the header of the block passing a struct HeadInfo with values
    // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0
    // to the setHeader() function.
    struct HeadInfo header;
    header.lblock = header.pblock = header.rblock = -1;
    header.numAttrs = header.numEntries = header.numSlots = 0;
    BlockBuffer::setHeader(&header);

    // update the block type of the block to the input block type
    setBlockType(blockType);
    return blockNum;
}

void BlockBuffer::releaseBlock() {
    int blockNum = this->blockNum;
    // If it is already released or invalidated, Eat 5 star - do nothing
    if(blockNum == INVALID_BLOCKNUM || StaticBuffer::blockAllocMap[blockNum]==UNUSED_BLK) {
        return;
    }

    int bufferNum = StaticBuffer::getBufferNum(blockNum);
    // if block is present in buffer, mark it free in metainfo (not tableMetaInfo - this is for metadata cache)
    if(bufferNum != E_BLOCKNOTINBUFFER && bufferNum != E_OUTOFBOUND) {
        StaticBuffer::metainfo[bufferNum].free = true;
    }
    
    // mark it as unused block in block allocation map
    // make its block number invalid
    StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK;
    this->blockNum = INVALID_BLOCKNUM;

}